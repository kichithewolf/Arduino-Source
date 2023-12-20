/*  Fixed Kleavor
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#ifndef PokemonAutomation_PokemonSV_ShinyHuntKleavor_H
#define PokemonAutomation_PokemonSV_ShinyHuntKleavor_H

#include "CommonFramework/Notifications/EventNotificationsTable.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "NintendoSwitch/Options/NintendoSwitch_GoHomeWhenDoneOption.h"

namespace PokemonAutomation {
namespace NintendoSwitch {
namespace PokemonSV {

class ShinyHuntKleavor_Descriptor : public SingleSwitchProgramDescriptor {
public:
    ShinyHuntKleavor_Descriptor();

    struct Stats;
    virtual std::unique_ptr<StatsTracker> make_stats() const override;
};

class ShinyHuntKleavor : public SingleSwitchProgramInstance {
public:
    ShinyHuntKleavor();
    virtual void program(SingleSwitchProgramEnvironment& env, BotBaseContext& context) override;

private:
    GoHomeWhenDoneOption GO_HOME_WHEN_DONE;
    EventNotificationOption NOTIFICATION_STATUS_UPDATE;
    EventNotificationsOption NOTIFICATIONS;
};

}
}
}
#endif
