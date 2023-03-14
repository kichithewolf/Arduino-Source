/*  Sandwich Makter Option
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#ifndef PokemonAutomation_PokemonSV_SandwichMakerOption_H
#define PokemonAutomation_PokemonSV_SandwichMakerOption_H

#include "CommonFramework/Options/StringSelectOption.h"
#include "Common/Cpp/Options/GroupOption.h"
#include "Common/Cpp/Options/EnumDropdownOption.h"
#include "CommonFramework/Options/StringSelectOption.h"
#include "Common/Cpp/Options/SimpleIntegerOption.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{

class SandwichRecipeSelectCell : public StringSelectCell {
public:
    SandwichRecipeSelectCell(const std::string& default_slug = "");
};

class SandwichRecipeSelectOption : public StringSelectOption {
public:
    SandwichRecipeSelectOption(
        std::string label,
        LockWhileRunning lock_while_running,
        const std::string& default_slug = ""
    );
};

class SandwichMakerOption : public GroupOption {

public:
    SandwichMakerOption();

private:
    SimpleIntegerOption<uint64_t> MAX_NUM_SANDWICHES;
    PokemonSV::SandwichRecipeSelectOption SANDWICH_TYPE;

};

}
}
}
#endif
