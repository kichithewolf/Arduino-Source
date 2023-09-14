/*  Battle Move Table
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#ifndef PokemonAutomation_PokemonSV_BattleMoveTable_H
#define PokemonAutomation_PokemonSV_BattleMoveTable_H

#include "Common/Cpp/Options/SimpleIntegerOption.h"
#include "Common/Cpp/Options/StringOption.h"
#include "Common/Cpp/Options/EnumDropdownOption.h"
#include "Common/Cpp/Options/EditableTableOption.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{


enum class BattleMoveType{
    Move1,
    Move2,
    Move3,
    Move4,
    Tera,
};
const EnumDatabase<BattleMoveType>& Battle_move_enum_database();

enum class BattleTarget{
    Opponent,
    Player0,    //  Yourself
};
const EnumDatabase<BattleTarget>& Battle_target_enum_database();


struct BattleMoveEntry{
    BattleMoveType type;
    BattleTarget target;

    std::string to_str() const;
};


class BattleMoveTableRow : public EditableTableRow, public ConfigOption::Listener{
public:
    ~BattleMoveTableRow();
    BattleMoveTableRow();
    virtual std::unique_ptr<EditableTableRow> clone() const override;

    BattleMoveEntry snapshot() const;

private:
    virtual void value_changed() override;

private:
    EnumDropdownCell<BattleMoveType> type;
    EnumDropdownCell<BattleTarget> target;
    StringCell notes;
};


class BattleMoveTable : public EditableTableOption_t<BattleMoveTableRow>{
public:
    BattleMoveTable();

    std::vector<BattleMoveEntry> snapshot();

    virtual std::vector<std::string> make_header() const;

    static std::vector<std::unique_ptr<EditableTableRow>> make_defaults();

};




}
}
}
#endif
