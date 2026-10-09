#include "pch.hpp"
#include "https.hpp"
#include "server_data.hpp"
#include "database/items.hpp"
#include <filesystem>
#include <fstream>

#include <openssl/err.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <csignal>
    #include <unistd.h>
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h> // @note TCP_DEFER_ACCEPT
    #include <sys/socket.h>

    #define SOCKET int
    #define INVALID_SOCKET	(SOCKET)(~0)
    #define SOCKET_ERROR	(-1)

#endif

/* cross-platform socket close */
static void cross_close(SOCKET fd)
{
    int ret =
#ifdef _WIN32
    closesocket(fd)
#else // @note unix
    close(fd)
#endif
    ; // ending of ret. hehe some silly code ;)

    if (ret == SOCKET_ERROR) printf("socket close error.\n");
}

/* cross-platform WSA error log, fallback on linux with strerror() */
static void cross_log(const std::string &message)
{
#ifdef _WIN32
    std::fprintf(stderr, "%s: %d\n", message.c_str(), WSAGetLastError());
#else // @note unix
    std::fprintf(stderr, "%s: %s\n", message.c_str(), strerror(errno));
#endif
}

static bool write_all(SSL *ssl, const void *data, std::size_t size)
{
    const auto *bytes = static_cast<const unsigned char*>(data);
    while (size > 0)
    {
        const int chunk = static_cast<int>(std::min<std::size_t>(size, 16 * 1024));
        const int written = SSL_write(ssl, bytes, chunk);
        if (written <= 0) return false;
        bytes += written;
        size -= static_cast<std::size_t>(written);
    }
    return true;
}

static std::string request_path(const char *request)
{
    const std::string_view line(request);
    const std::size_t begin = line.find(' ');
    if (begin == std::string_view::npos) return {};
    const std::size_t end = line.find(' ', begin + 1);
    if (end == std::string_view::npos) return {};
    return std::string(line.substr(begin + 1, end - begin - 1));
}

static bool proxy_official_asset(SSL *client_ssl, const std::string &path)
{
    constexpr char host[] = "ubistatic-a.akamaihd.net";
    constexpr char prefix[] = "/0098/024920264/cache";
    if (!path.starts_with("/assets/")) return false;

    const std::string upstream_path =
        std::format("{}{}", prefix, path.substr(std::string_view("/assets").size()));

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo *results = nullptr;
    if (getaddrinfo(host, "443", &hints, &results) != 0) return false;

    SOCKET fd = INVALID_SOCKET;
    for (addrinfo *it = results; it; it = it->ai_next)
    {
        fd = static_cast<SOCKET>(::socket(it->ai_family, it->ai_socktype, it->ai_protocol));
        if (fd == INVALID_SOCKET) continue;
        if (::connect(fd, it->ai_addr, static_cast<int>(it->ai_addrlen)) == 0) break;
        cross_close(fd);
        fd = INVALID_SOCKET;
    }
    freeaddrinfo(results);
    if (fd == INVALID_SOCKET) return false;

    SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) { cross_close(fd); return false; }
    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, nullptr);

    SSL *upstream = SSL_new(ctx);
    if (!upstream)
    {
        SSL_CTX_free(ctx);
        cross_close(fd);
        return false;
    }

    SSL_set_tlsext_host_name(upstream, host);
    SSL_set_fd(upstream, fd);

    bool ok = SSL_connect(upstream) == 1;
    std::vector<char> response;
    if (ok)
    {
        const std::string request = std::format(
            "GET {} HTTP/1.1\r\n"
            "Host: {}\r\n"
            "User-Agent: Growtopia\r\n"
            "Accept-Encoding: identity\r\n"
            "Connection: close\r\n\r\n",
            upstream_path, host);

        ok = write_all(upstream, request.data(), request.size());

        std::array<char, 32 * 1024> buffer{};
        while (ok)
        {
            const int n = SSL_read(upstream, buffer.data(), static_cast<int>(buffer.size()));
            if (n <= 0) break;
            response.insert(response.end(), buffer.data(), buffer.data() + n);
        }
    }

    SSL_shutdown(upstream);
    SSL_free(upstream);
    SSL_CTX_free(ctx);
    shutdown(fd, 2);
    cross_close(fd);

    if (!ok || response.empty()) return false;

    const auto header_end_it = std::search(
        response.begin(), response.end(),
        "\r\n\r\n", "\r\n\r\n" + 4);
    if (header_end_it == response.end()) return false;

    const std::size_t header_size =
        static_cast<std::size_t>(header_end_it - response.begin()) + 4;
    const std::string headers(response.data(), header_size);
    const auto first_line_end = headers.find("\r\n");
    const std::string status_line =
        first_line_end == std::string::npos ? std::string{} : headers.substr(0, first_line_end);

    if (!status_line.starts_with("HTTP/1.1 200") && !status_line.starts_with("HTTP/2 200"))
    {
        std::printf("[https] CDN returned: %s for %s\n", status_line.c_str(), path.c_str());
        return false;
    }

    std::vector<char> body(response.begin() + header_size, response.end());

    // Never forward chunked framing to the Growtopia asset downloader.
    // The client expects a normal HTTP body with a Content-Length.
    const std::string lower_headers = [&] {
        std::string v = headers;
        std::transform(v.begin(), v.end(), v.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return v;
    }();

    if (lower_headers.find("transfer-encoding: chunked") != std::string::npos)
    {
        std::vector<char> decoded;
        std::size_t p = 0;
        while (p < body.size())
        {
            const auto e = std::search(body.begin() + p, body.end(), "\r\n", "\r\n" + 2);
            if (e == body.end()) return false;
            const std::string size_text(body.begin() + p, e);
            std::size_t chunk_size = 0;
            try { chunk_size = std::stoull(size_text, nullptr, 16); }
            catch (...) { return false; }
            p = static_cast<std::size_t>(e - body.begin()) + 2;
            if (chunk_size == 0) break;
            if (p + chunk_size + 2 > body.size()) return false;
            decoded.insert(decoded.end(), body.begin() + p, body.begin() + p + chunk_size);
            p += chunk_size + 2;
        }
        body.swap(decoded);
    }

    std::printf("[https] CDN proxy: %s (%zu bytes)\n", path.c_str(), body.size());

    const std::string out_header = std::format(
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/octet-stream\r\n"
        "Content-Length: {}\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: close\r\n\r\n",
        body.size());

    return write_all(client_ssl, out_header.data(), out_header.size()) &&
           (body.empty() || write_all(client_ssl, body.data(), body.size()));
}

static bool serve_asset(SSL *ssl, const std::string &path)
{
    if (path == "/assets/items.dat" || path.ends_with("/items.dat"))
    {
        constexpr std::size_t header_size = sizeof(::gamePacket);
        if (im_data.size() <= header_size) return false;
        const auto *body = im_data.data() + header_size;
        std::printf("[https] serving custom items.dat (%zu bytes, hash=%u)\n", im_data.size() - header_size, item_data_hash());
        const std::size_t body_size = im_data.size() - header_size;
        const std::string header = std::format(
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/octet-stream\r\n"
            "Content-Length: {}\r\n"
            "Cache-Control: no-cache\r\n"
            "Connection: close\r\n\r\n",
            body_size);
        return write_all(ssl, header.data(), header.size()) &&
               write_all(ssl, body, body_size);
    }

    const std::filesystem::path requested(path);
    const std::string filename = requested.filename().string();
    if (filename.empty() || filename == "." || filename == "..") return false;

    const auto ext = std::filesystem::path(filename).extension().string();
    if (ext != ".rttex" && ext != ".mp3" && ext != ".ogg" && ext != ".wav" && ext != ".xml") return false;

    // Prefer the complete /assets-relative path so different directories can
    // safely contain assets with the same filename. Keep root-level filenames
    // working for existing custom assets such as Lkat.rttex.
    std::filesystem::path relative_asset;
    constexpr std::string_view assets_prefix = "/assets/";
    if (std::string_view(path).starts_with(assets_prefix))
        relative_asset = std::filesystem::path(path.substr(assets_prefix.size()));
    if (relative_asset.empty() || relative_asset.is_absolute()) return false;
    for (const auto &part : relative_asset)
        if (part == "..") return false;

    const std::filesystem::path custom_root("resources/custom_assets");
    std::filesystem::path file_path = custom_root / relative_asset;
    std::ifstream file(file_path, std::ios::binary | std::ios::ate);
    if (!file && relative_asset != filename)
    {
        // Backward-compatible fallback: existing custom assets live in the root.
        file_path = custom_root / filename;
        file.clear();
        file.open(file_path, std::ios::binary | std::ios::ate);
    }
    if (!file) return false;
    std::printf("[https] serving custom asset: %s\\n", file_path.string().c_str());

    const std::streamsize size = file.tellg();
    if (size < 0) return false;
    file.seekg(0, std::ios::beg);

    const std::string header = std::format(
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/octet-stream\r\n"
        "Content-Length: {}\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: close\r\n\r\n",
        size);
    if (!write_all(ssl, header.data(), header.size())) return false;

    std::array<char, 16 * 1024> buffer{};
    while (file)
    {
        file.read(buffer.data(), buffer.size());
        const std::streamsize read = file.gcount();
        if (read > 0 && !write_all(ssl, buffer.data(), static_cast<std::size_t>(read)))
            return false;
    }
    return true;
}


void https::listener()
{
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    constexpr int enable = 1;

    /* https://docs.openssl.org/3.0/man3/SSL_CTX_new/#return-values */
    SSL_CTX *ctx = SSL_CTX_new(TLS_server_method());
    if (!ctx)
    {
        ERR_print_errors_fp(stderr);
    }

    /* https://docs.openssl.org/master/man3/SSL_CTX_use_certificate/#return-values */
    if (SSL_CTX_use_certificate_file(ctx, "resources/ctx/server.crt", SSL_FILETYPE_PEM) != 1 ||
        SSL_CTX_use_PrivateKey_file(ctx, "resources/ctx/server.key", SSL_FILETYPE_PEM)  != 1)
    {
        ERR_print_errors_fp(stderr);
    }

#ifdef SIGPIPE // @note unix
    std::signal(SIGPIPE, SIG_IGN);
#endif

    /* https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-socket */
    SOCKET socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket == INVALID_SOCKET)
    {
        cross_log("socket function failed");
    }

#ifdef SO_REUSEADDR
    setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, (char*)&enable, sizeof(enable));
#endif
#ifdef TCP_DEFER_ACCEPT // @note unix
    setsockopt(socket, IPPROTO_TCP, TCP_DEFER_ACCEPT, (char*)&enable, sizeof(enable));
#endif
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(443);
    socklen_t addrlen = sizeof(addr);

    /* https://learn.microsoft.com/en-us/windows/win32/api/winsock/nf-winsock-bind */
    if (bind(socket, (struct sockaddr*)&addr, addrlen) == SOCKET_ERROR)
    {
        cross_log("could not bind socket");
    }

    const std::string Content =
        std::format(
            "server|{}\n"
            "port|{}\n"
            "type|{}\n"
            "type2|{}\n" // @todo remove for older clients
            "#maint|{}\n"
            "loginurl|{}\n"
            "meta|{}\n"
            "RTENDMARKERBS1001", 
            gServer_data.server, gServer_data.port, gServer_data.type, gServer_data.type2, gServer_data.maint, gServer_data.loginurl, gServer_data.meta
        );
    const std::string response =
        std::format(
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: {}\r\n"
            "Connection: close\r\n\r\n"
            "{}",
            Content.size(), Content);

    /* https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-listen */
    if (listen(socket, SOMAXCONN) == SOCKET_ERROR)
    {
        cross_log("failed to listen on socket");
    }
    else std::printf("listening on %s:%hu\n", gServer_data.server.c_str(), gServer_data.port);
    
    while (true)
    {
        /* https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-accept */
        SOCKET fd = accept(socket, reinterpret_cast<sockaddr*>(&addr), &addrlen);
        if (fd == INVALID_SOCKET) continue;
        
        /* https://docs.openssl.org/3.0/man3/SSL_new/#return-values */
        SSL *ssl = SSL_new(ctx);
        if (!ssl) { cross_close(fd); continue; } // @note don't leak the socket on alloc failure
        /* https://docs.openssl.org/3.0/man3/SSL_set_fd/#return-values */
        /* https://docs.openssl.org/3.0/man3/SSL_accept/#return-values */
        else if (SSL_set_fd(ssl, fd) != 1 || SSL_accept(ssl) <= 0) {
            //int ret = SSL_get_error(ssl, 3);
            ERR_print_errors_fp(stderr);
        }
        else {
            char buf[1024]; // @note POST header + body always fits; we only log the first line.
            const int length{ sizeof(buf) - 1 };

            int rbytes = SSL_read(ssl, buf, length);
            if (rbytes <= 0) ERR_print_errors_fp(stderr); // @todo support retryable
            else
            {
                buf[rbytes] = '\0'; // @note MUST null-terminate: buf is NOT a C-string otherwise
                // @note log only the request line — the 36-byte body is binary-ish base64,
                // and %s on the full buffer was reading past the POST into garbage.
                const char *eol = strchr(buf, '\r');
                if (!eol) eol = strchr(buf, '\n');
                const std::size_t line_len = eol ? static_cast<std::size_t>(eol - buf) : static_cast<std::size_t>(rbytes);
                printf("%.*s\n", static_cast<int>(std::min(line_len, static_cast<std::size_t>(128))), buf);

                const std::string path = request_path(buf);
                const bool is_get = std::string_view(buf, static_cast<std::size_t>(rbytes)).starts_with("GET ");
                if (is_get)
                {
                    if (!serve_asset(ssl, path))
                    {
                        // Vanilla assets must be proxied through the server.
                        // The Growtopia downloader does not reliably follow HTTP
                        // redirects for CDN assets, so a 302 makes valid vanilla
                        // textures appear missing/wrong.
                        if (!proxy_official_asset(ssl, path))
                        {
                            const std::string not_found =
                                "HTTP/1.1 404 Not Found\r\n"
                                "Content-Length: 0\r\n"
                                "Connection: close\r\n\r\n";
                            write_all(ssl, not_found.data(), not_found.size());
                        }
                    }
                }
                else
                {
                    const int wbytes = SSL_write(ssl, response.c_str(), static_cast<int>(response.size()));
                    if (wbytes <= 0) ERR_print_errors_fp(stderr); // @todo support retryable
                }
            }
        }

        /* "It can also occur when not all data was read using SSL_read()." */
        if (ssl && SSL_shutdown(ssl) < 0) 
        {
            //int ret = SSL_get_error(ssl, 3);
            ERR_print_errors_fp(stderr);
        }
        SSL_free(ssl);
        if (shutdown(fd, 2) == SOCKET_ERROR) cross_log("failed to shutdown socket"); // @todo unsure if WSA can handle this.
        cross_close(fd);
    }
}

#ifndef _WIN32
    #undef SOCKET
#endif
