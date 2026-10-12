#include "pch.hpp"
#include "integration/discord_webhook.hpp"

#include <curl/curl.h>

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <mutex>
#include <string>
#include <string_view>

namespace discord_webhook
{
namespace
{
    constexpr std::size_t max_queue_size = 32;
    constexpr auto request_timeout = std::chrono::seconds(5);
    constexpr auto send_interval = std::chrono::milliseconds(2100);

    // Paste your Discord webhook URL between the quotes below.
    // Treat the URL like a password; do not publish it or commit a real URL to a public repo.
    constexpr std::string_view configured_webhook_url = "PASTE_YOUR_DISCORD_WEBHOOK_URL_HERE";

    std::mutex queue_mutex;
    std::condition_variable queue_changed;
    std::deque<std::string> queue;
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
                case '"': out += "\\\""; break;
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

    size_t discard_response(char*, size_t size, size_t count, void*)
    {
        return size * count;
    }

    bool post_message(const std::string& message)
    {
        CURL* curl = curl_easy_init();
        if (!curl) return false;

        const std::string body = "{\"content\":\"" + json_escape(message) +
            "\",\"allowed_mentions\":{\"parse\":[]}}";
        curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");

        curl_easy_setopt(curl, CURLOPT_URL, webhook_url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discard_response);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(request_timeout.count()));
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
        // Keep libcurl's default peer and hostname verification enabled.
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

    void sender_loop()
    {
        auto last_sent = std::chrono::steady_clock::time_point{};
        for (;;)
        {
            std::string message;
            {
                std::unique_lock lock(queue_mutex);
                queue_changed.wait(lock, [] { return stopping || !queue.empty(); });
                if (queue.empty() && stopping) break;
                message = std::move(queue.front());
                queue.pop_front();
            }

            if (last_sent.time_since_epoch().count() != 0)
            {
                const auto ready_at = last_sent + send_interval;
                std::this_thread::sleep_until(ready_at);
            }
            post_message(message);
            last_sent = std::chrono::steady_clock::now();
        }
    }
}

void start()
{
    if (configured_webhook_url.empty() ||
        configured_webhook_url == "PASTE_YOUR_DISCORD_WEBHOOK_URL_HERE")
    {
        std::puts("[discord] webhook disabled (set configured_webhook_url in discord_webhook.cpp)");
        return;
    }

    webhook_url = configured_webhook_url;
    if (!webhook_url.starts_with("https://"))
    {
        std::fprintf(stderr, "[discord] webhook URL must use HTTPS; integration disabled\n");
        webhook_url.clear();
        return;
    }

    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
    {
        std::fprintf(stderr, "[discord] libcurl initialization failed; integration disabled\n");
        webhook_url.clear();
        return;
    }

    {
        std::lock_guard lock(queue_mutex);
        stopping = false;
        running = true;
    }
    worker = std::thread(sender_loop);
    std::puts("[discord] webhook integration enabled");
}

void notify(std::string_view message)
{
    if (message.empty()) return;
    {
        std::lock_guard lock(queue_mutex);
        if (!running || stopping) return;
        if (queue.size() >= max_queue_size)
        {
            std::fprintf(stderr, "[discord] notification queue full; dropping message\n");
            return;
        }
        queue.emplace_back(message);
    }
    queue_changed.notify_one();
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
}
