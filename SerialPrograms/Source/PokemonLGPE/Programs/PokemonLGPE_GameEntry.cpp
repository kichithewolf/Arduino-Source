/*  Game Entry
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonFramework/Tools/ErrorDumper.h"
#include "CommonFramework/Tools/ProgramEnvironment.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/VisualDetectors/BlackScreenDetector.h"
#include "NintendoSwitch/NintendoSwitch_Settings.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_Routines.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "NintendoSwitch/Inference/NintendoSwitch_DetectHome.h"
#include "PokemonLGPE/PokemonLGPE_Settings.h"
#include "PokemonLGPE_GameEntry.h"

//#include <iostream>
//using std::cout;
//using std::endl;

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLGPE{


bool reset_game_to_gamemenu(
    VideoStream& stream, JoyconContext& context,
    bool tolerate_update_menu
){
    bool video_available = (bool)stream.video().snapshot();
    if (video_available ||
        ConsoleSettings::instance().START_GAME_REQUIRES_INTERNET ||
        tolerate_update_menu
    ){
        //Handle Right Joycon attempting to update
        //If no update, this will open the All Software menu
        //The active game will be closed from there
        //pbf_move_joystick(context, 0, 128, 100ms, 10ms);
        //pbf_move_joystick(context, 0, 128, 100ms, 10ms);
        //pbf_press_button(context, BUTTON_A, 100ms, 10ms);

        close_game(stream, context);
        start_game_from_home(
            stream,
            context,
            tolerate_update_menu,
            0, 0,
            GameSettings::instance().START_GAME_MASH0
        );
    }else{
        pbf_press_button(context, BUTTON_X, 400ms, 0ms);
        pbf_mash_button(context, BUTTON_A, GameSettings::instance().START_GAME_MASH0);
    }

    // Now the game has opened:
    return openedgame_to_gamemenu(stream, context, GameSettings::instance().START_GAME_WAIT1);
}

bool gamemenu_to_ingame(
    VideoStream& stream, JoyconContext& context,
    Milliseconds mash_duration, Milliseconds enter_game_timeout
){
    //Includes choosing the controller.
    //Controllers are disconnected? on selection screen so make sure to mash.
    stream.log("Mashing A to enter game and select controller...");
    pbf_mash_button(context, BUTTON_A, mash_duration);
    context.wait_for_all_requests();

    //White screen, Pikachu/Eevee running across the screen.
    //Mash A at then end.
    BlackScreenOverWatcher detector(COLOR_RED, {0.2, 0.2, 0.6, 0.6});
    stream.log("Waiting to enter game...");
    int ret = run_until<JoyconContext>(
        stream, context,
        [&enter_game_timeout](JoyconContext& context){
            pbf_wait(context, enter_game_timeout);
            pbf_press_button(context, BUTTON_A, 400ms, 10ms);
            pbf_wait(context, 5000ms);
        },
        {detector}
    );
    context.wait_for_all_requests();
    if (ret == 0){
        stream.log("At continue screen.");
    }else{
        stream.log("Timed out waiting to enter game and select continue.", COLOR_RED);
        return false;
    }
    pbf_wait(context, 1000ms);
    context.wait_for_all_requests();

    //Continue your adventure.
    BlackScreenOverWatcher detector2(COLOR_YELLOW, {0.2, 0.2, 0.6, 0.6});
    int ret2 = run_until<JoyconContext>(
        stream, context,
        [](JoyconContext& context){
            pbf_press_button(context, BUTTON_A, 400ms, 10ms);
            pbf_wait(context, 5000ms);
        },
        {detector2}
    );
    context.wait_for_all_requests();
    if (ret2 == 0){
        stream.log("Entered game!");
        return true;
    }else{
        stream.log("Timed out waiting to enter game.", COLOR_RED);
        return false;
    }
}

bool reset_game_from_home(
    ProgramEnvironment& env, VideoStream& stream, JoyconContext& context,
    bool tolerate_update_menu,
    Milliseconds post_wait_time
){
    bool ok = true;
    ok &= reset_game_to_gamemenu(stream, context, tolerate_update_menu);
    ok &= gamemenu_to_ingame(
        stream, context,
        GameSettings::instance().ENTER_GAME_MASH0,
        GameSettings::instance().ENTER_GAME_WAIT0
    );
    if (!ok){
        dump_image(stream.logger(), env.program_info(), stream.video(), "StartGame");
    }
    stream.log("Entered game! Waiting out grace period.");
    pbf_wait(context, post_wait_time);
    context.wait_for_all_requests();
    return ok;
}





}
}
}
