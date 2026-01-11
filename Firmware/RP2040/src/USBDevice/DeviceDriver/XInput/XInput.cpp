#include <cstring>
#include <cstdlib>        // rand()
#include "pico/time.h"    // absolute_time, time_diff, etc.

#include "USBDevice/DeviceDriver/XInput/tud_xinput/tud_xinput.h"
#include "USBDevice/DeviceDriver/XInput/XInput.h"

void XInputDevice::initialize() 
{
    class_driver_ = *tud_xinput::class_driver();
}

void XInputDevice::process(const uint8_t idx, Gamepad& gamepad)
{
    (void)idx;

    // ---------- ESTADO ESTÁTICO PARA MACROS / TIEMPOS ----------
    static absolute_time_t shot_start_time;
    static bool            is_shooting = false;

    // Macro L1 (spam jump)
    static bool            jump_macro_active = false;
    static absolute_time_t jump_end_time;
    static uint64_t        last_tap_time = 0;
    static bool            tap_state    = false;

    if (gamepad.new_pad_in())
    {
        // Reiniciamos TODO el in_report_ (report_size se rellena en el ctor)
        in_report_ = XInput::InReport{};

        Gamepad::PadIn gp_in = gamepad.get_pad_in();
        const uint16_t btn    = gp_in.buttons;

        // =========================================================
        // 1. HAIR TRIGGERS (cualquier valor cuenta como apretado)
        // =========================================================
        const bool trig_l_pressed = (gp_in.trigger_l != 0);
        const bool trig_r_pressed = (gp_in.trigger_r != 0);

        uint8_t final_trig_l = trig_l_pressed ? 255 : 0;
        uint8_t final_trig_r = trig_r_pressed ? 255 : 0;

        // =========================================================
        // 2. STICKS BASE (int16, pero rango real -128..127)
        // =========================================================
        int16_t stick_l_x = gp_in.joystick_lx;
        int16_t stick_l_y = gp_in.joystick_ly;
        int16_t stick_r_x = gp_in.joystick_rx;
        int16_t stick_r_y = gp_in.joystick_ry;

        // =========================================================
        // 3. STICKY AIM (JITTER EN STICK IZQUIERDO AL DISPARAR)
        //    Siempre que dispares (R2 o L2) hace micro-movimientos
        // =========================================================
        if (final_trig_l || final_trig_r)
        {
            // Un jitter más fuerte para que se note:
            // rango aprox. -12..+12, luego se clamp a [-128,127]
            int16_t jitter_x = static_cast<int16_t>((rand() % 25) - 12);
            int16_t jitter_y = static_cast<int16_t>((rand() % 25) - 12);

            stick_l_x = Range::clamp<int16_t>(stick_l_x + jitter_x, -128, 127);
            stick_l_y = Range::clamp<int16_t>(stick_l_y + jitter_y, -128, 127);
        }

        // =========================================================
        // 4. ANTI-RECOIL DINÁMICO (STICK DERECHO Y, CUANDO R2)
        //
        //   - Primer segundo: fuerza 40 (baja MUCHO la mira)
        //   - Luego: fuerza 20 (baja pero más suave)
        // =========================================================
        if (final_trig_r)
        {
            if (!is_shooting)
            {
                is_shooting     = true;
                shot_start_time = get_absolute_time();
            }

            int64_t time_shooting_us = absolute_time_diff_us(shot_start_time, get_absolute_time());

            int16_t recoil_force = (time_shooting_us < 1000000)
                                 ? 40   // primer segundo
                                 : 20;  // después

            stick_r_y = Range::clamp<int16_t>(stick_r_y - recoil_force, -128, 127);
        }
        else
        {
            is_shooting = false;
        }

        // =========================================================
        // 5. DPAD
        // =========================================================
        switch (gp_in.dpad)
        {
            case Gamepad::DPAD_UP:
                in_report_.buttons[0] |= XInput::Buttons0::DPAD_UP;
                break;
            case Gamepad::DPAD_DOWN:
                in_report_.buttons[0] |= XInput::Buttons0::DPAD_DOWN;
                break;
            case Gamepad::DPAD_LEFT:
                in_report_.buttons[0] |= XInput::Buttons0::DPAD_LEFT;
                break;
            case Gamepad::DPAD_RIGHT:
                in_report_.buttons[0] |= XInput::Buttons0::DPAD_RIGHT;
                break;
            case Gamepad::DPAD_UP_LEFT:
                in_report_.buttons[0] |= XInput::Buttons0::DPAD_UP | XInput::Buttons0::DPAD_LEFT;
                break;
            case Gamepad::DPAD_UP_RIGHT:
                in_report_.buttons[0] |= XInput::Buttons0::DPAD_UP | XInput::Buttons0::DPAD_RIGHT;
                break;
            case Gamepad::DPAD_DOWN_LEFT:
                in_report_.buttons[0] |= XInput::Buttons0::DPAD_DOWN | XInput::Buttons0::DPAD_LEFT;
                break;
            case Gamepad::DPAD_DOWN_RIGHT:
                in_report_.buttons[0] |= XInput::Buttons0::DPAD_DOWN | XInput::Buttons0::DPAD_RIGHT;
                break;
            default:
                break;
        }

        // =========================================================
        // 6. BOTONES BÁSICOS + REMAPS
        // =========================================================
        // L1 físico (LB): se queda igual, y además tiene macro (abajo)
        if (btn & Gamepad::BUTTON_LB)    in_report_.buttons[1] |= XInput::Buttons1::LB;

        // R1
        if (btn & Gamepad::BUTTON_RB)    in_report_.buttons[1] |= XInput::Buttons1::RB;

        // A/B/X/Y
        if (btn & Gamepad::BUTTON_X)     in_report_.buttons[1] |= XInput::Buttons1::X;
        if (btn & Gamepad::BUTTON_A)     in_report_.buttons[1] |= XInput::Buttons1::A;
        if (btn & Gamepad::BUTTON_Y)     in_report_.buttons[1] |= XInput::Buttons1::Y;
        if (btn & Gamepad::BUTTON_B)     in_report_.buttons[1] |= XInput::Buttons1::B;

        // Sticks pulsados
        if (btn & Gamepad::BUTTON_L3)    in_report_.buttons[0] |= XInput::Buttons0::L3;
        if (btn & Gamepad::BUTTON_R3)    in_report_.buttons[0] |= XInput::Buttons0::R3;

        // START
        if (btn & Gamepad::BUTTON_START) in_report_.buttons[0] |= XInput::Buttons0::START;

        // HOME / PS
        if (btn & Gamepad::BUTTON_SYS)   in_report_.buttons[1] |= XInput::Buttons1::HOME;

        // --- REMAP: BOTÓN SELECT (BACK) PASA A SER OTRO L1 (LB) ---
        //     "select es l1 (que se aprete solo l1 no select)"
        if (btn & Gamepad::BUTTON_BACK)
        {
            in_report_.buttons[1] |= XInput::Buttons1::LB;
            // OJO: NO ponemos Buttons0::BACK -> ya no existe SELECT real.
        }

        // --- MUTE (BUTTON_MISC) YA NO APRETA SELECT ---
        //     Antes: if (btn & Gamepad::BUTTON_MISC) BACK;
        //     Ahora: no hace nada especial aquí. Si algún día
        //     sabemos qué botón es el TOUCHPAD real, lo mapeamos
        //     a Buttons0::BACK ahí.

        // =========================================================
        // 7. DROP SHOT (R2 sin L2) -> B
        // =========================================================
        if (final_trig_r && !final_trig_l)
        {
            in_report_.buttons[1] |= XInput::Buttons1::B;
        }

        // =========================================================
        // 8. MACRO L1 (LB) -> SPAM JUMP (A)
        // =========================================================
        if (btn & Gamepad::BUTTON_LB)
        {
            jump_macro_active = true;
            jump_end_time     = make_timeout_time_ms(1000); // 1 s después de soltar
        }

        if (jump_macro_active)
        {
            uint64_t now = to_ms_since_boot(get_absolute_time());

            // Velocidad del spam (50 ms ≈ 20 pulsos/seg)
            if (now - last_tap_time > 50)
            {
                tap_state     = !tap_state;
                last_tap_time = now;
            }

            if (tap_state)
            {
                in_report_.buttons[1] |= XInput::Buttons1::A;
            }

            // Si ya no se mantiene LB y el timeout pasó, apagamos macro
            if (!(btn & Gamepad::BUTTON_LB) && time_reached(jump_end_time))
            {
                jump_macro_active = false;
                tap_state         = false;
            }
        }

        // =========================================================
        // 9. ASIGNAR TRIGGERS Y STICKS FINALES
        // =========================================================
        in_report_.trigger_l   = final_trig_l;
        in_report_.trigger_r   = final_trig_r;

        in_report_.joystick_lx = stick_l_x;
        in_report_.joystick_ly = Range::invert(stick_l_y);
        in_report_.joystick_rx = stick_r_x;
        in_report_.joystick_ry = Range::invert(stick_r_y);

        // =========================================================
        // 10. ENVIAR REPORTE XINPUT
        // =========================================================
        if (tud_suspended())
        {
            tud_remote_wakeup();
        }

        tud_xinput::send_report(reinterpret_cast<uint8_t*>(&in_report_), sizeof(XInput::InReport));
    }

    // =============================================================
    // 11. RUMBLE (igual que el original)
    // =============================================================
    if (tud_xinput::receive_report(reinterpret_cast<uint8_t*>(&out_report_), sizeof(XInput::OutReport)) &&
        out_report_.report_id == XInput::OutReportID::RUMBLE)
    {
        Gamepad::PadOut gp_out;
        gp_out.rumble_l = out_report_.rumble_l;
        gp_out.rumble_r = out_report_.rumble_r;
        gamepad.set_pad_out(gp_out);
    }
}

uint16_t XInputDevice::get_report_cb(uint8_t itf,
                                     uint8_t report_id,
                                     hid_report_type_t report_type,
                                     uint8_t *buffer,
                                     uint16_t reqlen) 
{
    (void)itf;
    (void)report_id;
    (void)report_type;
    (void)reqlen;

    std::memcpy(buffer, &in_report_, sizeof(XInput::InReport));
    return sizeof(XInput::InReport);
}

void XInputDevice::set_report_cb(uint8_t itf,
                                 uint8_t report_id,
                                 hid_report_type_t report_type,
                                 uint8_t const *buffer,
                                 uint16_t bufsize) 
{
    (void)itf;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)bufsize;
}

bool XInputDevice::vendor_control_xfer_cb(uint8_t rhport,
                                          uint8_t stage,
                                          tusb_control_request_t const *request) 
{
    (void)rhport;
    (void)stage;
    (void)request;
    return false;
}

const uint16_t * XInputDevice::get_descriptor_string_cb(uint8_t index, uint16_t langid) 
{
    (void)langid;
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
