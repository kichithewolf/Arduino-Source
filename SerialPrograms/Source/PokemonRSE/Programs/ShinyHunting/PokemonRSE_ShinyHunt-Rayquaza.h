/*  E Shiny Rayquaza
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#ifndef PokemonAutomation_PokemonRSE_ShinyHuntRayquaza_H
#define PokemonAutomation_PokemonRSE_ShinyHuntRayquaza_H

#include "Common/Cpp/Options/SimpleIntegerOption.h"
#include "Common/Cpp/Options/TimeExpressionOption.h"
#include "CommonFramework/Notifications/EventNotificationsTable.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonRSE{

class ShinyHuntRayquaza_Descriptor : public SingleSwitchProgramDescriptor{
public:
    ShinyHuntRayquaza_Descriptor();
    struct Stats;
    virtual std::unique_ptr<StatsTracker> make_stats() const override;
};

class ShinyHuntRayquaza : public SingleSwitchProgramInstance{
public:
    ShinyHuntRayquaza();
    virtual void program(SingleSwitchProgramEnvironment& env, BotBaseContext& context) override;

private:
    EventNotificationOption NOTIFICATION_SHINY;
    EventNotificationOption NOTIFICATION_STATUS_UPDATE;
    EventNotificationsOption NOTIFICATIONS;

    void flee_battle(SingleSwitchProgramEnvironment& env, BotBaseContext& context);
    bool handle_encounter(SingleSwitchProgramEnvironment& env, BotBaseContext& context);
};

}
}
}
#endif



