#include "pch.hpp"
#include <fstream>

#include "shouhin.hpp"

std::vector<std::pair<short, shouhin>> shouhin_tachi{};

bool parse_store()
{
    std::ifstream file("resources/store.txt");
    if (!file) return false;
    shouhin_tachi.clear();
    for (std::string line; std::getline(file, line); )
    {
        if (line.empty() || line.starts_with("#")) continue; // @note '#' initiates a comment
        std::vector<std::string> pipes = readch(line, '|');
        if (pipes.size() < 9) { fprintf(stderr, "invalid store line: %s\n", line.c_str()); continue; }
        ::shouhin shouhin{
            .btn = pipes[1],
            .name = pipes[2],
            .rttx = pipes[3],
            .description = pipes[4],
            .tex1 = pipes[5].front(),
            .tex2 = pipes[6].front(), // @todo
            .cost = stoi(pipes[7])
        };
        std::vector<std::string> tachi = readch(pipes[8], ',');
        for (std::string &item : tachi)
        {
            std::vector<std::string> co = readch(item, ':');
            if (co.size() != 2) continue;
            try { shouhin.items.emplace_back(stoi(co[0]), stoi(co[1])); } catch (...) { continue; }
        }
        try { shouhin_tachi.emplace_back(stoi(pipes[0]), shouhin); } catch (...) { continue; }
    }
    return true;
}