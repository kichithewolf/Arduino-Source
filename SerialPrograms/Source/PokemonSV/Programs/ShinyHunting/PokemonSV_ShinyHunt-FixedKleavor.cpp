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
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_ScalarButtons.h"
#include "Pokemon/Pokemon_Strings.h"
#include "PokemonSV/PokemonSV_Settings.h"
#include "PokemonSV/Inference/Battles/PokemonSV_EncounterWatcher.h"
#include "PokemonSV/Inference/Battles/PokemonSV_NormalBattleMenus.h"
#include "PokemonSV/Programs/PokemonSV_GameEntry.h"
#include "PokemonSV/Programs/PokemonSV_Navigation.h"
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
        "Shiny hunt the fixed encounter Kleavor in the Indigo Disk.",
        FeedbackType::REQUIRED,
        AllowCommandsWhenRunning::DISABLE_COMMANDS,
        PABotBaseLevel::PABOTBASE_12KB
    )
{}
struct ShinyHuntKleavor_Descriptor::Stats : public StatsTracker {
    Stats()
        : shinies(m_stats["Shinies"])
        , nonspawns(m_stats["Non-spawns"])
        , resets(m_stats["Resets"])
        , errors(m_stats["Errors"])
    {
        m_display_order.emplace_back("Shinies");
        m_display_order.emplace_back("Non-spawns");
        m_display_order.emplace_back("Resets");
        m_display_order.emplace_back("Errors", HIDDEN_IF_ZERO);
    }
    std::atomic<uint64_t>& shinies;
    std::atomic<uint64_t>& nonspawns;
    std::atomic<uint64_t>& resets;
    std::atomic<uint64_t>& errors;
};
std::unique_ptr<StatsTracker> ShinyHuntKleavor_Descriptor::make_stats() const {
    return std::unique_ptr<StatsTracker>(new Stats());
}
ShinyHuntKleavor::ShinyHuntKleavor()
    : GO_HOME_WHEN_DONE(false)
    , NOTIFICATION_STATUS_UPDATE("Status Update", true, false, std::chrono::seconds(3600))
    , NOTIFICATIONS({
        &NOTIFICATION_STATUS_UPDATE,
        &NOTIFICATION_PROGRAM_FINISH,
        &NOTIFICATION_ERROR_FATAL,
        })
{
    PA_ADD_OPTION(GO_HOME_WHEN_DONE);
    PA_ADD_OPTION(NOTIFICATIONS);
}

void ShinyHuntKleavor::program(SingleSwitchProgramEnvironment& env, BotBaseContext& context) {
    /*
    * Kleavor is a fixed encounter, full shiny odds, sandwich/charm does not apply
    * Settings - Camera Support: Off, Flight Controls: REGULAR, this is easier to go from moving forward into gaining height by just pushing the left stick up
    * Audio required
    * Start at Canyon Plaza fly point
    * You must not have encountered or had Kleavor spawn for the day (save, close, dateskip, then reopen the game to be sure)
    * Only really works because Kleavor is aggressive and will attack shortly after you fall on its head
    * make sure no mass outbreak in area - load new day and then save for consistency
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

                //Navigate to target
                //Position toward cliff ledge and mount ride
                //pbf_move_left_joystick(context, 192, 64, 20, 105);
                pbf_move_left_joystick(context, 205, 64, 20, 105);
                pbf_press_button(context, BUTTON_L | BUTTON_PLUS, 20, 105);

                //Head toward ledge, stay near plaza to avoid encounters
                pbf_move_left_joystick(context, 128, 0, 375, 10);

                //Jump
                ssf_press_left_joystick(context, 128, 0, 125, 875);
                ssf_press_button(context, BUTTON_B, 125, 100);

                //Glide
                ssf_press_button(context, BUTTON_B, 0, 20, 10);
                ssf_press_button(context, BUTTON_B, 0, 20);

                //Fly
                ssf_press_button(context, BUTTON_LCLICK, 0, 20, 10);
                ssf_press_button(context, BUTTON_LCLICK, 0, 20);

                pbf_wait(context, 1950);
                context.wait_for_all_requests();

                //Drop on top of Kleavor
                pbf_press_button(context, BUTTON_B, 50, 375);

                NormalBattleMenuWatcher battle_menu(COLOR_YELLOW);
                int ret2 = wait_until(
                    env.console, context,
                    std::chrono::seconds(45), //Enough time for Kleavor to notice and attack twice, sometimes it misses the first charge
                    { battle_menu }
                );

                if (ret2 != 0) {
                    stats.nonspawns++;
                    env.update_stats();

                    //Sometimes Kleavor does not spawn.
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
        if (ret == 0) {
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
            //Reset game, no date skip needed
            stats.resets++;
            env.update_stats();

            send_program_status_notification(
                env, NOTIFICATION_STATUS_UPDATE,
                "Resetting game."
            );

            pbf_press_button(context, BUTTON_HOME, 20, GameSettings::instance().GAME_TO_HOME_DELAY);
            reset_game_from_home(env.program_info(), env.console, context, 5 * TICKS_PER_SECOND);
        }
    }
    env.update_stats();
    GO_HOME_WHEN_DONE.run_end_of_program(context);
}

}
}
}
