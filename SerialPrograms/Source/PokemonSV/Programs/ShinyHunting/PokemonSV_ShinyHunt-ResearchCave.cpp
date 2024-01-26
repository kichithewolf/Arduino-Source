/*  Shiny Hunt - Research Cave
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#include <atomic>
#include <sstream>
#include "Common/Cpp/PrettyPrint.h"
#include "CommonFramework/GlobalSettingsPanel.h"
#include "CommonFramework/Exceptions/ProgramFinishedException.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/InferenceInfra/InferenceRoutines.h"
#include "CommonFramework/Tools/StatsTracking.h"
#include "CommonFramework/Tools/VideoResolutionCheck.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "Pokemon/Pokemon_Strings.h"
#include "PokemonSV/Inference/Overworld/PokemonSV_LetsGoHpReader.h"
#include "PokemonSV/Inference/Boxes/PokemonSV_IvJudgeReader.h"
#include "PokemonSV/Inference/Battles/PokemonSV_EncounterWatcher.h"
#include "PokemonSV/Programs/PokemonSV_GameEntry.h"
#include "PokemonSV/Programs/PokemonSV_Navigation.h"
#include "PokemonSV/Programs/PokemonSV_SaveGame.h"
#include "PokemonSV/Programs/Battles/PokemonSV_Battles.h"
#include "PokemonSV/Programs/Sandwiches/PokemonSV_SandwichRoutines.h"
#include "PokemonSV_LetsGoTools.h"
#include "PokemonSV_ShinyHunt-ResearchCave.h"

namespace PokemonAutomation {
namespace NintendoSwitch {
namespace PokemonSV {

using namespace Pokemon;

ShinyHuntResearchCave_Descriptor::ShinyHuntResearchCave_Descriptor()
    : SingleSwitchProgramDescriptor(
        "PokemonSV:ShinyHuntResearchCave",
        STRING_POKEMON + " SV", "Shiny Hunt - Research Cave",
        "ComputerControl/blob/master/Wiki/Programs/PokemonSV/ShinyHunt-ResearchCave.md",
        "Shiny hunt in the cave outside Research Station 1.",
        FeedbackType::REQUIRED,
        AllowCommandsWhenRunning::DISABLE_COMMANDS,
        PABotBaseLevel::PABOTBASE_12KB
    )
{}
struct ShinyHuntResearchCave_Descriptor::Stats : public LetsGoEncounterBotStats {
    Stats()
        : m_sandwiches(m_stats["Sandwiches"])
        , m_autoheals(m_stats["Auto Heals"])
        , m_game_resets(m_stats["Game Resets"])
        , m_errors(m_stats["Errors"])
    {
        m_display_order.insert(m_display_order.begin() + 2, { "Sandwiches", HIDDEN_IF_ZERO });
        m_display_order.insert(m_display_order.begin() + 3, { "Auto Heals", HIDDEN_IF_ZERO });
        m_display_order.insert(m_display_order.begin() + 4, { "Game Resets", HIDDEN_IF_ZERO });
        m_display_order.insert(m_display_order.begin() + 5, { "Errors", HIDDEN_IF_ZERO });
    }
    std::atomic<uint64_t>& m_sandwiches;
    std::atomic<uint64_t>& m_autoheals;
    std::atomic<uint64_t>& m_game_resets;
    std::atomic<uint64_t>& m_errors;
};
std::unique_ptr<StatsTracker> ShinyHuntResearchCave_Descriptor::make_stats() const {
    return std::unique_ptr<StatsTracker>(new Stats());
}

ShinyHuntResearchCave::~ShinyHuntResearchCave(){
    MODE.remove_listener(*this);
}

ShinyHuntResearchCave::ShinyHuntResearchCave()
    : LANGUAGE(
          "<b>Game Language:</b><br>Required to read " + STRING_POKEMON + " names.",
          IV_READER().languages(),
          LockMode::LOCK_WHILE_RUNNING,
          false
          )
    , MODE(
        "<b>Mode:</b><br>"
        "If starting in the cave, you should stand near the center of the cave facing any direction.<br>"
        "If starting in the Zero Gate, you should be just inside the building as if you just entered."
        "<br>If making a sandwich, you should be at the Zero Gate fly spot as if you just flew there.",
        {
         {Mode::START_IN_CAVE,   "platform", "Start already inside the cave."},
         {Mode::START_IN_ZERO_GATE,  "zerogate", "Start inside Zero Gate."},
         {Mode::MAKE_SANDWICH,       "sandwich", "Make a sandwich."},
         },
        LockMode::LOCK_WHILE_RUNNING,
        Mode::MAKE_SANDWICH
        )

    , SANDWICH_RESET_IN_MINUTES(
        "<b>Sandwich Reset Time (in minutes):</b><br>The time to reset game to make a new sandwich.",
        LockMode::UNLOCK_WHILE_RUNNING,
        35
    )
    , SANDWICH_OPTIONS(LANGUAGE)
    , GO_HOME_WHEN_DONE(true)
    , AUTO_HEAL_PERCENT(
        "<b>Auto-Heal %</b><br>Auto-heal if your HP drops below this percentage.",
        LockMode::UNLOCK_WHILE_RUNNING,
        75, 0, 100
    )
    , NOTIFICATION_STATUS_UPDATE("Status Update", true, false, std::chrono::seconds(3600))
    , NOTIFICATIONS({
        &NOTIFICATION_STATUS_UPDATE,
        &ENCOUNTER_BOT_OPTIONS.NOTIFICATION_NONSHINY,
        &ENCOUNTER_BOT_OPTIONS.NOTIFICATION_SHINY,
        &ENCOUNTER_BOT_OPTIONS.NOTIFICATION_CATCH_SUCCESS,
        &ENCOUNTER_BOT_OPTIONS.NOTIFICATION_CATCH_FAILED,
        &NOTIFICATION_PROGRAM_FINISH,
        &NOTIFICATION_ERROR_RECOVERABLE,
        &NOTIFICATION_ERROR_FATAL,
        })
{
    PA_ADD_OPTION(LANGUAGE);
    PA_ADD_OPTION(MODE);
    PA_ADD_OPTION(SANDWICH_RESET_IN_MINUTES);
    PA_ADD_OPTION(SANDWICH_OPTIONS);
    PA_ADD_OPTION(ENCOUNTER_BOT_OPTIONS);
    PA_ADD_OPTION(GO_HOME_WHEN_DONE);
    PA_ADD_OPTION(AUTO_HEAL_PERCENT);
    PA_ADD_OPTION(NOTIFICATIONS);

    ShinyHuntResearchCave::value_changed();

    MODE.add_listener(*this);
}

std::string ShinyHuntResearchCave::check_validity() const{
    std::string error = SingleSwitchProgramInstance::check_validity();
    if (!error.empty()){
        return error;
    }
    if (LANGUAGE == Language::None && MODE == Mode::MAKE_SANDWICH){
        return "Sandwich mode requires selecting a language.";
    }
    return "";
}

void ShinyHuntResearchCave::value_changed(){
    ConfigOptionState state = MODE == Mode::MAKE_SANDWICH
        ? ConfigOptionState::ENABLED
        : ConfigOptionState::HIDDEN;
    SANDWICH_RESET_IN_MINUTES.set_visibility(state);
    SANDWICH_OPTIONS.set_visibility(state);
}


void ShinyHuntResearchCave::program(SingleSwitchProgramEnvironment& env, BotBaseContext& context) {
    ShinyHuntResearchCave_Descriptor::Stats& stats = env.current_stats<ShinyHuntResearchCave_Descriptor::Stats>();

    stats.m_game_resets++;

}





}
}
}
