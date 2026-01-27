#include <cstring>
#include <cstdint>
#include <limits>

#include "USBDevice/DeviceDriver/XInput/tud_xinput/tud_xinput.h"
#include "USBDevice/DeviceDriver/XInput/XInput.h"
#include "XInputDevice.h" // asumption: contains class declaration and member variables

void XInputDevice::initialize() 
{
    class_driver_ = *tud_xinput::class_driver();
}

void XInputDevice::process(const uint8_t idx, Gamepad& gamepad)
{
    // Nota: este archivo asume que los siguientes miembros existen en XInputDevice (ver XInputDevice.h):
    //   uint32_t turbo_tick_counter_;
    //   uint16_t turbo_period_ticks_;
    //   uint16_t turbo_on_ticks_;
    //   bool antirecoil_active_;
    //   int32_t antirecoil_offset_rx_;
    //   uint8_t trigger_threshold_;
    //
    // Ajusta esos miembros en el header si aún no los añadiste.

    if (gamepad.new_pad_in())
    {
        in_report_.buttons[0] = 0;
        in_report_.buttons[1] = 0;

        Gamepad::PadIn gp_in = gamepad.get_pad_in();

        // --- DPAD mapping (sin cambios) ---
        switch (gp_in.dpad)
        {
            case Gamepad::DPAD_UP:
                in_report_.buttons[0] = XInput::Buttons0::DPAD_UP;
                break;
            case Gamepad::DPAD_DOWN:
                in_report_.buttons[0] = XInput::Buttons0::DPAD_DOWN;
                break;
            case Gamepad::DPAD_LEFT:
                in_report_.buttons[0] = XInput::Buttons0::DPAD_LEFT;
                break;
            case Gamepad::DPAD_RIGHT:
                in_report_.buttons[0] = XInput::Buttons0::DPAD_RIGHT;
                break;
            case Gamepad::DPAD_UP_LEFT:
                in_report_.buttons[0] = XInput::Buttons0::DPAD_UP | XInput::Buttons0::DPAD_LEFT;
                break;
            case Gamepad::DPAD_UP_RIGHT:
                in_report_.buttons[0] = XInput::Buttons0::DPAD_UP | XInput::Buttons0::DPAD_RIGHT;
                break;
            case Gamepad::DPAD_DOWN_LEFT:
                in_report_.buttons[0] = XInput::Buttons0::DPAD_DOWN | XInput::Buttons0::DPAD_LEFT;
                break;
            case Gamepad::DPAD_DOWN_RIGHT:
                in_report_.buttons[0] = XInput::Buttons0::DPAD_DOWN | XInput::Buttons0::DPAD_RIGHT;
                break;
            default:
                break;
        }

        // -------------------------
        // 1) SELECT -> actuar como L1 (LB) + MUTE
        //    Asumimos que SELECT corresponde a Gamepad::BUTTON_BACK.
        const uint32_t GAMEPAD_SELECT = Gamepad::BUTTON_BACK;

        if (gp_in.buttons & GAMEPAD_SELECT)
        {
            // Mapear SELECT a L1 (XInput LB)
            in_report_.buttons[1] |= XInput::Buttons1::LB;

            // Acción MUTE: si tu Gamepad tiene método para mute, habilita la macro GAMEPAD_HAS_MUTE
            // y proporciona la implementación en tu proyecto. Si no existe, este bloque no hace nada.
            #ifdef GAMEPAD_HAS_MUTE
            gamepad.set_mute(true); // o el método que uses para mute
            #endif
        }

        // -------------------------
        // 2) L1 físico -> turbo (auto-fire) del botón X (mientras se mantenga)
        //    Asumimos que L1 físico es Gamepad::BUTTON_LB
        const uint32_t GAMEPAD_L1 = Gamepad::BUTTON_LB;
        if (gp_in.buttons & GAMEPAD_L1)
        {
            // incrementa contador de ticks mientras se mantiene L1 físico
            turbo_tick_counter_++;
            // determina si en este tick el turbo debe estar ON
            if ((turbo_tick_counter_ % turbo_period_ticks_) < turbo_on_ticks_)
            {
                // setear X como presionado en el in_report_ (turbo ON)
                in_report_.buttons[1] |= XInput::Buttons1::X;
            }
            // NO mapeamos el LB físico a XInput::Buttons1::LB para evitar colisión con SELECT
        }
        else
        {
            // reset contador turbo cuando sueltas L1 físico para evitar ráfagas inconsistentes
            turbo_tick_counter_ = 0;
        }

        // -------------------------
        // 3) Otros botones y mapeos no conflictivos
        if (gp_in.buttons & Gamepad::BUTTON_START) in_report_.buttons[0] |= XInput::Buttons0::START;
        if (gp_in.buttons & Gamepad::BUTTON_L3)    in_report_.buttons[0] |= XInput::Buttons0::L3;
        if (gp_in.buttons & Gamepad::BUTTON_R3)    in_report_.buttons[0] |= XInput::Buttons0::R3;

        // Botones A/B/X/Y y RB / HOME
        // Nota: X también puede ser establecido por turbo — el |= permite ambos casos.
        if (gp_in.buttons & Gamepad::BUTTON_X)     in_report_.buttons[1] |= XInput::Buttons1::X;
        if (gp_in.buttons & Gamepad::BUTTON_A)     in_report_.buttons[1] |= XInput::Buttons1::A;
        if (gp_in.buttons & Gamepad::BUTTON_Y)     in_report_.buttons[1] |= XInput::Buttons1::Y;
        if (gp_in.buttons & Gamepad::BUTTON_B)     in_report_.buttons[1] |= XInput::Buttons1::B;
        // Si quieres que el LB físico también ponga LB real (además del turbo), habilita la siguiente línea:
        // if (gp_in.buttons & Gamepad::BUTTON_LB) in_report_.buttons[1] |= XInput::Buttons1::LB;
        if (gp_in.buttons & Gamepad::BUTTON_RB)    in_report_.buttons[1] |= XInput::Buttons1::RB;
        if (gp_in.buttons & Gamepad::BUTTON_SYS)   in_report_.buttons[1] |= XInput::Buttons1::HOME;

        // Triggers directos
        in_report_.trigger_l = gp_in.trigger_l;
        in_report_.trigger_r = gp_in.trigger_r;

        // Joysticks base
        in_report_.joystick_lx = gp_in.joystick_lx;
        in_report_.joystick_ly = Range::invert(gp_in.joystick_ly);
        in_report_.joystick_rx = gp_in.joystick_rx;
        in_report_.joystick_ry = Range::invert(gp_in.joystick_ry);

        // -------------------------
        // 4) Antirecoil: activar cuando L2 + R2 estén "pulsados"
        //    Muchas implementaciones usan triggers analógicos; comprobamos trigger_l/r > umbral.
        if ((gp_in.trigger_l > trigger_threshold_) && (gp_in.trigger_r > trigger_threshold_))
        {
            antirecoil_active_ = true;

            // Aplica compensación hacia la derecha al eje rx. 
            int32_t rx = static_cast<int32_t>(in_report_.joystick_rx);
            rx += antirecoil_offset_rx_;

            const int32_t RX_MAX = std::numeric_limits<decltype(in_report_.joystick_rx)>::max();
            if (rx > RX_MAX) rx = RX_MAX;
            if (rx < 0) rx = 0;
            in_report_.joystick_rx = static_cast<decltype(in_report_.joystick_rx)>(rx);
        }
        else
        {
            antirecoil_active_ = false;
        }

        // -------------------------
        // USB resume + envío de informe
        if (tud_suspended())
        {
            tud_remote_wakeup();
        }

        tud_xinput::send_report((uint8_t*)&in_report_, sizeof(XInput::InReport));
    }

    // Recepción de rumble - sin cambios
    if (tud_xinput::receive_report(reinterpret_cast<uint8_t*>(&out_report_), sizeof(XInput::OutReport)) &&
        out_report_.report_id == XInput::OutReportID::RUMBLE)
    {
        Gamepad::PadOut gp_out;
        gp_out.rumble_l = out_report_.rumble_l;
        gp_out.rumble_r = out_report_.rumble_r;
        gamepad.set_pad_out(gp_out);
    }
}

uint16_t XInputDevice::get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen) 
{
    std::memcpy(buffer, &in_report_, sizeof(XInput::InReport));
    return sizeof(XInput::InReport);
}

void XInputDevice::set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize) 
{
    // No-op por ahora. Si necesitas manejar SET_REPORT desde host, implementa aquí.
    (void)itf;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)bufsize;
}

bool XInputDevice::vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request) 
{
    (void)rhport;
    (void)stage;
    (void)request;
    return false;
}

const uint16_t * XInputDevice::get_descriptor_string_cb(uint8_t index, uint16_t langid) 
{
    const char *value = reinterpret_cast<const char*>(XInput::DESC_STRING[index]);
    return get_string_descriptor(value, index);
}

const uint8_t * XInputDevice::get_descriptor_device_cb() 
{
    return XInput::DESC_DEVICE;
}

const uint8_t * XInputDevice::get_hid_descriptor_report_cb(uint8_t itf) 
{
    (void)itf;
    return nullptr;
}

const uint8_t * XInputDevice::get_descriptor_configuration_cb(uint8_t index) 
{
    (void)index;
    return XInput::DESC_CONFIGURATION;
}

const uint8_t * XInputDevice::get_descriptor_device_qualifier_cb() 
{
    return nullptr;
}
