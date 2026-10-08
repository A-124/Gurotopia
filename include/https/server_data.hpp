#pragma once

class server_data 
{
public:
    std::string server{"127.0.0.1"};
    u_short port{17091};
    u_char type{1};
    u_char type2{1};
    std::string maint{"Server under maintenance. Please try again later."};
    std::string loginurl{"login-gurotopia.vercel.app"};
    std::string meta{"gurotopia"};
    bool maintenance{}; // @note blocks non-staff logins; toggled from admin panel

    void init();
    void save_maintenance();
};
extern ::server_data gServer_data;
