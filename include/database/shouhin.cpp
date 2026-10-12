#include "pch.hpp"
#include <cstdio>
#include <fstream>
#include <sys/stat.h>

#include "shouhin.hpp"

std::vector<std::pair<short, shouhin>> shouhin_tachi{};

namespace {
std::time_t g_store_last_reload = 0;
std::time_t g_store_last_mtime = 0;

std::time_t file_mtime(const char *path)
{
    struct stat st{};
    if (stat(path, &st) != 0) return 0;
    return st.st_mtime;
}
}

bool parse_store()
{
    const char *path = "resources/store.txt";
    std::ifstream file(path);
    if (!file)
    {
        std::fprintf(stderr, "[store] could not open %s\n", path);
        return false;
    }

    // Build a complete replacement catalog first so concurrent purchases never see a half-cleared list.
    std::vector<std::pair<short, shouhin>> next;
    next.reserve(shouhin_tachi.size() + 8);

    int line_no = 0;
    int skipped = 0;
    for (std::string line; std::getline(file, line); )
    {
        ++line_no;
        if (line.empty() || line.starts_with("#")) continue;
        std::vector<std::string> pipes = readch(line, '|');
        if (pipes.size() < 9)
        {
            std::fprintf(stderr, "[store] line %d: expected >=9 fields, got %zu — skipped\n", line_no, pipes.size());
            ++skipped;
            continue;
        }
        if (pipes[5].empty() || pipes[6].empty())
        {
            std::fprintf(stderr, "[store] line %d: missing texture fields — skipped\n", line_no);
            ++skipped;
            continue;
        }

        ::shouhin product{
            .btn = pipes[1],
            .name = pipes[2],
            .rttx = pipes[3],
            .description = pipes[4],
            .tex1 = pipes[5].front(),
            .tex2 = pipes[6].front(),
            .cost = 0
        };
        try { product.cost = std::stoi(pipes[7]); }
        catch (...)
        {
            std::fprintf(stderr, "[store] line %d: invalid cost '%s' — skipped\n", line_no, pipes[7].c_str());
            ++skipped;
            continue;
        }

        std::vector<std::string> tachi = readch(pipes[8], ',');
        for (std::string &item : tachi)
        {
            std::vector<std::string> co = readch(item, ':');
            if (co.size() != 2) continue;
            try { product.items.emplace_back(static_cast<short>(std::stoi(co[0])), static_cast<short>(std::stoi(co[1]))); }
            catch (...) { continue; }
        }
        if (product.items.empty())
        {
            std::fprintf(stderr, "[store] line %d: no valid items — skipped\n", line_no);
            ++skipped;
            continue;
        }

        short tab = 0;
        try { tab = static_cast<short>(std::stoi(pipes[0])); }
        catch (...)
        {
            std::fprintf(stderr, "[store] line %d: invalid tab '%s' — skipped\n", line_no, pipes[0].c_str());
            ++skipped;
            continue;
        }
        next.emplace_back(tab, std::move(product));
    }

    if (next.empty())
    {
        std::fprintf(stderr, "[store] no valid products in %s — keeping previous catalog (%zu products)\n",
            path, shouhin_tachi.size());
        return false;
    }

    shouhin_tachi = std::move(next);
    g_store_last_reload = std::time(nullptr);
    g_store_last_mtime = file_mtime(path);
    if (skipped > 0)
        std::printf("[store] loaded %zu product(s) (%d line(s) skipped)\n", shouhin_tachi.size(), skipped);
    else
        std::printf("[store] loaded %zu product(s)\n", shouhin_tachi.size());
    return true;
}

std::size_t store_product_count() noexcept
{
    return shouhin_tachi.size();
}

std::time_t store_last_reload() noexcept
{
    return g_store_last_reload;
}

void store_auto_reload_tick()
{
    const char *path = "resources/store.txt";
    const std::time_t mtime = file_mtime(path);
    if (mtime == 0) return;
    // First successful load sets g_store_last_mtime; only reload when the file is newer.
    if (g_store_last_mtime != 0 && mtime <= g_store_last_mtime) return;
    if (g_store_last_mtime == 0 && g_store_last_reload != 0)
    {
        // Catalog was loaded but mtime was not recorded (older binary); seed and skip this tick.
        g_store_last_mtime = mtime;
        return;
    }
    if (parse_store())
        std::printf("[store] auto-reloaded from disk (file changed)\n");
}
