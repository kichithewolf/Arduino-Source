/*  E Shiny Deoxys
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
#include "PokemonRSE_ShinyHunt-Deoxys.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonRSE{

ShinyHuntDeoxys_Descriptor::ShinyHuntDeoxys_Descriptor()
    : SingleSwitchProgramDescriptor(
        "PokemonRSE:ShinyHuntDeoxys",
        "Pokemon RSE", "Shiny Hunt - Deoxys",
        "ComputerControl/blob/master/Wiki/Programs/PokemonRSE/ShinyHuntDeoxys.md",
        "Use the Run Away method to shiny hunt Deoxys in Emerald.",
        FeedbackType::VIDEO_AUDIO_GBA,
        AllowCommandsWhenRunning::DISABLE_COMMANDS,
        PABotBaseLevel::PABOTBASE_12KB
    )
{}

struct ShinyHuntDeoxys_Descriptor::Stats : public StatsTracker{
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
std::unique_ptr<StatsTracker> ShinyHuntDeoxys_Descriptor::make_stats() const{
    return std::unique_ptr<StatsTracker>(new Stats());
}

ShinyHuntDeoxys::ShinyHuntDeoxys()
    : WALK_UP_DOWN_TIME(
        "<b>Walk up/down time</b><br>Spend this long to walk up to the triangle rock.",
        LockMode::LOCK_WHILE_RUNNING,
        TICKS_PER_SECOND,
        "5 * TICKS_PER_SECOND"
    )
    , NOTIFICATION_SHINY(
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
    PA_ADD_OPTION(WALK_UP_DOWN_TIME);
    PA_ADD_OPTION(NOTIFICATIONS);
}

bool ShinyHuntDeoxys::walk_down(SingleSwitchProgramEnvironment& env, BotBaseContext& context) {
    //Walk up to the triangle rock from the ship. No bike allowed.
    AdvanceDialogWatcher run_up_call(COLOR_ORANGE);
    int res = run_until(
        env.console, context,
        [&](BotBaseContext& context) {
            pbf_press_dpad(context, DPAD_DOWN, WALK_UP_DOWN_TIME, 20);
            context.wait_for_all_requests();
        },
        {{run_up_call}}
    );
    if (res == 0) {
        env.log("PokeNav call detected. Ending call.");
        pbf_mash_button(context, BUTTON_B, 375);
        context.wait_for_all_requests();
        return false;
    }
    return true;
}

void ShinyHuntDeoxys::flee_battle(SingleSwitchProgramEnvironment& env, BotBaseContext& context) {
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

bool ShinyHuntDeoxys::handle_encounter(SingleSwitchProgramEnvironment& env, BotBaseContext& context) {
    ShinyHuntDeoxys_Descriptor::Stats& stats = env.current_stats<ShinyHuntDeoxys_Descriptor::Stats>();

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

void ShinyHuntDeoxys::solve_puzzle(SingleSwitchProgramEnvironment& env, BotBaseContext& context) {
    AdvanceDialogWatcher puzzle_solve_call(COLOR_ORANGE);

    int res = run_until(
        env.console, context,
        [&](BotBaseContext& context) {
            env.log("Step 1: Press A from below.");
            pbf_press_button(context, BUTTON_A, 20, 40);

            env.log("Step 2: 5 Left, 1 Down.");
            pbf_press_dpad(context, DPAD_LEFT, 10, 80);
            pbf_press_dpad(context, DPAD_LEFT, 10, 80);
            pbf_press_dpad(context, DPAD_LEFT, 10, 80);
            pbf_press_dpad(context, DPAD_LEFT, 10, 80);
            pbf_press_dpad(context, DPAD_LEFT, 10, 80);

            pbf_press_dpad(context, DPAD_DOWN, 10, 80);

            pbf_press_button(context, BUTTON_A, 20, 40);
            context.wait_for_all_requests();

            env.log("Step 3: 5 Right, 5 Up.");
            pbf_press_dpad(context, DPAD_RIGHT, 10, 80);
            pbf_press_dpad(context, DPAD_RIGHT, 10, 80);
            pbf_press_dpad(context, DPAD_RIGHT, 10, 80);
            pbf_press_dpad(context, DPAD_RIGHT, 10, 80);
            pbf_press_dpad(context, DPAD_RIGHT, 10, 80);

            pbf_press_dpad(context, DPAD_UP, 125, 80);

            pbf_press_button(context, BUTTON_A, 20, 40);
            context.wait_for_all_requests();

            env.log("Step 4: 5 Right, 5 Down");
            pbf_press_dpad(context, DPAD_RIGHT, 10, 80);
            pbf_press_dpad(context, DPAD_RIGHT, 10, 80);
            pbf_press_dpad(context, DPAD_RIGHT, 10, 80);
            pbf_press_dpad(context, DPAD_RIGHT, 10, 80);
            pbf_press_dpad(context, DPAD_RIGHT, 10, 80);

            pbf_press_dpad(context, DPAD_DOWN, 125, 80);

            pbf_press_button(context, BUTTON_A, 20, 40);
            context.wait_for_all_requests();

            env.log("Step 5: 3 Up, 7 Left");
            pbf_press_dpad(context, DPAD_UP, 10, 80);
            pbf_press_dpad(context, DPAD_UP, 10, 80);
            pbf_press_dpad(context, DPAD_UP, 10, 80);

            pbf_press_dpad(context, DPAD_LEFT, 200, 80);

            pbf_press_button(context, BUTTON_A, 20, 40);
            context.wait_for_all_requests();

            env.log("Step 6: 5 Right.");
            pbf_press_dpad(context, DPAD_RIGHT, 125, 80);

            pbf_press_button(context, BUTTON_A, 20, 40);
            context.wait_for_all_requests();

            env.log("Step 7: 3 Left, 2 Down.");
            pbf_press_dpad(context, DPAD_LEFT, 10, 80);
            pbf_press_dpad(context, DPAD_LEFT, 10, 80);
            pbf_press_dpad(context, DPAD_LEFT, 10, 80);

            pbf_press_dpad(context, DPAD_DOWN, 80, 80);

            pbf_press_button(context, BUTTON_A, 20, 40);
            context.wait_for_all_requests();

            env.log("Step 8: 1 Down, 4 Left.");
            pbf_press_dpad(context, DPAD_DOWN, 10, 80);

            pbf_press_dpad(context, DPAD_LEFT, 125, 80);

            pbf_press_button(context, BUTTON_A, 20, 40);
            context.wait_for_all_requests();

            env.log("Step 8: 7 Right.");
            pbf_press_dpad(context, DPAD_RIGHT, 200, 80);

            pbf_press_button(context, BUTTON_A, 20, 40);
            context.wait_for_all_requests();

            env.log("Step 9: 4 Left, Down 1.");
            pbf_press_dpad(context, DPAD_LEFT, 10, 80);
            pbf_press_dpad(context, DPAD_LEFT, 10, 80);
            pbf_press_dpad(context, DPAD_LEFT, 10, 80);
            pbf_press_dpad(context, DPAD_LEFT, 10, 80);

            pbf_press_dpad(context, DPAD_DOWN, 10, 80);

            pbf_press_button(context, BUTTON_A, 20, 40);
            context.wait_for_all_requests();

            env.log("Step 10: 4 Up.");
            pbf_press_dpad(context, DPAD_UP, 125, 80);
            context.wait_for_all_requests();
        },
        {{puzzle_solve_call}}
    );
    if (res == 0) {
        env.log("PokeNav call detected. Ending call.");
        pbf_mash_button(context, BUTTON_B, 375);
        context.wait_for_all_requests();
        return false;
    }
    return true;



}

bool ShinyHuntDeoxys::walk_up(SingleSwitchProgramEnvironment& env, BotBaseContext& context) {
    //Walk up to the triangle rock from the ship. No bike allowed.
    AdvanceDialogWatcher run_up_call(COLOR_ORANGE);
    int res = run_until(
        env.console, context,
        [&](BotBaseContext& context) {
            pbf_press_dpad(context, DPAD_UP, WALK_UP_DOWN_TIME, 20);
            context.wait_for_all_requests();
        },
        {{run_up_call}}
    );
    if (res == 0) {
        env.log("PokeNav call detected. Ending call.");
        pbf_mash_button(context, BUTTON_B, 375);
        context.wait_for_all_requests();
        return false;
    }
    return true;
}

void ShinyHuntDeoxys::program(SingleSwitchProgramEnvironment& env, BotBaseContext& context){
    assert_16_9_720p_min(env.logger(), env.console);
    ShinyHuntDeoxys_Descriptor::Stats& stats = env.current_stats<ShinyHuntDeoxys_Descriptor::Stats>();

    /*
    * Settings: Text Speed fast. Turn off animations.
    * Full screen, no filter.
    * If on a retro handheld, make sure the screen matches that of NSO+.
    * 
    * Setup: Lead is faster or has a Smoke Ball.
    * No abilities or items that activate on entry.
    * Lead cannot be shiny.
    * Stand enter Birth Island and stay at the door. Save the game.
    * 
    * Emerald only. This uses the Run Away method due to the game's RNG issues.
    * If powering off your game/resetting, try to start with different timing to avoid repeated frames.
    * 
    * Have to handle potentail random PokeNav calls. Why is that a thing?
    */

    while (true) {
        bool successful_walk_up = walk_up(env, context);
        while (!successful_walk_up) {
            walk_up(env, context);
        }

        bool successful_puzzle = solve_puzzle(env, context);
        while (!successful_puzzle) {
            //reset_puzzle() left 2 down lots then align to a tree. then turn back and find the steps.
            solve_puzzle(env, context);
        }

        //Start battle. We start outside of solve_puzzle in case there's a PokeNav call on the very last step.
        pbf_press_button(context, BUTTON_A, 20, 40);

        bool legendary_shiny = handle_encounter(env, context);
        if (legendary_shiny) {
            break;
        }
        env.log("No shiny found.");

        //After pressing the flee button, additional dialog box pops up for Deoxys
        pbf_mash_button(context, BUTTON_B, 250);
        context.wait_for_all_requests();

        bool successful_walk_down = walk_down(env, context);
        while (!successful_walk_down) {
            walk_down(env, context);
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
