#include "pch.hpp"
#include "content_commands.hpp"
#include "custom_content.hpp"
#include "quest_system.hpp"
#include "achievement_system.hpp"
#include "content_progress.hpp"
void content_status(ENetEvent& event,const std::string_view){
 send_varlist(event.peer,{"OnConsoleMessage",std::format("[CONTENT] custom_items={} recipes={} quests={} achievements={}",custom_content::items().size(),custom_content::find_recipe(0)?1:0,quest_system::all().size(),achievement_system::all().size())});
}
