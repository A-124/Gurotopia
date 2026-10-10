#include "pch.hpp"
#include <cstdlib>
#include <fstream>

#include "database_config.hpp"

::database_config gDb_config{};

void ::database_config::init()
{
    std::ifstream istrm("mysql_login.txt");
    if (!istrm.is_open())
    {
        std::ofstream ostrm("mysql_login.txt");
        ostrm << 
            std::format(
                "host|{}\n"
                "user|{}\n"
                "password|{}",
                this->host, this->user, this->passwd
            );
    } // @note close ostrm
    else
    {
        for (std::string line; std::getline(istrm, line); ) 
        {
            ::hPipe hPipe{ line };

            if (!hPipe["host"].empty()) this->host = hPipe["host"];
            else if (!hPipe["user"].empty()) this->user = hPipe["user"];
            else if (!hPipe["password"].empty()) this->passwd = hPipe["password"];
        }
    } // @note delete pipes

    // @note deployment: environment variables win over the file, so Docker / systemd never need a password on disk
    if (const char *v = std::getenv("GURO_DB_HOST"); v && *v) this->host = v;
    if (const char *v = std::getenv("GURO_DB_USER"); v && *v) this->user = v;
    if (const char *v = std::getenv("GURO_DB_PASSWORD")) this->passwd = v;
    if (const char *v = std::getenv("GURO_DB_PORT"); v && *v)
    {
        const long port = std::strtol(v, nullptr, 10);
        if (port > 0 && port < 65536) this->port = static_cast<unsigned>(port);
    }
}
