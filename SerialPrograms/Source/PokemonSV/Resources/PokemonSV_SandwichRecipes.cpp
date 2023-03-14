/*  Sandwich Recipes
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#include <map>
#include "Common/Cpp/Json/JsonValue.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "CommonFramework/Globals.h"
#include "PokemonSV_SandwichRecipes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{


struct SandwichRecipeDatabase {
    SandwichRecipeDatabase();

    static const SandwichRecipeDatabase& instance() {
        static SandwichRecipeDatabase database;
        return database;
    }

    static const std::string NULL_SLUG;
    std::vector<std::string> ordered_list;
    std::map<std::string, SandwichRecipes> database;
    std::map<std::string, std::string> reverse_lookup;
};
const std::string SandwichRecipeDatabase::NULL_SLUG;

SandwichRecipeDatabase::SandwichRecipeDatabase()
{
    std::string path_slugs = RESOURCE_PATH() + "PokemonSV/Picnic/SandwichRecipes.json";
    JsonValue json_slugs = load_json_file(path_slugs);
    JsonArray& slugs = json_slugs.get_array_throw(path_slugs);

    std::string path_disp = RESOURCE_PATH() + "PokemonSV/Picnic/SandwichRecipesDisplay.json";
    JsonValue json_disp = load_json_file(path_disp);
    JsonObject& item_disp = json_disp.get_object_throw(path_disp);

    for (auto& item : slugs) {
        std::string& slug = item.get_string_throw(path_slugs);

        JsonObject& auction_item_name_dict = item_disp.get_object_throw(slug, path_disp);
        std::string& display_name = auction_item_name_dict.get_string_throw("eng", path_disp);

        ordered_list.push_back(slug);
        database[std::move(slug)].m_display_name = std::move(display_name);
    }

    for (const auto& item : database) {
        reverse_lookup[item.second.m_display_name] = item.first;
    }
}

const SandwichRecipes& get_sandwich_recipe(const std::string& slug) {
    const std::map<std::string, SandwichRecipes>& database = SandwichRecipeDatabase::instance().database;
    auto iter = database.find(slug);
    if (iter == database.end()) {
        throw InternalProgramError(
            nullptr, PA_CURRENT_FUNCTION,
            "Sandwich Recipe slug not found in database: " + slug
        );
    }
    return iter->second;
}
const std::string& parse_sandwich_recipe(const std::string& display_name) {
    const std::map<std::string, std::string>& database = SandwichRecipeDatabase::instance().reverse_lookup;
    auto iter = database.find(display_name);
    if (iter == database.end()) {
        throw InternalProgramError(
            nullptr, PA_CURRENT_FUNCTION,
            "Sandwich Recipe name not found in database: " + display_name
        );
    }
    return iter->second;
}
const std::string& parse_sandwich_recipe_nothrow(const std::string& display_name) {
    const std::map<std::string, std::string>& database = SandwichRecipeDatabase::instance().reverse_lookup;
    auto iter = database.find(display_name);
    if (iter == database.end()) {
        return SandwichRecipeDatabase::NULL_SLUG;
    }
    return iter->second;
}


const std::vector<std::string>& SANDWICH_RECIPE_SLUGS() {
    return SandwichRecipeDatabase::instance().ordered_list;
}


}
}
}
