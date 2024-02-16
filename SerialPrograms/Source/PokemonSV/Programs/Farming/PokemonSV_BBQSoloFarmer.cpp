/*  BBQ Solo Farmer
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#include "Common/Cpp/PrettyPrint.h"
#include "CommonFramework/InferenceInfra/InferenceRoutines.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/Tools/StatsTracking.h"
#include "CommonFramework/Tools/VideoResolutionCheck.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_Superscalar.h"
#include "NintendoSwitch/NintendoSwitch_Settings.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "PokemonSV/Programs/PokemonSV_GameEntry.h"
#include "PokemonSV/Programs/PokemonSV_SaveGame.h"
#include "PokemonSV/Programs/PokemonSV_Navigation.h"
#include "Pokemon/Pokemon_Strings.h"
#include "PokemonSV/PokemonSV_Settings.h"
#include "PokemonSwSh/Commands/PokemonSwSh_Commands_DateSpam.h"
#include "PokemonSV/Inference/Dialogs/PokemonSV_DialogDetector.h"
#include "PokemonSV/Inference/Overworld/PokemonSV_OverworldDetector.h"
#include "PokemonSV/Inference/Battles/PokemonSV_NormalBattleMenus.h"
#include "PokemonSV/Programs/PokemonSV_BlueberryQuests.h"
#include "PokemonSV_BBQSoloFarmer.h"

namespace PokemonAutomation {
namespace NintendoSwitch {
namespace PokemonSV {

using namespace Pokemon;
 
BBQSoloFarmer_Descriptor::BBQSoloFarmer_Descriptor()
    : SingleSwitchProgramDescriptor(
        "PokemonSV:BBQSoloFarmer",
        STRING_POKEMON + " SV", "BBQ Farmer",
        "ComputerControl/blob/master/Wiki/Programs/PokemonSV/BBQSoloFarmer.md",
        "Farm Blueberry Quests in the Terarium.",
        FeedbackType::REQUIRED,
        AllowCommandsWhenRunning::DISABLE_COMMANDS,
        PABotBaseLevel::PABOTBASE_12KB
    )
{}
struct BBQSoloFarmer_Descriptor::Stats : public StatsTracker {
    Stats()
        : questsCompleted(m_stats["Quests Completed"])
        , saves(m_stats["Saves"])
        , errors(m_stats["Errors"])
    {
        m_display_order.emplace_back("Quests Completed");
        m_display_order.emplace_back("Saves");
        m_display_order.emplace_back("Errors", HIDDEN_IF_ZERO);
    }
    std::atomic<uint64_t>& questsCompleted;
    std::atomic<uint64_t>& saves;
    std::atomic<uint64_t>& errors;
};
std::unique_ptr<StatsTracker> BBQSoloFarmer_Descriptor::make_stats() const {
    return std::unique_ptr<StatsTracker>(new Stats());
}
BBQSoloFarmer::BBQSoloFarmer()
    : GO_HOME_WHEN_DONE(true)
    , NOTIFICATION_STATUS_UPDATE("Status Update", true, false, std::chrono::seconds(3600))
    , NOTIFICATIONS({
        &NOTIFICATION_STATUS_UPDATE,
        & NOTIFICATION_PROGRAM_FINISH,
        & NOTIFICATION_ERROR_FATAL,
        })
{
    PA_ADD_OPTION(BBQ_OPTIONS);
    PA_ADD_OPTION(GO_HOME_WHEN_DONE);
    PA_ADD_OPTION(NOTIFICATIONS);
}

void BBQSoloFarmer::program(SingleSwitchProgramEnvironment& env, BotBaseContext& context) {
    assert_16_9_720p_min(env.logger(), env.console);
    BBQSoloFarmer_Descriptor::Stats& stats = env.current_stats<BBQSoloFarmer_Descriptor::Stats>();

    /*
    start in the terarium at central plaza - we return to central plaza after every task
    must have certain fly points unlocked
    must have flying unlocked, must have completed the DLC storyline (caught turtle), this is needed for tera orb recharge and flying.
    for quests that cannot be done, reroll.

    smoke ball or flying pokemon required in slot 1 in case of arena trap
    first attack must be capable of killing all types (so no normal/fighting/etc in case of ghost/dark/etc.)
    last attack can be nonlethal for capture quests. not required though.

    handle out of bp rerolls? can't test this atm.

    camera support OFF
    camera controls regular
    date and time unsynced as this relies heavily on date skipping
    */
    
    //Fly to plaza
    //open_map_from_overworld(env.program_info(), env.console, context);
    //fly_to_overworld_from_map(env.program_info(), env.console, context);
    /*
    //Get initial BP
    int starting_BP = read_BP(env.program_info(), env.console, context);
    if (starting_BP < 100) {
        env.log("Starting BP is low."); //Todo throw error? migh tnot have enough for rerolls
    }
    */

    std::vector<BBQuests> quest_list; //all quests
    std::vector<BBQuests> quests_to_do; //do-able quests
    uint64_t eggs_hatched = 0; //Track eggs
    uint64_t num_completed_quests = 0;

    BBQuests test_quest = BBQuests::catch_ghost;
    bool questTest = process_and_do_quest(env.program_info(), env.console, context, BBQ_OPTIONS, test_quest, eggs_hatched);
    if (questTest) {
        env.log("Success");
    }

    while (num_completed_quests < BBQ_OPTIONS.NUM_QUESTS) {
        //Get and reroll quests until we can at least one
        while (quests_to_do.size() < 1) {
            quest_list = read_quests(env.program_info(), env.console, context, BBQ_OPTIONS);
            quests_to_do = process_quest_list(env.program_info(), env.console, context, BBQ_OPTIONS, quest_list, eggs_hatched);

            //Clear out the regular quest list.
            quest_list.clear();
        }

        for (auto current_quest : quests_to_do) {
            //Check if quest was already completed (ex. 500 meters completed while navigating to take a photo)
            quest_list = read_quests(env.program_info(), env.console, context, BBQ_OPTIONS);
            if (std::find(quest_list.begin(), quest_list.end(), current_quest) != quest_list.end()) {
                env.log("Current quest exists on list. Doing quest.");
                bool questSuccess = process_and_do_quest(env.program_info(), env.console, context, BBQ_OPTIONS, current_quest, eggs_hatched);
                if (questSuccess) {
                    env.log("Quest completed successfully.");
                    num_completed_quests++;
                    stats.questsCompleted++;
                    env.update_stats();
                }
                else {
                    env.log("Quest did not complete successfully.");
                }
            }
            else {
                //Note: This doesn't account for case such as "sneak up" being added and then completed alongside the next quest
                env.log("Current quest does not exist on list. Quest completed at some point.");
                num_completed_quests++;
                stats.questsCompleted++;
                env.update_stats();
            }
            quest_list.clear();
        }
        //Clear out the todo list
        quests_to_do.clear();

        /*
        //CHECK THAT BP WAS EARNED BEFORE SAVING
        if (SAVE_NUM_QUESTS != 0 && i % SAVE_NUM_ROUNDS == 0) {
            env.log("Saving and resetting.");
            save_game_from_overworld(env.program_info(), env.console, context);
            reset_game(env.program_info(), env.console, context);
            stats.m_saves++;
        }
        env.update_stats();
        send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE);
        */
    }
    env.update_stats();

    if (BBQ_OPTIONS.FIX_TIME_WHEN_DONE) {
        pbf_press_button(context, BUTTON_HOME, 10, GameSettings::instance().GAME_TO_HOME_DELAY);
        home_to_date_time(context, false, false);
        pbf_press_button(context, BUTTON_A, 20, 105);
        pbf_press_button(context, BUTTON_A, 20, 105);
        pbf_press_button(context, BUTTON_HOME, 20, ConsoleSettings::instance().SETTINGS_TO_HOME_DELAY);
        resume_game_from_home(env.console, context);
    }

    GO_HOME_WHEN_DONE.run_end_of_program(context);
    send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH);
}
    
}
}
}
