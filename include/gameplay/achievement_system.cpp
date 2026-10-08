#include "pch.hpp"
#include <fstream>
#include <sstream>
#include "achievement_system.hpp"
namespace achievement_system {
namespace { std::unordered_map<int,achievement> achievements; }
bool reload(){
 std::ifstream f("resources/achievements.txt"); if(!f)return false;
 std::unordered_map<int,achievement> next; std::string line;
 while(std::getline(f,line)){if(line.empty()||line[0]=='#')continue;std::stringstream ss(line);std::string a,b,c,d;if(!std::getline(ss,a,'|')||!std::getline(ss,b,'|')||!std::getline(ss,c,'|')||!std::getline(ss,d,'|'))continue;try{achievement x;x.id=std::stoi(a);x.name=b;x.target=std::stoi(d);if(c=="item_changed")x.trigger=event_bus::type::item_changed;else if(c=="block_changed")x.trigger=event_bus::type::block_changed;else if(c=="player_entered_world")x.trigger=event_bus::type::player_entered_world;else continue;next[x.id]=std::move(x);}catch(...){continue;}}
 achievements.swap(next);return true;
}
const achievement* find(int id) noexcept{auto i=achievements.find(id);return i==achievements.end()?nullptr:&i->second;}
void on_event(const event_bus::event& event){for(const auto& [id,a]:achievements)if(a.trigger==event.kind){(void)a;}}
const std::unordered_map<int,achievement>& all() noexcept{return achievements;}
}
