/*  Battle Move Table
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#include "PokemonSV_BattleMoveTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{


const EnumDatabase<BattleMoveType>& Battle_move_enum_database(){
    static EnumDatabase<BattleMoveType> database{
        {BattleMoveType::Move1,           "move1",            "Move 1"},
        {BattleMoveType::Move2,           "move2",            "Move 2"},
        {BattleMoveType::Move3,           "move3",            "Move 3"},
        {BattleMoveType::Move4,           "move4",            "Move 4"},
        {BattleMoveType::Tera,            "tera",             "Tera"},
    };
    return database;
}
const EnumDatabase<BattleTarget>& Battle_target_enum_database(){
    static EnumDatabase<BattleTarget> database{
        {BattleTarget::Opponent,  "opponent", "Opponent"},
        {BattleTarget::Player0,   "player0",  "Player 0 (yourself)"},
    };
    return database;
}


std::string BattleMoveEntry::to_str() const{
    switch (type){
    case BattleMoveType::Tera:
        return "Tera";
    case BattleMoveType::Move1:
    case BattleMoveType::Move2:
    case BattleMoveType::Move3:
    case BattleMoveType::Move4:{
        int slot = (int)type - (int)BattleMoveType::Move1;
        std::string str = "Move " + std::to_string(slot + 1) + " on ";
        if (target == BattleTarget::Opponent){
            return str + "opponent.";
        }else{
            slot = (int)target - (int)BattleTarget::Player0;
            return "player " + std::to_string(slot) + ".";
        }
    }
    }
    return "(Invalid Move)";
}




BattleMoveTableRow::~BattleMoveTableRow(){
    type.remove_listener(*this);
}
BattleMoveTableRow::BattleMoveTableRow()
    : type(Battle_move_enum_database(), LockWhileRunning::UNLOCKED, BattleMoveType::Move1)
    , target(Battle_target_enum_database(), LockWhileRunning::UNLOCKED, BattleTarget::Opponent)
    , notes(false, LockWhileRunning::UNLOCKED, "", "(e.g. Screech, Belly Drum)")
{
    PA_ADD_OPTION(type);
    PA_ADD_OPTION(target);
    PA_ADD_OPTION(notes);

    BattleMoveTableRow::value_changed();
    type.add_listener(*this);
}
std::unique_ptr<EditableTableRow> BattleMoveTableRow::clone() const{
    std::unique_ptr<BattleMoveTableRow> ret(new BattleMoveTableRow());
    ret->type.set(type);
    ret->target.set(target);
    ret->notes.set(notes);
    return ret;
}
BattleMoveEntry BattleMoveTableRow::snapshot() const{
    return BattleMoveEntry{type, target};
}
void BattleMoveTableRow::value_changed(){
    BattleMoveType type = this->type;

    bool is_move =
        type == BattleMoveType::Move1 ||
        type == BattleMoveType::Move2 ||
        type == BattleMoveType::Move3 ||
        type == BattleMoveType::Move4;

    target.set_visibility(
        is_move
        ? ConfigOptionState::ENABLED
        : ConfigOptionState::HIDDEN
    );

}





BattleMoveTable::BattleMoveTable()
    : EditableTableOption_t<BattleMoveTableRow>(
        "<b>Move Table:</b><br>"
        "Run this sequence of moves. When the end of the table is reached, "
        "the last entry will be repeated until the battle is won or lost. "
        "Changes to this table take effect on the next battle.",
        LockWhileRunning::UNLOCKED,
        make_defaults()
    )
{}

std::vector<BattleMoveEntry> BattleMoveTable::snapshot(){
    return EditableTableOption_t<BattleMoveTableRow>::snapshot<BattleMoveEntry>();
}
std::vector<std::string> BattleMoveTable::make_header() const{
    return {
        "Move",
        "Target",
        "Notes",
    };
}
std::vector<std::unique_ptr<EditableTableRow>> BattleMoveTable::make_defaults(){
    std::vector<std::unique_ptr<EditableTableRow>> ret;
    ret.emplace_back(new BattleMoveTableRow());
    return ret;
}








}
}
}
