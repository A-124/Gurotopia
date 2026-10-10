#include "pch.hpp"
#include <cstdlib>
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

        // @note a damaged server_data.php must not crash the server with an out of range index
        if (pipes.size() < 14) std::fprintf(stderr, "[server_data.php] incomplete, using defaults. delete the file to regenerate it.\n");
        else try
        {
            this->server = pipes[1];
            this->port = static_cast<u_short>(std::stoi(pipes[3]));
            this->type = static_cast<u_char>(std::stoi(pipes[5]));
            this->type2 = static_cast<u_char>(std::stoi(pipes[7]));
            this->maint = pipes[9];
            this->loginurl = pipes[11];
            this->meta = pipes[13];
        }
        catch (const std::exception &) { std::fprintf(stderr, "[server_data.php] unreadable number, using defaults.\n"); }
        // @note pipes[] is flat: RTENDMARKERBS1001 lands at 14, optional maintenance|0/1 at 15-16.
        if (pipes.size() > 16 && pipes[15] == "maintenance")
            this->maintenance = pipes[16] == "1";
    } // @note delete str, pipes

    // @note deployment: the address players connect to and the game port come from the environment when set
    if (const char *v = std::getenv("GURO_SERVER_ADDR"); v && *v) this->server = v;
    if (const char *v = std::getenv("GURO_PORT"); v && *v)
    {
        const long port = std::strtol(v, nullptr, 10);
        if (port > 0 && port < 65536) this->port = static_cast<u_short>(port);
    }
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
