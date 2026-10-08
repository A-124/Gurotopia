#include "pch.hpp"
#include "tools/create_dialog.hpp"
#include "automate/holiday.hpp"

#include "RequestGazette.hpp"

void on::RequestGazette(ENetEvent& event)
{
    std::tm time = localtime();
    std::vector<std::string> month = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };

    std::string dialog = ::create_dialog()
        .set_default_color("`o")
        .add_label_with_icon("big", "`wThe Gurotopia Gazette``", 5016)
        .add_spacer("small")
        .add_image_button("banner", holiday_banner(), "bannerlayout", "")
        .add_spacer("small")
        .add_textbox(std::format("`w{} {}{}: {}|", month[time.tm_mon], time.tm_mday,
            (time.tm_mday >= 11 && time.tm_mday <= 13) ? "th" :
            (time.tm_mday % 10 == 1) ? "st" :
            (time.tm_mday % 10 == 2) ? "nd" :
            (time.tm_mday % 10 == 3) ? "rd" : "th", holiday_greeting().first))
        .add_spacer("small")
        .add_label("big", "`wServer Update``")
        .add_textbox("`oThe server just got better!`` We've been squashing bugs and polishing systems so your worlds run smoother, safer, and more fun. Here's what's new in this update:")
        .add_spacer("small")
        .add_label("medium", "`2Vending Machines``")
        .add_textbox("Fixed vending machine shop interactions, corrected buy/sell prices, fixed stock display, and prevented item loss when buying from public shops. Vending is now safe to use again!")
        .add_spacer("small")
        .add_label("medium", "`5Worlds & Locks``")
        .add_textbox("Fixed world locks, access lists, and public/build permissions. Locks now save correctly, visitors can no longer build where they shouldn't, and locked areas stay protected after re-entering.")
        .add_spacer("small")
        .add_label("medium", "`4Stability & Crash Fixes``")
        .add_textbox("Fixed crashes on world entry, tile updates, and item drops. Cleaned up disconnect handling and world saving so progress is no longer lost when the server restarts.")
        .add_spacer("small")
        .add_label("medium", "`oCommands & Chat``")
        .add_textbox("Fixed `/warp`, `/find`, `/sb`, `/who`, `/ghost` and other chat commands. Command arguments are now validated, staff commands are properly protected, and spam handling is more stable.")
        .add_spacer("small")
        .add_textbox("`2SummerFest`` is still rolling! Dive into sunny surprises and fire-y rewards while you enjoy the new fixes. Thank you for playing and reporting bugs - keep them coming!")
        .add_spacer("small")
        .add_smalltext("`wTip:`` Re-enter your world or relog if anything looks outdated. Visit our Social Media pages for more content!")
        .add_spacer("small")
        .add_image_button("gazette_DiscordServer", "interface/large/gazette/gazette_5columns_social_btn01.rttex", "7imageslayout", "https://discord.com/invite/zzWHgzaF7J")
        .add_layout_spacer("7imageslayout")
        .add_layout_spacer("7imageslayout")
        .add_layout_spacer("7imageslayout")
        .add_layout_spacer("7imageslayout")
        .add_layout_spacer("7imageslayout")
        .add_layout_spacer("7imageslayout")
        .add_spacer("small")
        .add_image_button("gazette_PrivacyPolicy", "interface/large/gazette/gazette_3columns_policy_btn02.rttex", "3imageslayout", "https://www.ubisoft.com/en-us/privacy-policy")
        .add_image_button("gazette_GrowtopianCode", "interface/large/gazette/gazette_3columns_policy_btn01.rttex", "3imageslayout", "https://support.ubi.com/en-us/growtopia-faqs/the-growtopian-code/")
        .add_image_button("gazette_TermsofUse", "interface/large/gazette/gazette_3columns_policy_btn03.rttex", "3imageslayout", "https://legal.ubi.com/termsofuse/")
        .add_quick_exit().add_spacer("small")
        .end_dialog("gazette", "", "OK");

    send_varlist(event.peer, { "OnDialogRequest", dialog });
}
