/*  Sandwich Maker Option
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#include <map>
#include "PokemonSV/Resources/PokemonSV_SandwichRecipes.h"
#include "PokemonSV_SandwichMakerOption.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{


StringSelectDatabase make_all_sandwiches_database() {
    StringSelectDatabase ret;
    for (const auto& slug : SANDWICH_RECIPE_SLUGS()) {
        const SandwichRecipes& data = get_sandwich_recipe(slug);
        ret.add_entry(StringSelectEntry(slug, data.display_name()));
    }
    return ret;
}
const StringSelectDatabase& ALL_SANDWICHES_SELECT_DATABASE() {
    static StringSelectDatabase database = make_all_sandwiches_database();
    return database;
}

SandwichRecipeSelectCell::SandwichRecipeSelectCell(
    const std::string& default_slug
)
    : StringSelectCell(
        ALL_SANDWICHES_SELECT_DATABASE(),
        LockWhileRunning::LOCKED,
        default_slug
    )
{}

SandwichRecipeSelectOption::SandwichRecipeSelectOption(
    std::string label,
    LockWhileRunning lock_while_running,
    const std::string& default_slug
)
    : StringSelectOption(
        std::move(label),
        ALL_SANDWICHES_SELECT_DATABASE(),
        lock_while_running,
        default_slug
    )
{}

SandwichMakerOption::SandwichMakerOption()
    : GroupOption(
        "Sandwich Maker",
        LockWhileRunning::LOCKED,
        false, true
    )
    , MAX_NUM_SANDWICHES(
        "<b>Max Sandwiches:</b><br>How many sandwiches you can make before running out of ingredients.",
        LockWhileRunning::UNLOCKED,
        100, 0, 999
    )
    , SANDWICH_TYPE(
        "<b>Sandwich Recipe:</b><br>Which sandwich to make.<br>"
        "Sparkling/Title/Encounter: Cucumber + Pickle + 3x ingredient + 2x Herba Mystica<br>"
        "Sparkling/Title/Humungo: ???",
        LockWhileRunning::UNLOCKED,
        "love-ball"
        )
        
{
    PA_ADD_OPTION(MAX_NUM_SANDWICHES);
    PA_ADD_OPTION(SANDWICH_TYPE);

}


}
}
}
