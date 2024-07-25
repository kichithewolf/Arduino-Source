/*  Fixed Kleavor
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/InferenceInfra/InferenceRoutines.h"
#include "CommonFramework/Tools/StatsTracking.h"
#include "CommonFramework/Tools/VideoResolutionCheck.h"
#include "NintendoSwitch/NintendoSwitch_Settings.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_Superscalar.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "Pokemon/Pokemon_Strings.h"
#include "PokemonSwSh/Commands/PokemonSwSh_Commands_DateSpam.h"
#include "PokemonSV/PokemonSV_Settings.h"
#include "PokemonSV/Inference/Battles/PokemonSV_EncounterWatcher.h"
#include "PokemonSV/Inference/Battles/PokemonSV_NormalBattleMenus.h"
#include "PokemonSV/Inference/Overworld/PokemonSV_OverworldDetector.h"
#include "PokemonSV/Programs/PokemonSV_GameEntry.h"
#include "PokemonSV/Programs/PokemonSV_Navigation.h"
#include "PokemonSV/Programs/PokemonSV_Terarium.h"
#include "PokemonSV_ShinyHunt-FixedKleavor.h"

namespace PokemonAutomation {
namespace NintendoSwitch {
namespace PokemonSV {

using namespace Pokemon;

ShinyHuntKleavor_Descriptor::ShinyHuntKleavor_Descriptor()
    : SingleSwitchProgramDescriptor(
        "PokemonSV:ShinyHuntKleavor",
        STRING_POKEMON + " SV", "Shiny Hunt - Kleavor",
        "ComputerControl/blob/master/Wiki/Programs/PokemonSV/ShinyHuntKleavor.md",
        "Shiny hunt the fixed spawn Kleavor in the Indigo Disk.",
        FeedbackType::VIDEO_AUDIO,
        AllowCommandsWhenRunning::DISABLE_COMMANDS,
        PABotBaseLevel::PABOTBASE_12KB
    )
{}
struct ShinyHuntKleavor_Descriptor::Stats : public StatsTracker {
    Stats()
        : shinies(m_stats["Shinies"])
        , nonspawns(m_stats["Non-spawns"])
        , dateskips(m_stats["Date skips"])
        , errors(m_stats["Errors"])
    {
        m_display_order.emplace_back("Shinies");
        m_display_order.emplace_back("Non-spawns");
        m_display_order.emplace_back("Date skips");
        m_display_order.emplace_back("Errors", HIDDEN_IF_ZERO);
    }
    std::atomic<uint64_t>& shinies;
    std::atomic<uint64_t>& nonspawns;
    std::atomic<uint64_t>& dateskips;
    std::atomic<uint64_t>& errors;
};
std::unique_ptr<StatsTracker> ShinyHuntKleavor_Descriptor::make_stats() const {
    return std::unique_ptr<StatsTracker>(new Stats());
}
ShinyHuntKleavor::ShinyHuntKleavor()
    : INVERTED_FLIGHT(
        "<b>Inverted Flight:</b><br>Check this box if inverted flight controls are set.",
        LockMode::LOCK_WHILE_RUNNING,
        false
    )
    , FIX_TIME_WHEN_DONE(
        "<b>Fix Time when Done:</b><br>Fix the time after the program finishes.",
        LockMode::UNLOCK_WHILE_RUNNING, false
    )
    , GO_HOME_WHEN_DONE(false)
    , NOTIFICATION_STATUS_UPDATE("Status Update", true, false, std::chrono::seconds(3600))
    , NOTIFICATIONS({
        &NOTIFICATION_STATUS_UPDATE,
        &NOTIFICATION_PROGRAM_FINISH,
        &NOTIFICATION_ERROR_FATAL,
        })
{
    PA_ADD_OPTION(INVERTED_FLIGHT);
    PA_ADD_OPTION(FIX_TIME_WHEN_DONE);
    PA_ADD_OPTION(GO_HOME_WHEN_DONE);
    PA_ADD_OPTION(NOTIFICATIONS);
}

void ShinyHuntKleavor::program(SingleSwitchProgramEnvironment& env, BotBaseContext& context) {
    /*
    * Kleavor is a fixed encounter, full shiny odds, sandwich/charm does not apply
    * Settings - Camera Support: Off
    * Audio required, your lead must not be shiny
    * Start at Canyon Plaza fly point
    * Note that this program is mostly for proof of concept purposes, if you actually want shiny kleavor just find a shiny raid
    */

    assert_16_9_720p_min(env.logger(), env.console);
    ShinyHuntKleavor_Descriptor::Stats& stats = env.current_stats<ShinyHuntKleavor_Descriptor::Stats>();

    //  Connect the controller.
    pbf_press_button(context, BUTTON_L, 10, 10);
    
    while (true) {
        EncounterWatcher encounter_watcher(env.console, COLOR_RED);

        int ret = run_until(
            env.console, context,
            [&](BotBaseContext& context) {
                env.log("Navigating to Kleavor and attempting encounter.");

                pbf_move_left_joystick(context, 205, 64, 20, 105);
                pbf_press_button(context, BUTTON_L | BUTTON_PLUS, 20, 105);

                jump_glide_fly(env.console, context, INVERTED_FLIGHT, 1000, 1650, 300);

                ssf_press_button(context, BUTTON_ZR, 0, 200);
                ssf_press_button(context, BUTTON_ZL, 100, 50);
                ssf_press_button(context, BUTTON_ZL, 150, 50);

                pbf_wait(context, 300);
                context.wait_for_all_requests();

                NormalBattleMenuWatcher battle_menu(COLOR_YELLOW);
                int ret2 = wait_until(
                    env.console, context,
                    std::chrono::seconds(15),
                    { battle_menu }
                );

                if (ret2 != 0) {
                    stats.nonspawns++;
                    env.update_stats();
                    env.log("Did not enter battle. Did Kleavor spawn?");

                    send_program_status_notification(
                        env, NOTIFICATION_STATUS_UPDATE,
                        "Did not enter battle. Did Kleavor spawn?"
                    );
                }
            },
            {
                static_cast<VisualInferenceCallback&>(encounter_watcher),
                static_cast<AudioInferenceCallback&>(encounter_watcher),
            }
            );
        if (ret >= 0) {
            env.log("Battle menu detected.");
        }
        encounter_watcher.throw_if_no_sound();

        bool is_shiny = (bool)encounter_watcher.shiny_screenshot();
        if (is_shiny) {
            stats.shinies++;
            env.update_stats();
            pbf_press_button(context, BUTTON_CAPTURE, 2 * TICKS_PER_SECOND, 5 * TICKS_PER_SECOND);
            send_program_finished_notification(
                env, NOTIFICATION_PROGRAM_FINISH,
                "Shiny found!", encounter_watcher.shiny_screenshot(), true
            );
            break;
        } else {
            //Run from battle
            OverworldWatcher overworld(COLOR_BLUE);

            int ret2 = run_until(
                env.console, context,
                [&](BotBaseContext& context){
                while (true){
                    NormalBattleMenuWatcher battle_menu(COLOR_YELLOW);
                    int ret2 = wait_until(
                        env.console, context,
                        std::chrono::seconds(60),
                        { battle_menu }
                    );
                    switch (ret2){
                    case 0:
                        env.log("Attempting to flee.");
                        battle_menu.move_to_slot(env.console, context, 3);
                        pbf_press_button(context, BUTTON_A, 10, 50);
                        break;
                    default:
                        env.log("Invalid state when trying to run. Is a Smoke Ball equipped?");
                        throw OperationFailedException(
                            ErrorReport::SEND_ERROR_REPORT, env.console,
                            "Invalid state when trying to run. Is a Smoke Ball equipped?",
                            true
                        );
                    }
                }
                },
                { overworld }
                );
            switch (ret2){
            case 0:
                env.log("Overworld detected.");
                break;
            default:
                env.log("Invalid state in run_until().");
                throw OperationFailedException(
                    ErrorReport::SEND_ERROR_REPORT, env.console,
                    "Invalid state in run_until().",
                    true
                );
            }

            //Fly to plaza, day skip, fly back. This is faster than resetting the game.
            pbf_press_button(context, BUTTON_PLUS, 20, 105);
            return_to_plaza(env.program_info(), env.console, context);

            //Day skip and attempt to respawn fixed encounters
            pbf_press_button(context, BUTTON_HOME, 20, GameSettings::instance().GAME_TO_HOME_DELAY);
            home_to_date_time(context, true, true);
            PokemonSwSh::roll_date_forward_1(context, true);
            resume_game_from_home(env.console, context);
            stats.dateskips++;
            env.update_stats();

            central_to_canyon_plaza(env.program_info(), env.console, context);
        }
    }

    if (FIX_TIME_WHEN_DONE){
        pbf_press_button(context, BUTTON_HOME, 10, GameSettings::instance().GAME_TO_HOME_DELAY);
        home_to_date_time(context, false, false);
        pbf_press_button(context, BUTTON_A, 20, 105);
        pbf_press_button(context, BUTTON_A, 20, 105);
        pbf_press_button(context, BUTTON_HOME, 20, ConsoleSettings::instance().SETTINGS_TO_HOME_DELAY);
        resume_game_from_home(env.console, context);
    }

    env.update_stats();
    GO_HOME_WHEN_DONE.run_end_of_program(context);
}

}
}
}
