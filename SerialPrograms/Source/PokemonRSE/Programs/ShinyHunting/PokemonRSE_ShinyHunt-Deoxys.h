/*  E Shiny Deoxys
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#ifndef PokemonAutomation_PokemonRSE_ShinyHuntDeoxys_H
#define PokemonAutomation_PokemonRSE_ShinyHuntDeoxys_H

#include "Common/Cpp/Options/SimpleIntegerOption.h"
#include "Common/Cpp/Options/TimeExpressionOption.h"
#include "CommonFramework/Notifications/EventNotificationsTable.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonRSE{

class ShinyHuntDeoxys_Descriptor : public SingleSwitchProgramDescriptor{
public:
    ShinyHuntDeoxys_Descriptor();
    struct Stats;
    virtual std::unique_ptr<StatsTracker> make_stats() const override;
};

class ShinyHuntDeoxys : public SingleSwitchProgramInstance{
public:
    ShinyHuntDeoxys();
    virtual void program(SingleSwitchProgramEnvironment& env, BotBaseContext& context) override;

private:
    TimeExpressionOption<uint16_t> WALK_UP_DOWN_TIME;

    EventNotificationOption NOTIFICATION_SHINY;
    EventNotificationOption NOTIFICATION_STATUS_UPDATE;
    EventNotificationsOption NOTIFICATIONS;

    bool walk_down(SingleSwitchProgramEnvironment& env, BotBaseContext& context);
    void flee_battle(SingleSwitchProgramEnvironment& env, BotBaseContext& context);
    bool handle_encounter(SingleSwitchProgramEnvironment& env, BotBaseContext& context);
    void solve_puzzle(SingleSwitchProgramEnvironment& env, BotBaseContext& context);
    bool walk_up(SingleSwitchProgramEnvironment& env, BotBaseContext& context);
};

}
}
}
#endif

