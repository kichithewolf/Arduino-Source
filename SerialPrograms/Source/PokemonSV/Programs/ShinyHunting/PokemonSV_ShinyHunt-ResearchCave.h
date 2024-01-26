/*  Shiny Hunt - Research Cave
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#ifndef PokemonAutomation_PokemonSV_ShinyHuntResearchCave_H
#define PokemonAutomation_PokemonSV_ShinyHuntResearchCave_H

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


class ShinyHuntResearchCave_Descriptor : public SingleSwitchProgramDescriptor {
public:
    ShinyHuntResearchCave_Descriptor();

    struct Stats;
    virtual std::unique_ptr<StatsTracker> make_stats() const override;
};


class ShinyHuntResearchCave : public SingleSwitchProgramInstance, public ConfigOption::Listener {
public:
    ~ShinyHuntResearchCave();
    ShinyHuntResearchCave();

    virtual void program(SingleSwitchProgramEnvironment& env, BotBaseContext& context) override;

private:
    virtual std::string check_validity() const override;
    virtual void value_changed() override;

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

    EventNotificationOption NOTIFICATION_STATUS_UPDATE;
    EventNotificationsOption NOTIFICATIONS;

    LetsGoEncounterBotTracker* m_encounter_tracker;

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
