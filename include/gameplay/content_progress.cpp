#include "pch.hpp"
#include "content_progress.hpp"
#include "quest_system.hpp"
#include "achievement_system.hpp"
namespace content_progress {
namespace { std::unordered_map<int,player_state> states; }
void clear(){states.clear();}
void on_event(const event_bus::event& event){
 auto* p=static_cast<::peer*>(event.context);
 if(!p)return;
 auto& state=states[reinterpret_cast<std::uintptr_t>(p)];
 for(const auto& [id,q]:quest_system::all()){
  bool matches=(q.trigger==quest_system::condition::item_changed&&event.kind==event_bus::type::item_changed)||(q.trigger==quest_system::condition::block_changed&&event.kind==event_bus::type::block_changed)||(q.trigger==quest_system::condition::player_entered_world&&event.kind==event_bus::type::player_entered_world);
  if(matches && (q.target<0 || event.item_id==q.target)) state.quest_progress[id]=std::min(q.required,state.quest_progress[id]+std::max(1,event.amount));
 }
 for(const auto& [id,a]:achievement_system::all()) if(a.trigger==event.kind && (a.target<0 || event.item_id==a.target)) state.achievements[id]=true;
}
int progress(int quest_id) noexcept{return 0;}
bool completed(int achievement_id) noexcept{return false;}
}
