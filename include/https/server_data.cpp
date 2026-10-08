#include "pch.hpp"
#include <fstream>
#include <sstream>

#include "server_data.hpp"

::server_data gServer_data{};

void ::server_data::init()
{
    std::ifstream file("server_data.php");
    if (!file.is_open())
    {
        std::ofstream ostrm("server_data.php");
        ostrm << 
            std::format(
                "server|{}\n"
                "port|{}\n"
                "type|{}\n"
                "type2|{}\n"
                "#maint|{}\n"
                "loginurl|{}\n"
                "meta|{}\n"
                "RTENDMARKERBS1001", 
                this->server, this->port, this->type, this->type2, this->maint, this->loginurl, this->meta
            );
    } // @note close ostrm
    else
    {
        std::vector<std::string> pipes;
        for (std::string line; std::getline(file, line); ) 
        {
            auto pipe_pair = readch(line, '|');
            pipes.insert(pipes.end(), pipe_pair.begin(), pipe_pair.end());
        }

        this->server = pipes[1];
        this->port = std::stoi(pipes[3]);
        this->type = std::stoi(pipes[5]);
        this->type2 = std::stoi(pipes[7]);
        this->maint = pipes[9];
        this->loginurl = pipes[11];
        this->meta = pipes[13];
        // @note pipes[] is flat: RTENDMARKERBS1001 lands at 14, optional maintenance|0/1 at 15-16.
        if (pipes.size() > 16 && pipes[15] == "maintenance")
            this->maintenance = pipes[16] == "1";
    } // @note delete str, pipes
} // @note close file

void ::server_data::save_maintenance()
{
    // @note file can be missing/locked (AV scan, first boot) — never crash the server over it.
    std::ifstream file("server_data.php");
    if (!file.is_open()) return;
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    if (content.find("maintenance|") == std::string::npos)
    {
        if (!content.empty() && content.back() != '\n') content += '\n';
        content += std::format("maintenance|{}\n", this->maintenance ? 1 : 0);
    }
    else
    {
        std::string out;
        out.reserve(content.size() + 16);
        std::istringstream input(content);
        for (std::string line; std::getline(input, line); )
        {
            if (!line.empty() && line.back() == '\r') line.pop_back(); // @note tolerate CRLF
            if (line.starts_with("maintenance|")) line = std::format("maintenance|{}", this->maintenance ? 1 : 0);
            out += line + '\n';
        }
        content = std::move(out);
    }
    std::ofstream ostrm("server_data.php", std::ios::trunc);
    if (!ostrm.is_open()) return; // @note read-only dir / AV lock: keep running, memory state is already correct
    ostrm << content;
    ostrm.flush();
}
