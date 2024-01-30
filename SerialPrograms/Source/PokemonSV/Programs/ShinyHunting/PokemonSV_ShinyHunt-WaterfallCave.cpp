/*  Shiny Hunt - Waterfall Cave
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#include <atomic>
#include <sstream>
#include "Common/Cpp/PrettyPrint.h"
#include "CommonFramework/GlobalSettingsPanel.h"
#include "CommonFramework/Exceptions/ProgramFinishedException.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/Exceptions/FatalProgramException.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/InferenceInfra/InferenceRoutines.h"
#include "CommonFramework/Tools/StatsTracking.h"
#include "CommonFramework/Tools/VideoResolutionCheck.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_Superscalar.h"
#include "Pokemon/Pokemon_Strings.h"
#include "PokemonSV/PokemonSV_Settings.h"
#include "PokemonSV/Inference/Overworld/PokemonSV_LetsGoHpReader.h"
#include "PokemonSV/Inference/Boxes/PokemonSV_IvJudgeReader.h"
#include "PokemonSV/Inference/Battles/PokemonSV_EncounterWatcher.h"
#include "PokemonSV/Programs/PokemonSV_GameEntry.h"
#include "PokemonSV/Programs/PokemonSV_Navigation.h"
#include "PokemonSV/Programs/PokemonSV_SaveGame.h"
#include "PokemonSV/Programs/Battles/PokemonSV_Battles.h"
#include "PokemonSV/Programs/Sandwiches/PokemonSV_SandwichRoutines.h"
#include "PokemonSV_LetsGoTools.h"
#include "PokemonSV/Programs/Eggs/PokemonSV_EggRoutines.h"
#include "PokemonSV/Programs/PokemonSV_AreaZero.h"
#include "PokemonSV_ShinyHunt-WaterfallCave.h"

namespace PokemonAutomation {
namespace NintendoSwitch {
namespace PokemonSV {

using namespace Pokemon;

ShinyHuntWaterfallCave_Descriptor::ShinyHuntWaterfallCave_Descriptor()
    : SingleSwitchProgramDescriptor(
        "PokemonSV:ShinyHuntWaterfallCave",
        STRING_POKEMON + " SV", "Shiny Hunt - Waterfall Cave",
        "ComputerControl/blob/master/Wiki/Programs/PokemonSV/ShinyHunt-WaterfallCave.md",
        "Shiny hunt in the waterfall cave outside Research Station 1.",
        FeedbackType::REQUIRED,
        AllowCommandsWhenRunning::DISABLE_COMMANDS,
        PABotBaseLevel::PABOTBASE_12KB
    )
{}
struct ShinyHuntWaterfallCave_Descriptor::Stats : public LetsGoEncounterBotStats {
    Stats()
        : m_sandwiches(m_stats["Sandwiches"])
        , m_autoheals(m_stats["Auto Heals"])
        , m_game_resets(m_stats["Game Resets"])
        , m_errors(m_stats["Errors"])
    {
        m_display_order.insert(m_display_order.begin() + 2, { "Sandwiches", HIDDEN_IF_ZERO });
        m_display_order.insert(m_display_order.begin() + 3, { "Auto Heals", HIDDEN_IF_ZERO });
        m_display_order.insert(m_display_order.begin() + 4, { "Game Resets", HIDDEN_IF_ZERO });
        m_display_order.insert(m_display_order.begin() + 5, { "Errors", HIDDEN_IF_ZERO });
    }
    std::atomic<uint64_t>& m_sandwiches;
    std::atomic<uint64_t>& m_autoheals;
    std::atomic<uint64_t>& m_game_resets;
    std::atomic<uint64_t>& m_errors;
};
std::unique_ptr<StatsTracker> ShinyHuntWaterfallCave_Descriptor::make_stats() const {
    return std::unique_ptr<StatsTracker>(new Stats());
}

ShinyHuntWaterfallCave::~ShinyHuntWaterfallCave(){
    MODE.remove_listener(*this);
}

ShinyHuntWaterfallCave::ShinyHuntWaterfallCave()
    : LANGUAGE(
          "<b>Game Language:</b><br>Required to read " + STRING_POKEMON + " names.",
          IV_READER().languages(),
          LockMode::LOCK_WHILE_RUNNING,
          false
          )
    , MODE(
        "<b>Mode:</b><br>"
        "If starting in the cave, you should stand near the center of the cave facing any direction.<br>"
        "If starting in the Zero Gate, you should be just inside the building as if you just entered."
        "<br>If making a sandwich, you should be at the Zero Gate fly spot as if you just flew there.",
        {
         {Mode::START_IN_CAVE,   "platform", "Start already inside the cave."},
         {Mode::START_IN_ZERO_GATE,  "zerogate", "Start inside Zero Gate."},
         {Mode::MAKE_SANDWICH,       "sandwich", "Make a sandwich."},
         },
        LockMode::LOCK_WHILE_RUNNING,
        Mode::MAKE_SANDWICH
        )

    , SANDWICH_RESET_IN_MINUTES(
        "<b>Sandwich Reset Time (in minutes):</b><br>The time to reset game to make a new sandwich.",
        LockMode::UNLOCK_WHILE_RUNNING,
        35
    )
    , SANDWICH_OPTIONS(LANGUAGE)
    , GO_HOME_WHEN_DONE(true)
    , AUTO_HEAL_PERCENT(
        "<b>Auto-Heal %</b><br>Auto-heal if your HP drops below this percentage.",
        LockMode::UNLOCK_WHILE_RUNNING,
        75, 0, 100
    )
    , HEAL_AT_STATION(
        "<b>Heal at Station:</b><br>If you're passing through the station, take the opportunity to heal up.",
        LockMode::UNLOCK_WHILE_RUNNING,
        true
    )
    , STATION_ARRIVE_PAUSE_SECONDS(
        "<b>Station Arrive Pause Time:</b><br>Pause for this many seconds after leaving the station. "
        "This gives the game time to load and thus reduce the chance of lag affecting the flight path.",
        LockMode::UNLOCK_WHILE_RUNNING,
        1
    )
    , MIDAIR_PAUSE_TIME(
        "<b>Mid-Air Pause Time:</b><br>Pause for this long before dropping down into the cave. "
        "Adjust this if you are sticking onto the walls while falling down the hole.",
        LockMode::UNLOCK_WHILE_RUNNING,
        TICKS_PER_SECOND,
        "350"
    )
    , INVERTED_FLIGHT(
        "<b>Inverted controls while flying:</b><br>"
        "Check this option if you have inverted controls on during flying.",
        LockMode::UNLOCK_WHILE_RUNNING,
        false
    )
    , NOTIFICATION_STATUS_UPDATE("Status Update", true, false, std::chrono::seconds(3600))
    , NOTIFICATIONS({
        &NOTIFICATION_STATUS_UPDATE,
        &ENCOUNTER_BOT_OPTIONS.NOTIFICATION_NONSHINY,
        &ENCOUNTER_BOT_OPTIONS.NOTIFICATION_SHINY,
        &ENCOUNTER_BOT_OPTIONS.NOTIFICATION_CATCH_SUCCESS,
        &ENCOUNTER_BOT_OPTIONS.NOTIFICATION_CATCH_FAILED,
        &NOTIFICATION_PROGRAM_FINISH,
        &NOTIFICATION_ERROR_RECOVERABLE,
        &NOTIFICATION_ERROR_FATAL,
        })
{
    PA_ADD_OPTION(LANGUAGE);
    PA_ADD_OPTION(MODE);
    PA_ADD_OPTION(SANDWICH_RESET_IN_MINUTES);
    PA_ADD_OPTION(SANDWICH_OPTIONS);
    PA_ADD_OPTION(ENCOUNTER_BOT_OPTIONS);
    PA_ADD_OPTION(GO_HOME_WHEN_DONE);
    PA_ADD_OPTION(AUTO_HEAL_PERCENT);
    PA_ADD_OPTION(HEAL_AT_STATION);
    PA_ADD_OPTION(STATION_ARRIVE_PAUSE_SECONDS);
    PA_ADD_OPTION(MIDAIR_PAUSE_TIME);
    PA_ADD_OPTION(INVERTED_FLIGHT);
    PA_ADD_OPTION(NOTIFICATIONS);

    ShinyHuntWaterfallCave::value_changed();

    MODE.add_listener(*this);
}

std::string ShinyHuntWaterfallCave::check_validity() const{
    std::string error = SingleSwitchProgramInstance::check_validity();
    if (!error.empty()){
        return error;
    }
    if (LANGUAGE == Language::None && MODE == Mode::MAKE_SANDWICH){
        return "Sandwich mode requires selecting a language.";
    }
    return "";
}

void ShinyHuntWaterfallCave::value_changed(){
    ConfigOptionState state = MODE == Mode::MAKE_SANDWICH
        ? ConfigOptionState::ENABLED
        : ConfigOptionState::HIDDEN;
    SANDWICH_RESET_IN_MINUTES.set_visibility(state);
    SANDWICH_OPTIONS.set_visibility(state);
}

struct ResetException{};

void ShinyHuntWaterfallCave::inside_zero_gate_to_cave(const ProgramInfo& info, ConsoleHandle& console, BotBaseContext& context) {
    inside_zero_gate_to_station(info, console, context, 1, HEAL_AT_STATION);
    context.wait_for(std::chrono::seconds(STATION_ARRIVE_PAUSE_SECONDS));

    //Angle is already good, so center camera and mount ride
    pbf_press_button(context, BUTTON_L | BUTTON_PLUS, 20, 105);

    //Jump, glide, press fly
    ssf_press_button(context, BUTTON_B, 0, 100);
    ssf_press_button(context, BUTTON_B, 0, 20, 10); //  Double up this press in
    ssf_press_button(context, BUTTON_B, 0, 20);     //  case one is dropped.
    pbf_wait(context, 100);
    context.wait_for_all_requests();

    pbf_press_button(context, BUTTON_LCLICK, 50, 0);

    //Fly up
    if (INVERTED_FLIGHT) {
        pbf_move_left_joystick(context, 128, 255, 2250, 250);
    }
    else {
        pbf_move_left_joystick(context, 128, 0, 2250, 250);
    }
    pbf_wait(context, MIDAIR_PAUSE_TIME);
    context.wait_for_all_requests();

    //Drop down into the depths
    pbf_press_button(context, BUTTON_B, 20, 50);

    //Wait for drop to end
    pbf_wait(context, 1750);
    context.wait_for_all_requests();

    //Dismount and center camera
    pbf_press_button(context, BUTTON_PLUS, 20, 105);
    pbf_press_button(context, BUTTON_R, 20, 355);
}

void ShinyHuntWaterfallCave::run_path(
    ProgramEnvironment& env, ConsoleHandle& console, BotBaseContext& context,
    LetsGoEncounterBotTracker& tracker,
    uint64_t iteration_count
){
    //Turn right
    pbf_press_button(context, BUTTON_L, 20, 50);
    pbf_move_left_joystick(context, 255, 128, 10, 0);
    pbf_press_button(context, BUTTON_L, 20, 50);

    use_lets_go_to_clear_in_front(console, context, tracker, false, [&](BotBaseContext& context){
        pbf_move_left_joystick(context, 128, 0, 800, 0);
        pbf_press_button(context, BUTTON_L, 20, 50);
    });

    //Turn around
    pbf_move_left_joystick(context, 128, 255, 10, 0);
    pbf_press_button(context, BUTTON_L, 20, 50);

    use_lets_go_to_clear_in_front(console, context, tracker, false, [&](BotBaseContext& context){
        pbf_move_left_joystick(context, 128, 0, 800, 0);
        pbf_press_button(context, BUTTON_L, 20, 50);
    });
}

bool ShinyHuntWaterfallCave::run_traversal(BotBaseContext& context){
    ShinyHuntWaterfallCave_Descriptor::Stats& stats = m_env->current_stats<ShinyHuntWaterfallCave_Descriptor::Stats>();

    const ProgramInfo& info = m_env->program_info();
    ConsoleHandle& console = m_env->console;

    double hp = m_hp_watcher->last_known_value() * 100;
    if (0 < hp){
        console.log("Last Known HP: " + tostr_default(hp) + "%", COLOR_BLUE);
    }else{
        console.log("Last Known HP: ?", COLOR_RED);
    }
    if (0 < hp && hp < AUTO_HEAL_PERCENT){
        auto_heal_from_menu_or_overworld(info, console, context, 0, true);
        stats.m_autoheals++;
        m_env->update_stats();
    }

    WallClock start = current_time();

    size_t kills = 0, encounters = 0;
    //std::chrono::minutes window_minutes(PLATFORM_RESET.WINDOW_IN_MINUTES); TODO MAKE CONFIG?
    std::chrono::minutes window_minutes(35);
    WallDuration window = m_time_tracker->last_window_in_realtime(start, window_minutes);

    std::chrono::seconds window_seconds;
    bool enough_time;
    if (window == WallDuration::zero()){
        enough_time = false;
        window = start - m_encounter_tracker->encounter_rate_tracker_start_time();
        window_seconds = std::chrono::duration_cast<Seconds>(window);
        m_encounter_tracker->get_encounters_in_window(
            kills, encounters, window_seconds
        );
    }else{
        window_seconds = std::chrono::duration_cast<Seconds>(window);
        enough_time = m_encounter_tracker->get_encounters_in_window(
            kills, encounters, window_seconds
        );
    }
    console.log(
        "Starting Traversal Iteration: " + tostr_u_commas(m_iterations) +
        "\n    Time Window (Seconds): " + std::to_string(window_seconds.count()) +
        "\n    Kills: " + std::to_string(kills) +
        "\n    Encounters: " + std::to_string(encounters)
    );

    // Send Let's Go pokemon to beat wild pokemon while moving on the platform following one path.
    // It tracks the kill chain by sound detection from `m_encounter_tracker`.
    try{
        run_path(*m_env, console, context, *m_encounter_tracker, m_iterations);
        m_iterations++;
    }catch (...){
        m_time_tracker->add_block(start, current_time());
        throw;
    }
    m_time_tracker->add_block(start, current_time());

    return true;
}

void ShinyHuntWaterfallCave::run_state(SingleSwitchProgramEnvironment& env, BotBaseContext& context){
    ShinyHuntWaterfallCave_Descriptor::Stats& stats = env.current_stats<ShinyHuntWaterfallCave_Descriptor::Stats>();

    const ProgramInfo& info = m_env->program_info();
    ConsoleHandle& console = m_env->console;

    if (m_pending_save){
        console.log("Executing: Pending Save...");
        if (m_current_location != Location::ZERO_GATE_FLY_SPOT && m_current_location != Location::AREA_ZERO_CAVE){
            return_to_outside_zero_gate(info, console, context);
            m_current_location = Location::ZERO_GATE_FLY_SPOT;
        }
        save_game_from_overworld(info, console, context);
        m_saved_location = m_current_location;
        m_pending_save = false;
        return;
    }

    if (m_pending_sandwich){
        console.log("Executing: Pending Sandwich...");

        //  If we need to reset, do so now.
        if (m_reset_on_next_sandwich){
            throw ResetException();
        }

        //  If we're not at Zero Gate, go there now.
        if (m_current_location != Location::ZERO_GATE_FLY_SPOT){
            return_to_outside_zero_gate(info, console, context);
            m_current_location = Location::ZERO_GATE_FLY_SPOT;
            m_pending_platform_reset = false;
        }

        m_reset_on_next_sandwich = true;

        //  If we're not saved at Zero Gate, do it now.
        if (m_saved_location != Location::ZERO_GATE_FLY_SPOT){
            save_game_from_overworld(info, console, context);
            m_saved_location = m_current_location;
        }

        picnic_at_zero_gate(info, console, context);
        pbf_move_left_joystick(context, 128, 0, 70, 0);
        enter_sandwich_recipe_list(info, console, context);
        run_sandwich_maker(env, context, SANDWICH_OPTIONS);

        console.log("Sandwich Reset: Starting new sandwich timer...");
        m_last_sandwich = current_time();

        stats.m_sandwiches++;
        m_env->update_stats();

        leave_picnic(info, console, context);
        return_to_inside_zero_gate_from_picnic(info, console, context);
        m_current_location = Location::ZERO_GATE_INSIDE;

        inside_zero_gate_to_cave(info, console, context);
        m_current_location = Location::AREA_ZERO_CAVE;

        m_pending_sandwich = false;

        m_encounter_tracker->reset_rate_tracker_start_time();
        m_consecutive_failures = 0;
        return;
    }

    if (m_pending_platform_reset){
        console.log("Executing: Platform Reset");
        return_to_inside_zero_gate(info, console, context);
        inside_zero_gate_to_cave(info, console, context);
        m_current_location = Location::AREA_ZERO_CAVE;

        m_pending_platform_reset = false;
        m_encounter_tracker->reset_rate_tracker_start_time();
        m_consecutive_failures = 0;
        return;
    }

    switch (m_current_location){
    case Location::UNKNOWN:
    case Location::ZERO_GATE_FLY_SPOT:
    case Location::TRAVELING_TO_CAVE:
        console.log("Executing: Platform Reset (state-based)...");
        return_to_inside_zero_gate(info, console, context);
        inside_zero_gate_to_cave(info, console, context);
        m_current_location = Location::AREA_ZERO_CAVE;
        m_pending_platform_reset = false;
        m_encounter_tracker->reset_rate_tracker_start_time();
        m_consecutive_failures = 0;
        return;
    case Location::ZERO_GATE_INSIDE:
        console.log("Executing: Zero Gate -> Platform...");
        m_current_location = Location::AREA_ZERO_CAVE;
        inside_zero_gate_to_cave(info, console, context);
        m_pending_platform_reset = false;
        m_encounter_tracker->reset_rate_tracker_start_time();
        m_consecutive_failures = 0;
        return;
    case Location::AREA_ZERO_CAVE:
        console.log("Executing: Traversal...");
        try{
            run_traversal(context);
        }catch (OperationFailedException&){
            m_pending_platform_reset = true;
            throw;
        }
        m_consecutive_failures = 0;
        return;
    }
}

void ShinyHuntWaterfallCave::set_flags_and_run_state(SingleSwitchProgramEnvironment& env, BotBaseContext& context){
    ShinyHuntWaterfallCave_Descriptor::Stats& stats = env.current_stats<ShinyHuntWaterfallCave_Descriptor::Stats>();
    ConsoleHandle& console = m_env->console;

    send_program_notification(
        *m_env, NOTIFICATION_STATUS_UPDATE,
        Color(0),
        "Program Status",
        {}, m_encounter_tracker->encounter_frequencies().dump_sorted_map("")
    );

    WallClock now = current_time();
    if (MODE == Mode::MAKE_SANDWICH &&
        m_last_sandwich + std::chrono::minutes(SANDWICH_RESET_IN_MINUTES) < now
    ){
        console.log("Enough time has elapsed. Time to reset sandwich...");
        m_pending_sandwich = true;
    }

    int64_t seconds_on_sandwich = std::chrono::duration_cast<std::chrono::seconds>(now - m_last_sandwich).count();
    console.log(
        std::string("State:\n") +
        "    Time on Sandwich: " + (m_last_sandwich == WallClock::min()
            ? "N/A"
            : std::to_string(seconds_on_sandwich)) + " seconds\n" +
        "    Pending Save: " + (m_pending_save ? "Yes" : "No") + "\n" +
        "    Pending Platform Reset: " + (m_pending_platform_reset ? "Yes" : "No") + "\n" +
        "    Pending Sandwich: " + (m_pending_sandwich ? "Yes" : "No") + "\n" +
        "    Reset after Sandwich: " + (m_reset_on_next_sandwich ? "Yes" : "No") + "\n"
    );

    try{
        run_state(env, context);
    }catch (OperationFailedException& e){
        stats.m_errors++;
        m_env->update_stats();
        m_consecutive_failures++;
        e.send_notification(*m_env, NOTIFICATION_ERROR_RECOVERABLE);
        if (m_consecutive_failures >= 3){
            throw FatalProgramException(
                ErrorReport::SEND_ERROR_REPORT, console,
                "Failed 3 times consecutively."
            );
        }
    }
}


void ShinyHuntWaterfallCave::program(SingleSwitchProgramEnvironment& env, BotBaseContext& context) {
    ShinyHuntWaterfallCave_Descriptor::Stats& stats = env.current_stats<ShinyHuntWaterfallCave_Descriptor::Stats>();
    assert_16_9_720p_min(env.logger(), env.console);

    /*
    This is Area Zero Platform with a different path.
    Pokemon: Espeon, Umbreon, Chansey, Pawmi, Pawmo, Dugtrio, Glimmet, Sableye, Sneasel, Weavile, Gible, Gabite, Zweilous, Lycanroc-Midnight, Flutter Mane, Iron Jugulis
    TODO: Dectect if fallen outside the cave by checking encounters?
    */

    m_env = &env;
    m_iterations = 0;

    LetsGoHpWatcher hp_watcher(COLOR_RED);
    m_hp_watcher = &hp_watcher;

    DiscontiguousTimeTracker time_tracker;
    m_time_tracker = &time_tracker;

    LetsGoEncounterBotTracker encounter_tracker(
        env, env.console, context,
        stats,
        LANGUAGE
    );
    m_encounter_tracker = &encounter_tracker;

    m_saved_location = Location::UNKNOWN;
    m_pending_save = false;
    m_pending_platform_reset = false;
    m_pending_sandwich = false;
    m_reset_on_next_sandwich = false;

    switch (MODE){
    case Mode::START_IN_CAVE:
        m_current_location = Location::AREA_ZERO_CAVE;
        break;
    case Mode::START_IN_ZERO_GATE:
        m_current_location = Location::ZERO_GATE_INSIDE;
        break;
    case Mode::MAKE_SANDWICH:
        m_current_location = Location::UNKNOWN;
        m_pending_save = false;
        m_pending_sandwich = true;
        break;
    }

    m_last_sandwich = WallClock::min();

    //  This is the outer-most program loop that wraps all logic with the
    //  battle menu detector. If at any time you detect a battle menu, you break
    //  all the way out here to handle the encounter. This is needed because you
    //  can get attacked at almost any time while in Area Zero.
    m_consecutive_failures = 0;
    while (true){
        try{
            env.console.log("Starting encounter loop...", COLOR_PURPLE);
            EncounterWatcher encounter_watcher(env.console, COLOR_RED);
            run_until(
                env.console, context,
                [&](BotBaseContext& context){
                    //  Inner program loop that runs the state machine.
                    while (true){
                        set_flags_and_run_state(env, context);
                    }
                },
                {
                    static_cast<VisualInferenceCallback&>(encounter_watcher),
                    static_cast<AudioInferenceCallback&>(encounter_watcher),
                    hp_watcher,
                }
            );
            encounter_watcher.throw_if_no_sound();

            env.console.log("Detected battle.", COLOR_PURPLE);
            bool caught, should_save;
            encounter_tracker.process_battle(
                caught, should_save,
                encounter_watcher, ENCOUNTER_BOT_OPTIONS
            );
            m_pending_save |= should_save;
            if (caught){
                m_reset_on_next_sandwich = false;
            }
        }catch (ResetException&){
            pbf_press_button(context, BUTTON_HOME, 20, GameSettings::instance().GAME_TO_HOME_DELAY);
            reset_game_from_home_zoom_out(env.program_info(), env.console, context, 5 * TICKS_PER_SECOND);

            m_current_location = m_saved_location;

            stats.m_game_resets++;
            m_env->update_stats();

            m_pending_platform_reset = false;
            m_reset_on_next_sandwich = false;
        }catch (ProgramFinishedException&){
            GO_HOME_WHEN_DONE.run_end_of_program(context);
            throw;
        }
    }

}





}
}
}
