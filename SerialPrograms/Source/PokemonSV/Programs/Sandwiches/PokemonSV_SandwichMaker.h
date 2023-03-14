/*  Sandwich Maker
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#ifndef PokemonAutomation_PokemonSwSh_SandwichMaker_H
#define PokemonAutomation_PokemonSwSh_SandwichMaker_H

#include "Common/Cpp/Options/SimpleIntegerOption.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "NintendoSwitch/Options/NintendoSwitch_GoHomeWhenDoneOption.h"
#include "CommonFramework/Notifications/EventNotificationsTable.h"
#include "Common/Cpp/Options/BooleanCheckBoxOption.h"
#include "PokemonSV/Options/PokemonSV_SandwichMakerOption.h"

namespace PokemonAutomation {
namespace NintendoSwitch {
namespace PokemonSV {


class SandwichMaker_Descriptor : public SingleSwitchProgramDescriptor {
public:
    SandwichMaker_Descriptor();
};

class SandwichMaker : public SingleSwitchProgramInstance {
public:
    SandwichMaker();

    virtual void program(SingleSwitchProgramEnvironment& env, BotBaseContext& context) override;

private:
    SandwichMakerOption EGG_SANDWICH;
    SimpleIntegerOption<uint32_t> SKIPS;
    GoHomeWhenDoneOption GO_HOME_WHEN_DONE;
    BooleanCheckBoxOption FIX_TIME_WHEN_DONE;
    EventNotificationsOption NOTIFICATIONS;
};


}
}
}
#endif



