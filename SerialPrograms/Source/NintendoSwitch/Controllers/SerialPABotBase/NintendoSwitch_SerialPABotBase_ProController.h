/*  SerialPABotBase: Pro Controller
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#ifndef PokemonAutomation_NintendoSwitch_SerialPABotBase_ProController_H
#define PokemonAutomation_NintendoSwitch_SerialPABotBase_ProController_H

#include "ClientSource/Connection/BotBase.h"
#include "Controllers/SerialPABotBase/SerialPABotBase_Connection.h"
#include "NintendoSwitch/Controllers/NintendoSwitch_ProControllerWithScheduler.h"

namespace PokemonAutomation{
namespace NintendoSwitch{


class SerialPABotBase_ProController : public ProControllerWithScheduler{
public:
    using ContextType = ProControllerContext;

public:
    SerialPABotBase_ProController(
        Logger& logger,
        ControllerType controller_type,
        SerialPABotBase::SerialPABotBase_Connection& connection,
        const ControllerRequirements& requirements
    );
    ~SerialPABotBase_ProController();

    virtual bool is_ready() const override{
        return m_serial && m_handle.is_ready() && m_error_string.empty();
    }
    virtual std::string error_string() const override{
        return m_error_string;
    }


public:
    virtual void cancel_all_commands() override;
    virtual void replace_on_next_command() override;

    virtual void wait_for_all(const Cancellable* cancellable) override;

    virtual void send_botbase_request(
        const Cancellable* cancellable,
        const BotBaseRequest& request
    ) override;
    virtual BotBaseMessage send_botbase_request_and_wait(
        const Cancellable* cancellable,
        const BotBaseRequest& request
    ) override;


protected:
    //  These are set on construction and never changed again. So it is safe to
    //  access these asynchronously.
    SerialPABotBase::SerialPABotBase_Connection& m_handle;
    BotBaseController* m_serial;
    std::string m_error_string;
};




}
}
#endif
