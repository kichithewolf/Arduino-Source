/*  Tournament Prize Names
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#ifndef PokemonAutomation_PokemonSV_SandwichRecipes_H
#define PokemonAutomation_PokemonSV_SandwichRecipes_H

#include <string>
#include <vector>

namespace PokemonAutomation {
namespace NintendoSwitch {
namespace PokemonSV {


class SandwichRecipes {
public:
    const std::string& display_name() const { return m_display_name; }

private:
    friend struct SandwichRecipeDatabase;

    std::string m_display_name;
};


const SandwichRecipes& get_sandwich_recipe(const std::string& slug);
const std::string& parse_sandwich_recipe(const std::string& display_name);
const std::string& parse_sandwich_recipe_nothrow(const std::string& display_name);

const std::vector<std::string>& SANDWICH_RECIPE_SLUGS();


}
}
}
#endif
