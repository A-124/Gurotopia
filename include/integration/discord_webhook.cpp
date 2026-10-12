#include "pch.hpp"
#include "integration/discord_webhook.hpp"

#include <curl/curl.h>

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

namespace discord_webhook
{
namespace
{
    constexpr std::size_t max_queue_size = 64;
    constexpr std::size_t max_content_len = 1900;
    constexpr auto request_timeout = std::chrono::seconds(8);
    constexpr auto send_interval = std::chrono::milliseconds(2200);
    constexpr const char *k_url_file = "resources/discord_webhook.url";
    constexpr const char *k_bot_name = "Gurotopia";

    struct payload
    {
        bool embed = false;
        std::string content;
        std::string title;
        std::string description;
        int color = 0x5865F2;
    };

    std::mutex queue_mutex;
    std::condition_variable queue_changed;
    std::deque<payload> queue;
    std::thread worker;
    std::string webhook_url;
    bool running = false;
    bool stopping = false;

    std::string json_escape(std::string_view value)
    {
        std::string out;
        out.reserve(value.size() + 16);
        for (unsigned char c : value)
        {
            switch (c)
            {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\b': out += "\\b"; break;
                case '\f': out += "\\f"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default:
                    if (c < 0x20)
                    {
                        constexpr char hex[] = "0123456789abcdef";
                        out += "\\u00";
                        out += hex[(c >> 4) & 0x0f];
                        out += hex[c & 0x0f];
                    }
                    else out += static_cast<char>(c);
            }
        }
        return out;
    }

    std::string truncate(std::string_view value, std::size_t max_len)
    {
        if (value.size() <= max_len) return std::string{value};
        std::string out{value.substr(0, max_len - 1)};
        out += "\xe2\x80\xa6";
        return out;
    }

    std::string resolve_url()
    {
        if (const char *env = std::getenv("GURO_DISCORD_WEBHOOK_URL"); env && *env)
            return std::string{env};

        std::ifstream file(k_url_file);
        if (!file) return {};
        std::string line;
        if (!std::getline(file, line)) return {};
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t'))
            line.pop_back();
        std::size_t start = 0;
        while (start < line.size() && (line[start] == ' ' || line[start] == '\t')) ++start;
        if (start) line.erase(0, start);
        if (line.empty() || line[0] == '#') return {};
        return line;
    }

    size_t discard_response(char *, size_t size, size_t count, void *)
    {
        return size * count;
    }

    std::string build_json(const payload &p)
    {
        std::string body = "{";
        body += "\"username\":\"";
        body += json_escape(k_bot_name);
        body += "\",";
        body += "\"allowed_mentions\":{\"parse\":[]},";
        if (p.embed)
        {
            body += "\"embeds\":[{";
            body += "\"title\":\"";
            body += json_escape(truncate(p.title, 256));
            body += "\",\"description\":\"";
            body += json_escape(truncate(p.description, max_content_len));
            body += "\",\"color\":";
            body += std::to_string(p.color & 0xffffff);
            body += "}]";
        }
        else
        {
            body += "\"content\":\"";
            body += json_escape(truncate(p.content, max_content_len));
            body += "\"";
        }
        body += "}";
        return body;
    }

    bool post_once(const std::string &body)
    {
        CURL *curl = curl_easy_init();
        if (!curl) return false;

        curl_slist *headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");

        curl_easy_setopt(curl, CURLOPT_URL, webhook_url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discard_response);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 4L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(request_timeout.count()));
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

        const CURLcode result = curl_easy_perform(curl);
        long status = 0;
        if (result == CURLE_OK)
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (result != CURLE_OK || status < 200 || status >= 300)
        {
            std::fprintf(stderr, "[discord] webhook delivery failed (curl=%d, HTTP=%ld)\n",
                static_cast<int>(result), status);
            return false;
        }
        return true;
    }

    void post_message(const payload &p)
    {
        const std::string body = build_json(p);
        if (post_once(body)) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
        post_once(body);
    }

    void sender_loop()
    {
        auto last_sent = std::chrono::steady_clock::time_point{};
        for (;;)
        {
            payload item;
            {
                std::unique_lock lock(queue_mutex);
                queue_changed.wait(lock, [] { return stopping || !queue.empty(); });
                if (queue.empty() && stopping) break;
                item = std::move(queue.front());
                queue.pop_front();
            }

            if (last_sent.time_since_epoch().count() != 0)
            {
                const auto ready_at = last_sent + send_interval;
                std::this_thread::sleep_until(ready_at);
            }
            post_message(item);
            last_sent = std::chrono::steady_clock::now();
        }
    }

    void enqueue(payload item)
    {
        {
            std::lock_guard lock(queue_mutex);
            if (!running || stopping) return;
            if (queue.size() >= max_queue_size)
            {
                std::fprintf(stderr, "[discord] notification queue full; dropping message\n");
                return;
            }
            queue.push_back(std::move(item));
        }
        queue_changed.notify_one();
    }
}

void start()
{
    webhook_url = resolve_url();
    if (webhook_url.empty())
    {
        std::puts("[discord] webhook disabled — set GURO_DISCORD_WEBHOOK_URL or resources/discord_webhook.url");
        return;
    }
    if (!webhook_url.starts_with("https://discord.com/api/webhooks/") &&
        !webhook_url.starts_with("https://discordapp.com/api/webhooks/"))
    {
        std::fprintf(stderr, "[discord] URL must be an https://discord.com/api/webhooks/... link; disabled\n");
        webhook_url.clear();
        return;
    }

    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
    {
        std::fprintf(stderr, "[discord] libcurl init failed; disabled\n");
        webhook_url.clear();
        return;
    }

    {
        std::lock_guard lock(queue_mutex);
        stopping = false;
        running = true;
        queue.clear();
    }
    worker = std::thread(sender_loop);
    std::puts("[discord] webhook integration enabled");

    notify_embed("Server online", "\xf0\x9f\x9f\xa2 **Gurotopia** is up and accepting players.", 0x57F287);
}

void stop()
{
    {
        std::lock_guard lock(queue_mutex);
        if (!running) return;
        stopping = true;
    }
    queue_changed.notify_all();
    if (worker.joinable()) worker.join();
    {
        std::lock_guard lock(queue_mutex);
        running = false;
        queue.clear();
    }
    curl_global_cleanup();
    webhook_url.clear();
}

void notify(std::string_view message)
{
    if (message.empty()) return;
    payload p;
    p.embed = false;
    p.content = std::string{message};
    enqueue(std::move(p));
}

void notify_embed(std::string_view title, std::string_view description, int color)
{
    if (title.empty() && description.empty()) return;
    payload p;
    p.embed = true;
    p.title = std::string{title};
    p.description = std::string{description};
    p.color = color;
    enqueue(std::move(p));
}

bool enabled() noexcept
{
    std::lock_guard lock(queue_mutex);
    return running;
}
}
