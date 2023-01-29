/*  Auto Host
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#include "Common/Cpp/PrettyPrint.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Qt/TimeQt.h"
#include "ClientSource/Connection/BotBase.h"
#include "CommonFramework/Exceptions/FatalProgramException.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
//#include "CommonFramework/GlobalSettingsPanel.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
//#include "CommonFramework/InferenceInfra/InferenceSession.h"
#include "CommonFramework/InferenceInfra/InferenceRoutines.h"
#include "CommonFramework/Inference/BlackScreenDetector.h"
//#include "CommonFramework/OCR/OCR_StringNormalization.h"
#include "CommonFramework/Tools/StatsTracking.h"
#include "CommonFramework/Tools/ErrorDumper.h"
#include "CommonFramework/Tools/VideoResolutionCheck.h"
#include "CommonFramework/Tools/FileDownloader.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "Pokemon/Pokemon_Strings.h"
#include "PokemonSV/PokemonSV_Settings.h"
#include "PokemonSV/Inference/Battles/PokemonSV_BattleMenuDetector.h"
#include "PokemonSV/Inference/Dialogs/PokemonSV_DialogDetector.h"
#include "PokemonSV/Inference/Tera/PokemonSV_TeraCardDetector.h"
#include "PokemonSV/Programs/PokemonSV_GameEntry.h"
#include "PokemonSV/Programs/PokemonSV_ConnectToInternet.h"
#include "PokemonSV/Programs/PokemonSV_Navigation.h"
#include "PokemonSV/Programs/TeraRaids/PokemonSV_TeraBattler.h"
#include "PokemonSV/Programs/TeraRaids/PokemonSV_TeraRoutines.h"
#include "PokemonSV_AutoHostTools.h"
#include "PokemonSV_AutoHostLobbyWaiter.h"
#include "PokemonSV_AutoHost.h"

//#include <iostream>
//using std::cout;
//using std::endl;

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{

using namespace Pokemon;


AutoHost_Descriptor::AutoHost_Descriptor()
    : SingleSwitchProgramDescriptor(
        "PokemonSV:AutoHost",
        STRING_POKEMON + " SV", "Auto-Host",
        "ComputerControl/blob/master/Wiki/Programs/PokemonSV/AutoHost.md",
        "Auto-host a Tera raid.",
        FeedbackType::REQUIRED,
        AllowCommandsWhenRunning::DISABLE_COMMANDS,
        PABotBaseLevel::PABOTBASE_12KB
    )
{}
struct AutoHost_Descriptor::Stats : public StatsTracker{
    Stats()
        : m_raids(m_stats["Raids"])
        , m_empty(m_stats["Empty Raids"])
        , m_full(m_stats["Full Raids"])
        , m_raiders(m_stats["Total Raiders"])
        , m_wins(m_stats["Wins"])
        , m_losses(m_stats["Losses"])
        , m_banned(m_stats["Banned"])
        , m_errors(m_stats["Errors"])
    {
        m_display_order.emplace_back("Raids");
        m_display_order.emplace_back("Empty Raids");
        m_display_order.emplace_back("Full Raids");
        m_display_order.emplace_back("Total Raiders");
        m_display_order.emplace_back("Wins");
        m_display_order.emplace_back("Losses");
        m_display_order.emplace_back("Banned", true);
        m_display_order.emplace_back("Errors", true);
    }
    std::atomic<uint64_t>& m_raids;
    std::atomic<uint64_t>& m_empty;
    std::atomic<uint64_t>& m_full;
    std::atomic<uint64_t>& m_raiders;
    std::atomic<uint64_t>& m_wins;
    std::atomic<uint64_t>& m_losses;
    std::atomic<uint64_t>& m_banned;
    std::atomic<uint64_t>& m_errors;
};
std::unique_ptr<StatsTracker> AutoHost_Descriptor::make_stats() const{
    return std::unique_ptr<StatsTracker>(new Stats());
}



AutoHost::AutoHost()
    : SingleSwitchProgramInstance({"Notifs", "LiveHost"})
    , MODE(
        "<b>Hosting Mode:</b>",
        {
            {HostingMode::LOCAL,            "local",            "Host Locally"},
            {HostingMode::ONLINE_CODED,     "online-coded",     "Host Online (link code)"},
            {HostingMode::ONLINE_EVERYONE,  "online-everyone",  "Host Online (everyone)"},
        },
        LockWhileRunning::UNLOCKED,
        HostingMode::ONLINE_CODED
    )
    , NOTIFICATIONS0({
        &NOTIFICATION_RAID_POST,
        &NOTIFICATION_RAID_START,
        &NOTIFICATION_JOIN_REPORT,
        &NOTIFICATION_ERROR_RECOVERABLE,
        &NOTIFICATION_ERROR_FATAL,
    })
{
    PA_ADD_OPTION(MODE);

    //  General Auto-Hosting Options
    PA_ADD_OPTION(LOBBY_WAIT_DELAY);
    PA_ADD_OPTION(START_RAID_PLAYERS);
    PA_ADD_OPTION(SHOW_RAID_CODE);
    PA_ADD_OPTION(DESCRIPTION);
    PA_ADD_OPTION(REMOTE_KILL_SWITCH0);
    PA_ADD_OPTION(CONSECUTIVE_FAILURE_PAUSE);
    PA_ADD_OPTION(FAILURE_PAUSE_MINUTES);

    PA_ADD_OPTION(ROLLOVER_PREVENTION);
    PA_ADD_OPTION(BATTLE_AI);

    //  Extended Auto-Hosting Options
    PA_ADD_OPTION(BAN_LIST);
    PA_ADD_OPTION(JOIN_REPORT);

    PA_ADD_OPTION(NOTIFICATIONS0);
}



WallClock AutoHost::wait_for_lobby_open(
    SingleSwitchProgramEnvironment& env, BotBaseContext& context,
    std::string& lobby_code
){
    VideoOverlaySet overlays(env.console.overlay());

    TeraLobbyWatcher lobby(env.logger(), env.realtime_dispatcher(), COLOR_RED);
    lobby.make_overlays(overlays);

    int ret = wait_until(
        env.console, context,
        std::chrono::seconds(60),
        {{lobby, std::chrono::milliseconds(500)}}
    );
    if (ret < 0){
        dump_image_and_throw_recoverable_exception(
            env, env.console,
            NOTIFICATION_ERROR_RECOVERABLE,
            "NoLobby", "Unable to detect Tera lobby after 60 seconds."
        );
    }
    WallClock start_time = current_time();
    context.wait_for(std::chrono::seconds(1));

    send_host_announcement(
        env, env.console,
        lobby_code,
        SHOW_RAID_CODE,
        DESCRIPTION,
        NOTIFICATION_RAID_POST
    );

#if 0
    //  Send notification.
    std::vector<std::pair<std::string, std::string>> messages;
    std::string description = DESCRIPTION;
    if (!description.empty()){
        messages.emplace_back("Description:", std::move(description));
    }

    VideoSnapshot snapshot = env.console.video().snapshot();
    lobby_code = lobby.raid_code(env.logger(), env.realtime_dispatcher(), snapshot);
    if (SHOW_RAID_CODE && !lobby_code.empty()){
        messages.emplace_back("Raid Code:", lobby_code);
    }

    send_program_notification(
        env, NOTIFICATION_RAID_POST,
        Color(0),
        "Tera Raid Notification",
        messages, "",
        snapshot
    );
#endif

    return start_time;
}
void AutoHost::update_stats_on_raid_start(SingleSwitchProgramEnvironment& env, uint8_t player_count){
    AutoHost_Descriptor::Stats& stats = env.current_stats<AutoHost_Descriptor::Stats>();

    player_count = std::max<uint8_t>(player_count, 1);

    if (player_count == 4){
        stats.m_full++;
    }
    if (player_count == 1){
        stats.m_empty++;
    }
    stats.m_raiders += player_count - 1;
}
bool AutoHost::start_raid(
    SingleSwitchProgramEnvironment& env, BotBaseContext& context,
    WallClock start_time,
    uint8_t player_count
){
    AutoHost_Descriptor::Stats& stats = env.current_stats<AutoHost_Descriptor::Stats>();

    //  This is the state machine to actually start the raid.

    while (true){
        AdvanceDialogWatcher dialog(COLOR_YELLOW);
        WhiteScreenOverWatcher start_raid(COLOR_BLUE);
        TeraBattleMenuWatcher battle_menu(COLOR_CYAN);
        context.wait_for_all_requests();
        int ret = run_until(
            env.console, context,
            [start_time](BotBaseContext& context){
                while (true){
                    pbf_press_button(context, BUTTON_A, 20, 105);
                    context.wait_for_all_requests();
                    if (current_time() > start_time + std::chrono::minutes(4)){
                        return;
                    }
                }
            },
            {dialog, start_raid, battle_menu}
        );
        context.wait_for(std::chrono::milliseconds(100));
        switch (ret){
        case 0:
            env.log("Raid timed out!", COLOR_ORANGE);
            stats.m_empty++;
            return false;
        case 1:
            env.log("Raid started! (white screen)", COLOR_BLUE);
            update_stats_on_raid_start(env, player_count);
            return true;
        case 2:
            env.log("Raid started! (battle menu)", COLOR_BLUE);
            update_stats_on_raid_start(env, player_count);
            return true;
        default:
            dump_image_and_throw_recoverable_exception(
                env, env.console,
                NOTIFICATION_ERROR_RECOVERABLE,
                "StuckLobby", "Stuck in lobby for 4 minutes."
            );
        }
    }
}


bool AutoHost::run_lobby(
    SingleSwitchProgramEnvironment& env, BotBaseContext& context,
    std::string& lobby_code,
    std::array<std::map<Language, std::string>, 4>& player_names
){
    AutoHost_Descriptor::Stats& stats = env.current_stats<AutoHost_Descriptor::Stats>();

    WallClock start_time = wait_for_lobby_open(env, context, lobby_code);

    TeraLobbyWaiter waiter(
        env, env.console, context,
        1,
        lobby_code, start_time,
        LOBBY_WAIT_DELAY,
        START_RAID_PLAYERS,
        NOTIFICATION_RAID_START,
        BAN_LIST,
        JOIN_REPORT
    );

    TeraLobbyWaiter::LobbyResult result = waiter.run_lobby();
    player_names = waiter.names();

    if (result == TeraLobbyWaiter::LobbyResult::BANNED_PLAYER){
        stats.m_banned++;
    }
    if (result != TeraLobbyWaiter::LobbyResult::RAID_STARTED){
        return false;
    }

    return start_raid(env, context, start_time, waiter.last_known_players());
}
void AutoHost::check_kill_switch(ProgramEnvironment& env){
    std::string kill_switch_url = REMOTE_KILL_SWITCH0;
    if (MODE == HostingMode::LOCAL || kill_switch_url.empty()){
        return;
    }

    WallClock start_time = env.program_info().start_time;

    if (kill_switch_url.ends_with(".txt")){
        env.log("Loading remote kill switch time...");
        try{
            std::string kill_time_str = FileDownloader::download_file(env.logger(), kill_switch_url);
            m_killswitch_time = parse_utc_time_str(kill_time_str);
        }catch (OperationFailedException& e){
            env.log("Unable to load kill switch URL: " + e.message(), COLOR_RED);
        }catch (ParseException& e){
            env.log("Unable to load kill switch URL: " + e.message(), COLOR_RED);
        }
    }else if (kill_switch_url.ends_with(".json")){
        env.log("Loading remote kill switch time...");

        try{
            JsonValue json = FileDownloader::download_json_file(env.logger(), kill_switch_url);
            const JsonObject* obj = json.get_object();
            if (obj == nullptr){
                throw ParseException("Invalid kill-switch Json.");
            }
            const std::string* date = obj->get_string("date");
            if (date == nullptr){
                env.log("Invalid Kill Switch Json", COLOR_RED);
            }else{
                m_killswitch_time = parse_utc_time_str(*date);
            }
            const std::string* reason = obj->get_string("reason");
            if (date == nullptr){
                env.log("Invalid Kill Switch Json", COLOR_RED);
            }else{
                m_killswitch_reason = *reason;
            }
        }catch (OperationFailedException& e){
            env.log("Invalid kill-switch JSON: " + e.message(), COLOR_RED);
        }catch (ParseException& e){
            env.log("Invalid kill-switch JSON: " + e.message(), COLOR_RED);
        }
    }else{
        throw UserSetupError(env.logger(), "Invalid kill switch URL extension.");
    }

    WallClock now = current_time();
    env.log(
        "Start UTC: " + to_utc_time_str(start_time) +
        ", Current UTC: " + to_utc_time_str(now) +
        ", Kill UTC: " + to_utc_time_str(m_killswitch_time)
    );
    if (start_time < m_killswitch_time && now > m_killswitch_time){
        if (m_killswitch_reason.empty()){
            throw FatalProgramException(env.logger(), "Stopped by remote kill switch. No reason specified.");
        }else{
            throw FatalProgramException(env.logger(), "Stopped by remote kill switch. Reason: " + m_killswitch_reason);
        }
    }
}

void AutoHost::program(SingleSwitchProgramEnvironment& env, BotBaseContext& context){
    assert_16_9_720p_min(env.logger(), env.console);

    AutoHost_Descriptor::Stats& stats = env.current_stats<AutoHost_Descriptor::Stats>();

    //  Connect the controller.
    pbf_press_button(context, BUTTON_LCLICK, 10, 10);

    m_killswitch_time = WallClock::max();

    std::string report_name = "PokemonSV-AutoHost-JoinReport-" + now_to_filestring() + ".txt";
    MultiLanguageJoinTracker join_tracker;

    bool skip_reset = true;
    bool completed_one = false;
    size_t consecutive_failures = 0;
    WallClock last_time_fix = WallClock::min();
    while (true){
        env.update_stats();

        if (consecutive_failures > 0 && !completed_one){
            throw FatalProgramException(env.logger(), "Failed 1st raid attempt. Will not retry due to risk of ban.");
        }
        size_t fail_threshold = CONSECUTIVE_FAILURE_PAUSE;
        if (consecutive_failures >= fail_threshold){
            uint16_t minutes = FAILURE_PAUSE_MINUTES;
            if (minutes == 0){
                throw FatalProgramException(
                    env.logger(),
                    "Failed " + std::to_string(fail_threshold) +  " raid(s) in the row. "
                    "Stopping to prevent possible ban."
                );
            }else{
                send_program_recoverable_error_notification(
                    env, NOTIFICATION_ERROR_RECOVERABLE,
                    "Failed " + std::to_string(fail_threshold) +  " raid(s) in the row. "
                    "Pausing program for " + std::to_string(minutes) + " minute(s)."
                );
                WallClock start_time = current_time();
                while (current_time() < start_time + std::chrono::minutes(minutes)){
                    context.wait_for(std::chrono::seconds(1));
                }
                consecutive_failures = 0;
            }
        }

        if (!skip_reset){
            pbf_press_button(context, BUTTON_HOME, 20, GameSettings::instance().GAME_TO_HOME_DELAY);
            if (ROLLOVER_PREVENTION){
                WallClock now = current_time();
                if (last_time_fix == WallClock::min() || now - last_time_fix > std::chrono::hours(4)){
                    set_time_to_12am_from_home(env.program_info(), env.console, context);
                    last_time_fix = now;
                }
            }
            reset_game_from_home(env.program_info(), env.console, context, 5 * TICKS_PER_SECOND);
        }
        skip_reset = false;

        //  Check kill-switch now before we go online.
        check_kill_switch(env);

        //  Store the mode locally in case the user changes in the middle of
        //  this iteration.
        HostingMode mode = MODE;

        if (mode != HostingMode::LOCAL){
            //  Connect to internet.
            try{
                connect_to_internet_from_overworld(env.program_info(), env.console, context);
            }catch (OperationFailedException& e){
                consecutive_failures++;
                stats.m_errors++;
                e.send_notification(env, NOTIFICATION_ERROR_RECOVERABLE);
                continue;
            }
        }

        if (!open_raid(env.console, context)){
            env.log("No Tera raid found.", COLOR_RED);
            consecutive_failures++;
            continue;
        }
        env.log("Tera raid found!", COLOR_BLUE);

        context.wait_for(std::chrono::milliseconds(100));

        BAN_LIST.refresh_online_table(env.logger());

#if 1
        try{
            open_hosting_lobby(env, env.console, context, mode);
        }catch (OperationFailedException& e){
            consecutive_failures++;
            stats.m_errors++;
            e.send_notification(env, NOTIFICATION_ERROR_RECOVERABLE);
            continue;
        }
#else
        pbf_press_button(context, BUTTON_A, 20, 230);
        if (mode != HostingMode::LOCAL){
            if (mode == HostingMode::ONLINE_EVERYONE){
                pbf_press_dpad(context, DPAD_DOWN, 20, 105);
            }
            pbf_press_button(context, BUTTON_A, 20, 230);
        }
        context.wait_for_all_requests();
#endif

        try{
            std::string lobby_code;
            std::array<std::map<Language, std::string>, 4> player_names;

            if (!run_lobby(env, context, lobby_code, player_names)){
                continue;
            }
            env.update_stats();

            stats.m_raids++;
            bool win = run_tera_battle(
                env, env.console, context,
                NOTIFICATION_ERROR_RECOVERABLE,
                BATTLE_AI
            );
            env.update_stats();
            if (win){
                stats.m_wins++;
            }else{
                stats.m_losses++;
            }
            if (JOIN_REPORT.enabled() && (win || !JOIN_REPORT.wins_only)){
                join_tracker.append(player_names, lobby_code);
                join_tracker.dump(report_name);
                send_program_notification_with_file(
                    env,
                    NOTIFICATION_JOIN_REPORT,
                    Color(0),
                    "Join Report",
                    {}, "",
                    report_name
                );
            }
            if (win){
                exit_tera_win_without_catching(env.program_info(), env.console, context, 0);
            }
            completed_one = true;
            consecutive_failures = 0;
        }catch (OperationFailedException& e){
            consecutive_failures++;
            stats.m_errors++;
            e.send_notification(env, NOTIFICATION_ERROR_RECOVERABLE);
            continue;
        }
    }
}



}
}
}