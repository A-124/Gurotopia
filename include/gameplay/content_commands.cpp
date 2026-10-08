#include "pch.hpp"
#include "content_commands.hpp"
#include "database/custom_content.hpp"

#include "gameplay/quest_system.hpp"
#include "gameplay/achievement_system.hpp"

void content_status(ENetEvent& event,const std::string_view){
 send_varlist(event.peer,{"OnConsoleMessage",std::format("[CONTENT] custom_items={} recipes={} quests={} achievements={}",custom_content::items().size(),custom_content::recipe_count(),quest_system::all().size(),achievement_system::all().size())});
}
