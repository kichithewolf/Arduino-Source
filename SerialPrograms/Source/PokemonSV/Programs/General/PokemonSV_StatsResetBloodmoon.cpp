/*  Stats Reset Bloodmoon
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/Options/LanguageOCROption.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/InferenceInfra/InferenceRoutines.h"
#include "CommonFramework/Tools/StatsTracking.h"
#include "CommonFramework/Tools/VideoResolutionCheck.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "Pokemon/Pokemon_Strings.h"
#include "PokemonSV/PokemonSV_Settings.h"
#include "PokemonSV/Inference/Dialogs/PokemonSV_DialogDetector.h"
#include "PokemonSV/Inference/Battles/PokemonSV_NormalBattleMenus.h"
#include "PokemonSV/Inference/Boxes/PokemonSV_StatsResetChecker.h"
#include "PokemonSV/Inference/Boxes/PokemonSV_BoxDetection.h"
#include "PokemonSV/Inference/Boxes/PokemonSV_IVCheckerReader.h"
#include "PokemonSV/Inference/Overworld/PokemonSV_OverworldDetector.h"
#include "PokemonSV/Programs/PokemonSV_GameEntry.h"
#include "PokemonSV/Programs/PokemonSV_Navigation.h"
#include "PokemonSV/Programs/PokemonSV_BasicCatcher.h"
#include "PokemonSV/Programs/Boxes/PokemonSV_BoxRoutines.h"
#include "PokemonSV_StatsResetBloodmoon.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{

using namespace Pokemon;
 
StatsResetBloodmoon_Descriptor::StatsResetBloodmoon_Descriptor()
    : SingleSwitchProgramDescriptor(
        "PokemonSV:StatsResetBloodmoon",
        STRING_POKEMON + " SV", "Stats Reset - Bloodmoon Ursaluna",
        "ComputerControl/blob/master/Wiki/Programs/PokemonSV/StatsResetBloodmoon.md",
        "Repeatedly catch Bloodmoon Ursaluna until you get the stats you want.",
        FeedbackType::REQUIRED,
        AllowCommandsWhenRunning::DISABLE_COMMANDS,
        PABotBaseLevel::PABOTBASE_12KB
    )
{}
struct StatsResetBloodmoon_Descriptor::Stats : public StatsTracker {
    Stats()
        : resets(m_stats["Resets"])
        , catches(m_stats["Catches"])
        , matches(m_stats["Matches"])
        , errors(m_stats["Errors"])
    {
        m_display_order.emplace_back("Resets");
        m_display_order.emplace_back("Catches");
        m_display_order.emplace_back("Matches");
        m_display_order.emplace_back("Errors", true);
    }
    std::atomic<uint64_t>& resets;
    std::atomic<uint64_t>& catches;
    std::atomic<uint64_t>& matches;
    std::atomic<uint64_t>& errors;
};
std::unique_ptr<StatsTracker> StatsResetBloodmoon_Descriptor::make_stats() const {
    return std::unique_ptr<StatsTracker>(new Stats());
}
StatsResetBloodmoon::StatsResetBloodmoon()
    : LANGUAGE(
        "<b>Game Language:</b><br>This field is required so we can read IVs.",
        IV_READER().languages(),
        LockWhileRunning::LOCKED,
        true
    )
    , BALL_SELECT(
        "<b>Ball Select:</b>",
        LockWhileRunning::UNLOCKED,
        "poke-ball"
    )
    , GO_HOME_WHEN_DONE(false)
    , NOTIFICATION_STATUS_UPDATE("Status Update", true, false, std::chrono::seconds(3600))
    , NOTIFICATIONS({
        &NOTIFICATION_STATUS_UPDATE,
        & NOTIFICATION_PROGRAM_FINISH,
        & NOTIFICATION_ERROR_FATAL,
    })
{
    PA_ADD_OPTION(LANGUAGE);
    PA_ADD_OPTION(BALL_SELECT);
    PA_ADD_OPTION(BATTLE_AI);
    PA_ADD_OPTION(FILTERS);
    PA_ADD_OPTION(GO_HOME_WHEN_DONE);
    PA_ADD_OPTION(NOTIFICATIONS);
}
bool run_battle(ProgramEnvironment& env, ConsoleHandle& console, BotBaseContext& context) {
    size_t turn = 0;
    std::vector<BattleMoveEntry> move_table = BATTLE_AI.snapshot();
    BattleMoveEntry current_move{ BattleMoveType::Move1, 0, BattleTarget::Opponent };
    if (!move_table.empty()) {
        current_move = move_table[0];
    }

    size_t consecutive_timeouts = 0;
    size_t consecutive_move_select = 0;
    bool battle_menu_seen = false;
    bool next_turn_on_battle_menu = false;
    while (true) {
        // Warning, this terastallizing detector isn't used in the wait_until() below.
        TerastallizingDetector terastallizing(COLOR_ORANGE);
        VideoOverlaySet overlay_set(console);
        terastallizing.make_overlays(overlay_set);

        TeraBattleMenuWatcher battle_menu(COLOR_RED);
        MoveSelectWatcher move_select_menu(COLOR_YELLOW);
        WhiteButtonWatcher next_button(
            COLOR_CYAN,
            WhiteButton::ButtonA,
            { 0.8, 0.93, 0.2, 0.07 },
            WhiteButtonWatcher::FinderType::PRESENT,
            //  This can false positive against the back (B) button while in the
            //  lobby. So don't trigger this unless we see the battle menu first.
            //  At the same time, there's a possibility that we miss the battle
            //  menu if the raid is won before it even loads. And this can only
            //  happen if the raid was uncatchable to begin with.
            std::chrono::seconds(battle_menu_seen ? 5 : 60)
        );
        TeraCatchWatcher catch_menu(COLOR_BLUE);
        OverworldWatcher overworld(COLOR_GREEN);
        context.wait_for_all_requests();
        int ret = run_until(
            console, context,
            [](BotBaseContext& context) {
                for (size_t c = 0; c < 4; c++) {
                    pbf_wait(context, 30 * TICKS_PER_SECOND);
                    pbf_press_button(context, BUTTON_B, 20, 0);
                }
            },
            {
                battle_menu,
                cheer_select_menu,
                move_select_menu,
                target_select_menu,
                next_button,
                catch_menu,
                overworld,
            }
            );
        context.wait_for(std::chrono::milliseconds(100));
        switch (ret) {
        case 0: {
            console.log("Detected battle menu.");
            battle_menu_seen = true;

            //  If we enter here, we advance to the next turn.
            if (next_turn_on_battle_menu) {
                console.log("Detected battle menu. Turn: " + std::to_string(turn));
                turn++;
                //  Reset the move to the table entry in case we were forced to
                //  change moves due to move being unselectable.
                if (move_table.empty()) {
                    current_move = TeraMoveEntry{ TeraMoveType::Move1, 0, TeraTarget::Opponent };
                }
                else if (turn < move_table.size()) {
                    current_move = move_table[turn];
                }
                else {
                    current_move = move_table.back();
                }
                next_turn_on_battle_menu = false;
            }

            console.log("Current Move Selection: " + current_move.to_str());
            BattleMenuResult battle_menu_result = run_battle_menu(
                console, context,
                move_table,
                battle_menu,
                catch_menu,
                overworld,
                current_move
            );
            switch (battle_menu_result) {
            case BattleMenuResult::RESUME_CURRENT_TURN:
                continue;
            case BattleMenuResult::RESUME_ADVANCE_TURN:
                next_turn_on_battle_menu = true;
                continue;
            case BattleMenuResult::BATTLE_WON:
                console.log("Detected a win!", COLOR_BLUE);
                pbf_mash_button(context, BUTTON_B, 30);
                return true;
            case BattleMenuResult::BATTLE_LOST:
                console.log("Detected a loss!", COLOR_ORANGE);
                return false;
            }
        }
        case 1: {
            console.log("Detected cheer select. Turn: " + std::to_string(turn));
            if (run_cheer_select(console, context, cheer_select_menu, current_move)) {
                next_turn_on_battle_menu = true;
            }
            continue;
        }
        case 2: {
            console.log("Detected move select. Turn: " + std::to_string(turn));
            consecutive_move_select++;
            run_move_select(
                console, context,
                battle_AI,
                terastallizing,
                move_select_menu,
                current_move,
                consecutive_move_select
            );
            continue;
        }
        case 3:
            console.log("Detected target select. Turn: " + std::to_string(turn));
            consecutive_move_select = 0;
            if (run_target_select(console, context, target_select_menu, current_move)) {
                next_turn_on_battle_menu = true;
            }
            continue;
        case 4:
        case 5:
            console.log("Detected a win!", COLOR_BLUE);
            pbf_mash_button(context, BUTTON_B, 30);
            return true;
        case 6:
            console.log("Detected a loss!", COLOR_ORANGE);
            return false;
        default:
            consecutive_timeouts++;
            if (consecutive_timeouts == 3) {
                throw OperationFailedException(
                    ErrorReport::SEND_ERROR_REPORT, console,
                    "No state detected after 6 minutes.",
                    true
                );
            }
            console.log("Unable to detect any state for 2 minutes. Mashing B...", COLOR_RED);
            pbf_mash_button(context, BUTTON_B, 250);
        }
    }

    return false;
}

bool StatsResetBloodmoon::check_stats(SingleSwitchProgramEnvironment& env, BotBaseContext& context) {
    StatsResetBloodmoon_Descriptor::Stats& stats = env.current_stats<StatsResetBloodmoon_Descriptor::Stats>();

    bool stats_matched = false;

    //Finish up cutscenes, etc.
    pbf_mash_button(context, BUTTON_B, 170);
    context.wait_for_all_requests();

    //Open box
    enter_box_system_from_overworld(env.program_info(), env.console, context);
    context.wait_for(std::chrono::milliseconds(400));

    if (check_empty_slots_in_party(env.program_info(), env.console, context) != 0) {
        //Is this even possible for Ursaluna?
        env.console.log("One or more empty slots in party. Ursaluna was not caught or user setup error.");
        send_program_status_notification(
            env, NOTIFICATION_STATUS_UPDATE,
            "One or more empty slots in party. Ursaluna was not caught."
        );
    }
    else {
        //Navigate to last party slot
        move_box_cursor(env.program_info(), env.console, context, BoxCursorLocation::PARTY, 5, 0);

        //Check the IVs of the newly caught Pokemon - *must be on IV panel*
        EggHatchAction action = EggHatchAction::Keep;
        check_stats_reset_info(env.console, context, LANGUAGE, FILTERS, action);

        switch (action) {
        case EggHatchAction::StopProgram:
            stats_matched = true;
            env.console.log("Match found!");
            stats.matches++;
            env.update_stats();
            send_program_status_notification(
                env, NOTIFICATION_PROGRAM_FINISH,
                "Match found!"
            );
            break;
        case EggHatchAction::Release:
            stats_matched = false;
            env.console.log("Stats did not match table settings.");
            send_program_status_notification(
                env, NOTIFICATION_STATUS_UPDATE,
                "Stats did not match table settings."
            );
            break;
        default:
            env.console.log("Invalid state.");
            stats.errors++;
            env.update_stats();
            throw OperationFailedException(
                ErrorReport::SEND_ERROR_REPORT, env.console,
                "Invalid state.",
                true
            );
        }
    }
    return stats_matched;
}

void StatsResetBloodmoon::program(SingleSwitchProgramEnvironment& env, BotBaseContext& context) {
    assert_16_9_720p_min(env.logger(), env.console);
    //StatsResetBloodmoon_Descriptor::Stats& stats = env.current_stats<StatsResetBloodmoon_Descriptor::Stats>();

    /*
    bool stats_matched = false;
    while (!stats_matched){

        if (battle_ended){

        }
        if (!battle_ended){
            send_program_status_notification(
                env, NOTIFICATION_STATUS_UPDATE,
                "Resetting game."
            );
            //Reset game
            stats.resets++;
            env.update_stats();
            pbf_press_button(context, BUTTON_HOME, 20, GameSettings::instance().GAME_TO_HOME_DELAY);
            reset_game_from_home(env.program_info(), env.console, context, 5 * TICKS_PER_SECOND);
        }
    }
    */

    env.update_stats();
    GO_HOME_WHEN_DONE.run_end_of_program(context);
    send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH);
}
    
}
}
}
