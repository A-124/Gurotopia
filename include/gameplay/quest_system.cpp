#include "pch.hpp"
#include <fstream>
#include <sstream>
#include "gameplay/quest_system.hpp"
namespace quest_system {
namespace { std::unordered_map<int,quest> quests; }
bool reload(){
 std::ifstream f("resources/quests.txt"); if(!f)return false;
 std::unordered_map<int,quest> next; std::string line;
 while(std::getline(f,line)){if(line.empty()||line[0]=='#')continue;std::stringstream ss(line);std::string a,b,c,d,e;if(!std::getline(ss,a,'|')||!std::getline(ss,b,'|')||!std::getline(ss,c,'|')||!std::getline(ss,d,'|')||!std::getline(ss,e,'|'))continue;try{quest q;q.id=std::stoi(a);q.name=b;if(c=="item_changed")q.trigger=condition::item_changed;else if(c=="block_changed")q.trigger=condition::block_changed;else if(c=="player_entered_world")q.trigger=condition::player_entered_world;else continue;q.target=std::stoi(d);q.required=std::max(1,std::stoi(e));next[q.id]=std::move(q);}catch(...){continue;}}
 quests.swap(next); return true;
}
const quest* find(int id) noexcept{auto i=quests.find(id);return i==quests.end()?nullptr:&i->second;}
void on_event(const event_bus::event& event){for(const auto& [id,q]:quests){if((q.trigger==condition::item_changed&&event.kind==event_bus::type::item_changed)||(q.trigger==condition::block_changed&&event.kind==event_bus::type::block_changed)||(q.trigger==condition::player_entered_world&&event.kind==event_bus::type::player_entered_world)){/* Progress persistence/rewards land in Phase 3. */ (void)q;}}}
const std::unordered_map<int,quest>& all() noexcept{return quests;}
}
