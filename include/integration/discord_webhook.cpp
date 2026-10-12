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

namespace discord_webhook {
namespace {
constexpr std::size_t max_queue_size = 32;
constexpr auto request_timeout = std::chrono::seconds(5);
constexpr auto send_interval = std::chrono::milliseconds(2100);
constexpr auto status_update_interval = std::chrono::seconds(10);
constexpr std::string_view configured_webhook_url = "PASTE_YOUR_DISCORD_WEBHOOK_URL_HERE";
std::mutex queue_mutex;
std::condition_variable queue_changed;
std::deque<std::string> queue;
std::thread worker, status_worker;
std::string webhook_url;
bool running = false, stopping = false;
std::chrono::steady_clock::time_point server_started_at;

std::string json_escape(std::string_view value) {
    std::string out;
    for (unsigned char c : value) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) {
                constexpr char hex[] = "0123456789abcdef";
                out += "\\u00"; out += hex[(c >> 4) & 15]; out += hex[c & 15];
            } else out += static_cast<char>(c);
        }
    }
    return out;
}
size_t write_response(char* data, size_t size, size_t count, void* userdata) {
    const auto bytes = size * count;
    if (userdata) static_cast<std::string*>(userdata)->append(data, bytes);
    return bytes;
}
bool request(const std::string& url, const std::string& body, bool patch, std::string* response = nullptr) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;
    curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    if (patch) curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
    else curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response);
    if (response) curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(request_timeout.count()));
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    const CURLcode result = curl_easy_perform(curl);
    long status = 0;
    if (result == CURLE_OK) curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_slist_free_all(headers); curl_easy_cleanup(curl);
    if (result != CURLE_OK || status < 200 || status >= 300) {
        std::fprintf(stderr, "[discord] webhook request failed (curl=%d, HTTP=%ld)\n", (int)result, status);
        return false;
    }
    return true;
}
bool post_message(const std::string& message) {
    return request(webhook_url, "{\"content\":\"" + json_escape(message) +
        "\",\"allowed_mentions\":{\"parse\":[]}}", false);
}
std::string uptime() {
    const auto t = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - server_started_at).count();
    char b[96]{};
    std::snprintf(b, sizeof(b), "%lldd %02lldh %02lldm %02llds",
        (long long)(t / 86400), (long long)((t % 86400) / 3600),
        (long long)((t % 3600) / 60), (long long)(t % 60));
    return b;
}
std::string embed_json() {
    return "{\"embeds\":[{\"title\":\"Gurotopia Server Status\","
        "\"description\":\"Server is online\",\"color\":5763719,"
        "\"fields\":[{\"name\":\"Uptime\",\"value\":\"" + std::string(1, 96) + uptime() +
        std::string(1, 96) + "\",\"inline\":true}],"
        "\"footer\":{\"text\":\"Gurotopia | Live server status\"}}],"
        "\"allowed_mentions\":{\"parse\":[]}}";
}
std::string response_message_id(const std::string& json) {
    const auto key = json.find("\"id\"");
    if (key == std::string::npos) return {};
    const auto colon = json.find(':', key + 4);
    const auto first = colon == std::string::npos ? colon : json.find('"', colon + 1);
    const auto last = first == std::string::npos ? first : json.find('"', first + 1);
    if (last == std::string::npos) return {};
    auto id = json.substr(first + 1, last - first - 1);
    if (id.empty() || id.find_first_not_of("0123456789") != std::string::npos) return {};
    return id;
}
void status_loop() {
    std::string response;
    const std::string create_url = webhook_url +
        (webhook_url.find('?') == std::string::npos ? "?wait=true" : "&wait=true");
    if (!request(create_url, embed_json(), false, &response)) {
        std::fprintf(stderr, "[discord] could not create persistent status message\n"); return;
    }
    const auto id = response_message_id(response);
    if (id.empty()) { std::fprintf(stderr, "[discord] Discord returned no status message ID\n"); return; }
    const auto edit_url = webhook_url + "/messages/" + id;
    std::puts("[discord] persistent status message created; uptime refreshes every 10 seconds");
    for (;;) {
        {
            std::unique_lock lock(queue_mutex);
            if (queue_changed.wait_for(lock, status_update_interval, [] { return stopping; })) break;
        }
        if (!request(edit_url, embed_json(), true))
            std::fprintf(stderr, "[discord] status refresh failed; retrying in 10 seconds\n");
    }
}
void sender_loop() {
    auto last_sent = std::chrono::steady_clock::time_point{};
    for (;;) {
        std::string message;
        {
            std::unique_lock lock(queue_mutex);
            queue_changed.wait(lock, [] { return stopping || !queue.empty(); });
            if (queue.empty() && stopping) break;
            message = std::move(queue.front()); queue.pop_front();
        }
        if (last_sent.time_since_epoch().count() != 0) std::this_thread::sleep_until(last_sent + send_interval);
        post_message(message); last_sent = std::chrono::steady_clock::now();
    }
}
}
void start() {
    if (configured_webhook_url.empty() || configured_webhook_url == "PASTE_YOUR_DISCORD_WEBHOOK_URL_HERE") {
        std::puts("[discord] webhook disabled (set configured_webhook_url in discord_webhook.cpp)"); return;
    }
    webhook_url = configured_webhook_url;
    if (!webhook_url.starts_with("https://")) {
        std::fprintf(stderr, "[discord] webhook URL must use HTTPS; integration disabled\n"); webhook_url.clear(); return;
    }
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        std::fprintf(stderr, "[discord] libcurl initialization failed; integration disabled\n"); webhook_url.clear(); return;
    }
    {
        std::lock_guard lock(queue_mutex);
        stopping = false; running = true; server_started_at = std::chrono::steady_clock::now();
    }
    worker = std::thread(sender_loop);
    status_worker = std::thread(status_loop);
    std::puts("[discord] webhook integration enabled");
}
void notify(std::string_view message) {
    if (message.empty()) return;
    {
        std::lock_guard lock(queue_mutex);
        if (!running || stopping) return;
        if (queue.size() >= max_queue_size) { std::fprintf(stderr, "[discord] notification queue full; dropping message\n"); return; }
        queue.emplace_back(message);
    }
    queue_changed.notify_one();
}
void stop() {
    {
        std::lock_guard lock(queue_mutex);
        if (!running) return;
        stopping = true;
    }
    queue_changed.notify_all();
    if (status_worker.joinable()) status_worker.join();
    if (worker.joinable()) worker.join();
    {
        std::lock_guard lock(queue_mutex);
        running = false; queue.clear();
    }
    curl_global_cleanup(); webhook_url.clear();
}
}
