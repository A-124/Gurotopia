#include "pch.hpp"
#include "runtime_reload.hpp"
#include "database/items.hpp"
#include "database/shouhin.hpp"
#include "automate/holiday.hpp"
#include "database/custom_content.hpp"
#include "gameplay/quest_system.hpp"
#include "gameplay/achievement_system.hpp"
#include "gameplay/daily_system.hpp"
namespace runtime_reload {
result reload(std::string_view target) {
 if (target == "items") return decode_items() ? result::ok : result::failed;
 if (target == "content") { if (items.empty()) return result::failed; if (!custom_content::reload()) return result::failed; if (!rebuild_custom_items()) return result::failed; return (quest_system::reload() && achievement_system::reload() && daily_system::reload()) ? result::ok : result::failed; }
    if (target == "store") return parse_store() ? result::ok : result::failed;
 if (target == "holiday") { check_for_holiday(); return result::ok; }
 if (target == "all") { if (!decode_items()) return result::failed; if (!parse_store()) return result::failed; if (!custom_content::reload()) return result::failed; if (!rebuild_custom_items()) return result::failed; if (!quest_system::reload()) return result::failed; if (!achievement_system::reload()) return result::failed; if (!daily_system::reload()) return result::failed; check_for_holiday(); return result::ok; }
 return result::unknown_target;
}
std::string_view result_message(result value) { switch(value) { case result::ok:return "reload completed"; case result::unknown_target:return "unknown reload target"; case result::failed:return "reload failed"; } return "reload failed"; }
}
