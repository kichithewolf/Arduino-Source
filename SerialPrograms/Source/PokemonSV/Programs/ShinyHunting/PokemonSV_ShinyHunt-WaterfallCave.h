/*  Shiny Hunt - Waterfall Cave
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#ifndef PokemonAutomation_PokemonSV_ShinyHuntWaterfallCave_H
#define PokemonAutomation_PokemonSV_ShinyHuntWaterfallCave_H

#include <functional>
#include "Common/Cpp/Options/FloatingPointOption.h"
#include "Common/Cpp/Options/EnumDropdownOption.h"
#include "CommonFramework/Options/LanguageOCROption.h"
#include "CommonFramework/Notifications/EventNotificationsTable.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "NintendoSwitch/Options/NintendoSwitch_GoHomeWhenDoneOption.h"
#include "PokemonSV/Options/PokemonSV_EncounterBotCommon.h"
#include "PokemonSV/Options/PokemonSV_SandwichMakerOption.h"

namespace PokemonAutomation {
namespace NintendoSwitch {
namespace PokemonSV {

class LetsGoHpWatcher;
class DiscontiguousTimeTracker;
class LetsGoEncounterBotTracker;


class ShinyHuntWaterfallCave_Descriptor : public SingleSwitchProgramDescriptor {
public:
    ShinyHuntWaterfallCave_Descriptor();

    struct Stats;
    virtual std::unique_ptr<StatsTracker> make_stats() const override;
};


class ShinyHuntWaterfallCave : public SingleSwitchProgramInstance, public ConfigOption::Listener {
public:
    ~ShinyHuntWaterfallCave();
    ShinyHuntWaterfallCave();

    virtual void program(SingleSwitchProgramEnvironment& env, BotBaseContext& context) override;

private:
    virtual std::string check_validity() const override;
    virtual void value_changed() override;

    enum class Location{
        UNKNOWN,
        ZERO_GATE_FLY_SPOT,
        ZERO_GATE_INSIDE,
        TRAVELING_TO_CAVE,
        AREA_ZERO_CAVE,
    };

    void inside_zero_gate_to_cave(const ProgramInfo& info, ConsoleHandle& console, BotBaseContext& context);
    void run_path(ProgramEnvironment& env, ConsoleHandle& console, BotBaseContext& context,
    LetsGoEncounterBotTracker& tracker,
    uint64_t iteration_count);

    void run_state(SingleSwitchProgramEnvironment& env, BotBaseContext& context);
    bool run_traversal(BotBaseContext& context);
    void set_flags_and_run_state(SingleSwitchProgramEnvironment& env, BotBaseContext& context);

    OCR::LanguageOCROption LANGUAGE;

    enum class Mode{
        START_IN_CAVE,
        START_IN_ZERO_GATE,
        MAKE_SANDWICH,
    };
    EnumDropdownOption<Mode> MODE;

    SimpleIntegerOption<uint16_t> SANDWICH_RESET_IN_MINUTES;
    SandwichMakerOption SANDWICH_OPTIONS;

    EncounterBotCommonOptions ENCOUNTER_BOT_OPTIONS;
    GoHomeWhenDoneOption GO_HOME_WHEN_DONE;

    FloatingPointOption AUTO_HEAL_PERCENT;
    BooleanCheckBoxOption HEAL_AT_STATION;
    SimpleIntegerOption<uint16_t> STATION_ARRIVE_PAUSE_SECONDS;
    TimeExpressionOption<uint16_t> MIDAIR_PAUSE_TIME;

    BooleanCheckBoxOption INVERTED_FLIGHT;

    EventNotificationOption NOTIFICATION_STATUS_UPDATE;
    EventNotificationsOption NOTIFICATIONS;

    SingleSwitchProgramEnvironment* m_env;

    LetsGoHpWatcher* m_hp_watcher;
    DiscontiguousTimeTracker* m_time_tracker;
    LetsGoEncounterBotTracker* m_encounter_tracker;

    uint64_t m_iterations = 0;
    Location m_current_location;
    Location m_saved_location;

    //  Set to true if we should save on the first available opportunity.
    bool m_pending_save;
    bool m_pending_platform_reset;
    bool m_pending_sandwich;
    bool m_reset_on_next_sandwich;

    WallClock m_last_sandwich;

    size_t m_consecutive_failures;

};

}
}
}
#endif
