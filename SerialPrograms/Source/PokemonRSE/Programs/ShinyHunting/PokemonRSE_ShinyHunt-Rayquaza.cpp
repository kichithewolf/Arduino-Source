/*  E Shiny Rayquaza
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#include "Common/Cpp/PrettyPrint.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/InferenceInfra/InferenceRoutines.h"
#include "CommonFramework/Inference/BlackScreenDetector.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/Tools/VideoResolutionCheck.h"
#include "CommonFramework/Tools/StatsTracking.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_Superscalar.h"
#include "PokemonRSE/Inference/Dialogs/PokemonRSE_DialogDetector.h"
#include "PokemonRSE/Inference/Sounds/PokemonRSE_ShinySoundDetector.h"
#include "PokemonRSE/PokemonRSE_Navigation.h"
#include "PokemonRSE_ShinyHunt-Rayquaza.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonRSE{

ShinyHuntRayquaza_Descriptor::ShinyHuntRayquaza_Descriptor()
    : SingleSwitchProgramDescriptor(
        "PokemonRSE:ShinyHuntRayquaza",
        "Pokemon RSE", "Shiny Hunt - Rayquaza",
        "ComputerControl/blob/master/Wiki/Programs/PokemonRSE/ShinyHuntRayquaza.md",
        "Use the Run Away method to shiny hunt Rayquaza in Emerald.",
        FeedbackType::VIDEO_AUDIO_GBA,
        AllowCommandsWhenRunning::DISABLE_COMMANDS,
        PABotBaseLevel::PABOTBASE_12KB
    )
{}

struct ShinyHuntRayquaza_Descriptor::Stats : public StatsTracker{
    Stats()
        : resets(m_stats["Resets"])
        , shinies(m_stats["Shinies"])
    {
        m_display_order.emplace_back("Resets");
        m_display_order.emplace_back("Shinies");
    }
    std::atomic<uint64_t>& resets;
    std::atomic<uint64_t>& shinies;
};
std::unique_ptr<StatsTracker> ShinyHuntRayquaza_Descriptor::make_stats() const{
    return std::unique_ptr<StatsTracker>(new Stats());
}

ShinyHuntRayquaza::ShinyHuntRayquaza()
    : NOTIFICATION_SHINY(
        "Shiny Found",
        true, true, ImageAttachmentMode::JPG,
        {"Notifs", "Showcase"}
    )
    , NOTIFICATION_STATUS_UPDATE("Status Update", true, false, std::chrono::seconds(3600))
    , NOTIFICATIONS({
        &NOTIFICATION_SHINY,
        &NOTIFICATION_STATUS_UPDATE,
        &NOTIFICATION_PROGRAM_FINISH,
        })
{
    PA_ADD_OPTION(NOTIFICATIONS);
}

void ShinyHuntRayquaza::flee_battle(SingleSwitchProgramEnvironment& env, BotBaseContext& context) {
    env.log("Navigate to Run.");
    pbf_press_dpad(context, DPAD_RIGHT, 20, 20);
    pbf_press_dpad(context, DPAD_DOWN, 20, 20);
    pbf_press_button(context, BUTTON_A, 20, 40);

    AdvanceDialogWatcher ran_away(COLOR_YELLOW);
    int ret2 = wait_until(
        env.console, context,
        std::chrono::seconds(5),
        {{ran_away}}
    );
    if (ret2 == 0) {
        env.log("Running away...");
    } else {
        OperationFailedException::fire(
            env.console, ErrorReport::SEND_ERROR_REPORT,
            "handle_encounter(): Unable to navigate to flee button."
        );
    }

    pbf_press_button(context, BUTTON_A, 40, 40);
    BlackScreenOverWatcher battle_over(COLOR_RED, {0.282, 0.064, 0.448, 0.871});
    int ret3 = wait_until(
        env.console, context,
        std::chrono::seconds(5),
        {{battle_over}}
    );
    if (ret3 == 0) {
        env.log("Ran from battle.");
    } else {
        OperationFailedException::fire(
            env.console, ErrorReport::SEND_ERROR_REPORT,
            "handle_encounter(): Unable to flee from battle."
        );
    }
}

bool ShinyHuntRayquaza::handle_encounter(SingleSwitchProgramEnvironment& env, BotBaseContext& context) {
    ShinyHuntRayquaza_Descriptor::Stats& stats = env.current_stats<ShinyHuntRayquaza_Descriptor::Stats>();

    float shiny_coefficient = 1.0;
    ShinySoundDetector shiny_detector(env.console, [&](float error_coefficient) -> bool{
        shiny_coefficient = error_coefficient;
        return true;
    });
    AdvanceDialogWatcher legendary_appeared(COLOR_YELLOW);

    env.log("Starting battle.");
    pbf_mash_button(context, BUTTON_A, 540);
    context.wait_for_all_requests();

    int res = run_until(
        env.console, context,
        [&](BotBaseContext& context) {
            int ret = wait_until(
                env.console, context,
                std::chrono::seconds(30),
                {{legendary_appeared}}
            );
            if (ret == 0) {
                env.log("Advance arrow detected.");
            } else {
                OperationFailedException::fire(
                    env.console, ErrorReport::SEND_ERROR_REPORT,
                    "handle_encounter(): Did not detect battle start."
                );
            }
            pbf_wait(context, 125);
            context.wait_for_all_requests();
        },
        {{shiny_detector}}
    );
    shiny_detector.throw_if_no_sound();
    if (res == 0){
        env.log("Shiny detected!");
        stats.shinies++;
        env.update_stats();
        send_program_notification(env, NOTIFICATION_SHINY, COLOR_YELLOW, "Shiny found!", {}, "", env.console.video().snapshot(), true);

        return true;
    }
    env.log("Shiny not found.");

    //Send out lead, no shiny detection needed.
    BattleMenuWatcher battle_menu(COLOR_RED);
    env.log("Sending out lead Pokemon.");
    pbf_press_button(context, BUTTON_A, 40, 40);

    int ret = wait_until(
        env.console, context,
        std::chrono::seconds(15),
        {{battle_menu}}
    );
    if (ret == 0) {
        env.log("Battle menu detecteed!");
    } else {
        OperationFailedException::fire(
            env.console, ErrorReport::SEND_ERROR_REPORT,
            "handle_encounter(): Did not detect battle menu."
        );
    }
    pbf_wait(context, 125);
    context.wait_for_all_requests();

    flee_battle(env, context);

    return false;
}

void ShinyHuntRayquaza::program(SingleSwitchProgramEnvironment& env, BotBaseContext& context){
    assert_16_9_720p_min(env.logger(), env.console);
    ShinyHuntRayquaza_Descriptor::Stats& stats = env.current_stats<ShinyHuntRayquaza_Descriptor::Stats>();

    /*
    * Settings: Text Speed fast. Turn off animations (optional)
    * Full screen, no filter.
    * If on a retro handheld, make sure the screen matches that of NSO+.
    * 
    * Setup: Lead is faster or has a Smoke Ball.
    * No abilities or items that activate on entry.
    * Lead cannot be shiny.
    * Stand in front Rayquaza. Save the game.
    * 
    * Emerald only. This uses the Run Away method due to the game's RNG issues.
    * Rayquaza's room is different in Emerald.
    * If powering off your game/resetting, try to start with different timing to avoid repeated frames.
    * 
    * Very small chance that you will encounter a wild pokemon when turning around to head back up the stairs
    * If its shiny it will stop the program.
    */

    while (true) {
        bool legendary_shiny = handle_encounter(env, context);
        if (legendary_shiny) {
            break;
        }
        env.log("No shiny found.");


        //TODO: Handle random Pokenav calls. Why is this a thing?

        //After pressing the flee button, additional dialog box pops up for Rayquaza
        pbf_mash_button(context, BUTTON_B, 250);
        context.wait_for_all_requests();

        //Exit - Bike not allowed. Tried running but its riskier and likely to end up stuck
        ssf_press_button(context, BUTTON_B, 0, 270); //Total hold+release time for below
        pbf_press_dpad(context, DPAD_DOWN, 80, 0); //Face down
        pbf_press_dpad(context, DPAD_LEFT, 30, 0); //Turn left
        pbf_press_dpad(context, DPAD_DOWN, 60, 0); //Face down
        pbf_press_dpad(context, DPAD_RIGHT, 80, 0); //Turn right
        pbf_press_dpad(context, DPAD_UP, 20, 0); //And exit

        //Wait for screen load
        pbf_wait(context, 250);
        context.wait_for_all_requests();

        //Turn around - note that a wild enounter is possible here!
        //Only one tile risk, so no need to loop.
        float shiny_coefficient = 1.0;
        ShinySoundDetector shiny_detector(env.console, [&](float error_coefficient) -> bool{
            shiny_coefficient = error_coefficient;
            return true;
        });
        int res = run_until(
            env.console, context,
            [&](BotBaseContext& context) {
                AdvanceDialogWatcher random_battle(COLOR_YELLOW);







                int res2 = run_until(
                    env.console, context,
                    [&](BotBaseContext& context) {
                        pbf_press_dpad(context, DPAD_UP, 80, 20);
        
                        //Wait for screen load
                        pbf_wait(context, 250);
                        context.wait_for_all_requests();

                        //Reverse
                        ssf_press_button(context, BUTTON_B, 0, 250); //Total hold+release time for below
                        pbf_press_dpad(context, DPAD_LEFT, 80, 0);
                        pbf_press_dpad(context, DPAD_UP, 60, 0);
                        pbf_press_dpad(context, DPAD_RIGHT, 30, 0);
                        pbf_press_dpad(context, DPAD_UP, 80, 0);

                    },
                    {{random_battle}}
                );
                if(res2 == 0) {
                    env.log("Random encounter detected!");
                    flee_battle(env, context);

                    pbf_press_dpad(context, DPAD_UP, 80, 20);
        
                    //Wait for screen load
                    pbf_wait(context, 250);
                    context.wait_for_all_requests();

                    //Reverse
                    ssf_press_button(context, BUTTON_B, 0, 250); //Total hold+release time for below
                    pbf_press_dpad(context, DPAD_LEFT, 80, 0);
                    pbf_press_dpad(context, DPAD_UP, 60, 0);
                    pbf_press_dpad(context, DPAD_RIGHT, 30, 0);
                    pbf_press_dpad(context, DPAD_UP, 80, 0);
                }
            },
            {{shiny_detector}}
        );
        shiny_detector.throw_if_no_sound();
        if (res == 0){
            env.log("Shiny detected!");
            stats.shinies++;
            env.update_stats();
            send_program_notification(env, NOTIFICATION_SHINY, COLOR_YELLOW, "Random shiny found!", {}, "", env.console.video().snapshot(), true);
            break;
        }

        stats.resets++;
        env.update_stats();
    }

    //switch - go home when done

    send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH);
}

}
}
}

