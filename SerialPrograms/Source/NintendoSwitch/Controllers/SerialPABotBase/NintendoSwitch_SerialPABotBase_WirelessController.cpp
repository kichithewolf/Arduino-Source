/*  SerialPABotBase: Wireless Controller
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <sstream>
#include "Common/Cpp/PrettyPrint.h"
#include "Common/Cpp/Concurrency/ReverseLockGuard.h"
#include "Common/NintendoSwitch/NintendoSwitch_Protocol_ESP32.h"
#include "ClientSource/Libraries/MessageConverter.h"
#include "CommonFramework/GlobalSettingsPanel.h"
#include "CommonFramework/Options/Environment/ThemeSelectorOption.h"
#include "NintendoSwitch_SerialPABotBase_WirelessController.h"

//#include <iostream>
//using std::cout;
//using std::endl;

namespace PokemonAutomation{
namespace NintendoSwitch{

using namespace std::chrono_literals;



int register_message_converters_ESP32(){
    register_message_converter(
        PABB_MSG_ESP32_REQUEST_STATUS,
        [](const std::string& body){
            //  Disable this by default since it's very spammy.
            if (!GlobalSettings::instance().LOG_EVERYTHING){
                return std::string();
            }
            std::ostringstream ss;
            ss << "ESP32_controller_status() - ";
            if (body.size() != sizeof(pabb_esp32_request_status)){ ss << "(invalid size)" << std::endl; return ss.str(); }
            const auto* params = (const pabb_esp32_request_status*)body.c_str();
            ss << "seqnum = " << (uint64_t)params->seqnum;
            return ss.str();
        }
    );
    register_message_converter(
        PABB_MSG_ESP32_REQUEST_GET_COLORS,
        [](const std::string& body){
            std::ostringstream ss;
            ss << "pabb_esp32_get_colors() - ";
            if (body.size() != sizeof(pabb_esp32_get_colors)){ ss << "(invalid size)" << std::endl; return ss.str(); }
            const auto* params = (const pabb_esp32_get_colors*)body.c_str();
            ss << "seqnum = " << (uint64_t)params->seqnum;
            ss << ", controller = " << (uint64_t)params->controller_type;
            return ss.str();
        }
    );
    register_message_converter(
        PABB_MSG_ESP32_REQUEST_SET_COLORS,
        [](const std::string& body){
            std::ostringstream ss;
            ss << "pabb_esp32_set_colors() - ";
            if (body.size() != sizeof(pabb_esp32_set_colors)){ ss << "(invalid size)" << std::endl; return ss.str(); }
            const auto* params = (const pabb_esp32_set_colors*)body.c_str();
            ss << "seqnum = " << (uint64_t)params->seqnum;
            return ss.str();
        }
    );
    register_message_converter(
        PABB_MSG_ESP32_REPORT,
        [](const std::string& body){
            //  Disable this by default since it's very spammy.
            if (!GlobalSettings::instance().LOG_EVERYTHING){
                return std::string();
            }
            std::ostringstream ss;
            ss << "ESP32_controller_state() - ";
            if (body.size() != sizeof(pabb_esp32_report30)){ ss << "(invalid size)" << std::endl; return ss.str(); }
            const auto* params = (const pabb_esp32_report30*)body.c_str();
            ss << "seqnum = " << (uint64_t)params->seqnum;
            ss << ", ticks = " << (int)params->ticks;
            ss << ", active = " << (int)params->active;
            return ss.str();
        }
    );
    return 0;
}
int init_Messages_ESP32 = register_message_converters_ESP32();




SerialPABotBase_WirelessController::SerialPABotBase_WirelessController(
    Logger& logger,
    SerialPABotBase::SerialPABotBase_Connection& connection,
    ControllerType controller_type
)
    : SerialPABotBase_Controller(
        logger,
        controller_type,
        connection
    )
    , m_controller_type(controller_type)
    , m_stopping(false)
    , m_status_thread(&SerialPABotBase_WirelessController::status_thread, this)
{}
SerialPABotBase_WirelessController::~SerialPABotBase_WirelessController(){
    stop();
    m_status_thread.join();
}
void SerialPABotBase_WirelessController::stop(){
    if (m_stopping.exchange(true)){
        return;
    }
    m_scope.cancel(nullptr);
    {
        std::unique_lock<std::mutex> lg(m_sleep_lock);
        if (m_serial){
            m_serial->notify_all();
        }
        m_cv.notify_all();
    }
}







void SerialPABotBase_WirelessController::issue_report(
    const Cancellable* cancellable,
    const ESP32Report0x30& report,
    WallDuration duration
){
    bool is_active = this->is_active();

    //  Release the state lock since we are no longer touching state.
    //  This loop can block indefinitely if the command queue is full.
    ReverseLockGuard<std::mutex> lg(m_state_lock);

    //  Divide the controller state into smaller chunks of 255 ticks.
    Milliseconds time_left = std::chrono::duration_cast<Milliseconds>(duration);
    while (time_left > Milliseconds::zero()){
        Milliseconds current_ms = std::min(time_left, 255 * 15ms);
        uint8_t current_ticks = (uint8_t)milliseconds_to_ticks_15ms(current_ms.count());
        m_serial->issue_request(
            MessageControllerState(current_ticks, is_active, report),
            cancellable
        );
        time_left -= current_ms;
    }
}




void SerialPABotBase_WirelessController::status_thread(){
    constexpr std::chrono::milliseconds PERIOD(1000);
    std::atomic<WallClock> last_ack(current_time());

    //  Read controller colors.
    std::string color_html;
#if 1
    try{
        m_logger.log("Reading Controller Colors...");
        BotBaseMessage response = m_serial->issue_request_and_wait(
            MessageControllerGetColors(m_controller_type),
            &m_scope
        );
        ControllerColors colors{};
        if (response.body.size() == sizeof(seqnum_t) + sizeof(ControllerColors)){
            memcpy(&colors, response.body.data() + sizeof(seqnum_t), sizeof(ControllerColors));
        }else{
            m_logger.log(
                "Invalid response size to PABB_MSG_ESP32_GET_COLORS: body = " + std::to_string(response.body.size()),
                COLOR_RED
            );
            m_handle.set_status_line1("Error: See log for more information.", COLOR_RED);
            return;
        }
        m_logger.log("Reading Controller Colors... Done");

        switch (m_controller_type){
        case ControllerType::NintendoSwitch_WirelessProController:{
            Color left(colors.left_grip[0], colors.left_grip[1], colors.left_grip[2]);
            Color body(colors.body[0], colors.body[1], colors.body[2]);
            Color right(colors.right_grip[0], colors.right_grip[1], colors.right_grip[2]);
            color_html += html_color_text("&#x2b24;", left);
            color_html += " " + html_color_text("&#x2b24;", body);
            color_html += " " + html_color_text("&#x2b24;", right);
            break;
        }
        case ControllerType::NintendoSwitch_LeftJoycon:
        case ControllerType::NintendoSwitch_RightJoycon:{
            Color body(colors.body[0], colors.body[1], colors.body[2]);
            color_html = html_color_text("&#x2b24;", body);
            break;
        }
        default:;
        }

    }catch (Exception& e){
        e.log(m_logger);
        m_handle.set_status_line1("Error: See log for more information.", COLOR_RED);
        return;
    }
#endif


    std::thread watchdog([&, this]{
        WallClock next_ping = current_time();
        while (true){
            if (m_stopping.load(std::memory_order_relaxed) || !m_handle.is_ready()){
                break;
            }

            auto last = current_time() - last_ack.load(std::memory_order_relaxed);
            std::chrono::duration<double> seconds = last;
            if (last > 2 * PERIOD){
                std::string text = "Last Ack: " + tostr_fixed(seconds.count(), 3) + " seconds ago";
                m_handle.set_status_line1(text, COLOR_RED);
//                m_logger.log("Connection issue detected. Turning on all logging...");
//                settings.log_everything.store(true, std::memory_order_release);
            }

            std::unique_lock<std::mutex> lg(m_sleep_lock);
            if (m_stopping.load(std::memory_order_relaxed) || !m_handle.is_ready()){
                break;
            }

            WallClock now = current_time();
            next_ping += PERIOD;
            if (now + PERIOD < next_ping){
                next_ping = now + PERIOD;
            }
            m_cv.wait_until(lg, next_ping);
        }
    });

    WallClock next_ping = current_time();
    while (true){
        if (m_stopping.load(std::memory_order_relaxed) || !m_handle.is_ready()){
            break;
        }

        std::string error;
        try{
            pabb_MsgAckRequestI32 response;
            m_serial->issue_request_and_wait(
                MessageControllerStatus(),
                &m_scope
            ).convert<PABB_MSG_ACK_REQUEST_I32>(m_logger, response);
            last_ack.store(current_time(), std::memory_order_relaxed);

            uint32_t status = response.data;
            bool status_connected   = status & 1;
            bool status_paired      = status & 2;

            std::string str;
            str += "Connected: " + (status_connected
                ? html_color_text("Yes", theme_friendly_darkblue())
                : html_color_text("No", COLOR_RED)
            );
            str += " - Paired: " + (status_paired
                ? html_color_text("Yes", theme_friendly_darkblue())
                : html_color_text("No", COLOR_RED)
            );
            str += " - " + color_html;

            m_handle.set_status_line1(str);
        }catch (OperationCancelledException&){
            break;
        }catch (InvalidConnectionStateException&){
            break;
        }catch (SerialProtocolException& e){
            error = e.message();
        }catch (ConnectionException& e){
            error = e.message();
        }catch (...){
            error = "Unknown error.";
        }
        if (!error.empty()){
            m_handle.set_status_line1(error, COLOR_RED);
        }

//        cout << "lock()" << endl;
        std::unique_lock<std::mutex> lg(m_sleep_lock);
//        cout << "lock() - done" << endl;
        if (m_stopping.load(std::memory_order_relaxed) || !m_handle.is_ready()){
            break;
        }

        WallClock now = current_time();
        next_ping += PERIOD;
        if (now + PERIOD < next_ping){
            next_ping = now + PERIOD;
        }
        m_cv.wait_until(lg, next_ping);
    }

    {
        std::unique_lock<std::mutex> lg(m_sleep_lock);
        m_cv.notify_all();
    }
    watchdog.join();
}




}
}
