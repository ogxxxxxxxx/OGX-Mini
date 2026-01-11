#include <cstring>
#include <cstdlib> 
#include "pico/time.h"
#include "USBDevice/DeviceDriver/XInput/tud_xinput/tud_xinput.h"
#include "USBDevice/DeviceDriver/XInput/XInput.h"

void XInputDevice::initialize() 
{
    class_driver_ = *tud_xinput::class_driver();
}

void XInputDevice::process(const uint8_t idx, Gamepad& gamepad)
{
    // Variables estáticas para las macros y tiempos
    static absolute_time_t shot_start_time;
    static bool is_shooting = false;
    
    // Variables para la Macro L1 (Spam Jump)
    static bool jump_macro_active = false;
    static absolute_time_t jump_end_time;
    static uint64_t last_tap_time = 0;
    static bool tap_state = false;

    if (gamepad.new_pad_in())
    {
        in_report_.buttons[0] = 0;
        in_report_.buttons[1] = 0;

        Gamepad::PadIn gp_in = gamepad.get_pad_in();

        // --- 1. HAIR TRIGGERS (Gatillos instantáneos) ---
        uint8_t final_trig_l = (gp_in.trigger_l > 5) ? 255 : 0;
        uint8_t final_trig_r = (gp_in.trigger_r > 5) ? 255 : 0;

        // --- 2. STICKY AIM ASSIST (JITTER) ---
        int8_t stick_l_x = gp_in.joystick_lx;
        int8_t stick_l_y = gp_in.joystick_ly;
        if (final_trig_l > 50 || final_trig_r > 50) {
            int8_t jitter_x = (rand() % 9) - 4; 
            int8_t jitter_y = (rand() % 9) - 4;
            stick_l_x = Range::clamp(stick_l_x + jitter_x, -128, 127);
            stick_l_y = Range::clamp(stick_l_y + jitter_y, -128, 127);
        }

        // --- 3. ANTI-RECOIL DINÁMICO (1 SEGUNDO) ---
        int8_t stick_r_y = gp_in.joystick_ry;
        if (final_trig_r > 50) {
            if (!is_shooting) {
                is_shooting = true;
                shot_start_time = get_absolute_time();
            }
            int64_t time_shooting = absolute_time_diff_us(shot_start_time, get_absolute_time());
            int8_t recoil_force = (time_shooting < 1000000) ? 25 : 12;
            stick_r_y = Range::clamp(stick_r_y - recoil_force, -128, 127);
        } else {
            is_shooting = false;
        }

        // --- 4. DROP SHOT INTELIGENTE ---
        if (final_trig_r > 50 && final_trig_l < 50) {
            in_report_.buttons[1] |= XInput::Buttons1::B;
        }

        // --- 5. MACRO L1 -> SPAM JUMP (X X X X) + 1 SEG EXTRA ---
        if (gp_in.buttons & Gamepad::BUTTON_LB) { // L1 físico
            jump_macro_active = true;
            jump_end_time = make_timeout_time_ms(1000); // Se actualiza mientras se presiona
        }

        if (jump_macro_active) {
            uint64_t now = to_ms_since_boot(get_absolute_time());
            // Crea el efecto de apretar y soltar (Turbo) cada 50ms
            if (now - last_tap_time > 50) {
                tap_state = !tap_state;
                last_tap_time = now;
            }

            if (tap_state) {
                in_report_.buttons[1] |= XInput::Buttons1::A; // A es X en PS
            }

            // Si se soltó L1 y ya pasó el segundo de gracia, apagamos la macro
            if (!(gp_in.buttons & Gamepad::BUTTON_LB) && time_reached(jump_end_time)) {
                jump_macro_active = false;
            }
        }

        // --- 6. REMAPEOS: SHARE -> L1 / TOUCHPAD -> MAPA ---
        if (gp_in.buttons & Gamepad::BUTTON_BACK) {
            in_report_.buttons[1] |= XInput::Buttons1::LB; // Share físico lanza L1
        }
        if (gp_in.buttons & Gamepad::BUTTON_TOUCHPAD) {
            in_report_.buttons[0] |= XInput::Buttons0::BACK; // Touchpad abre Mapa
        }

        // --- RESTO DE BOTONES ---
        if (gp_in.buttons & Gamepad::BUTTON_START) in_report_.buttons[0] |= XInput::Buttons0::START;
        if (gp_in.buttons & Gamepad::BUTTON_L3)    in_report_.buttons[0] |= XInput::Buttons0::L3;
        if (gp_in.buttons & Gamepad::BUTTON_R3)    in_report_.buttons[0] |= XInput::Buttons0::R3;
        if (gp_in.buttons & Gamepad::BUTTON_X)     in_report_.buttons[1] |= XInput::Buttons1::X;
        if (gp_in.buttons & Gamepad::BUTTON_A)     in_report_.buttons[1] |= XInput::Buttons1::A;
        if (gp_in.buttons & Gamepad::BUTTON_Y)     in_report_.buttons[1] |= XInput::Buttons1::Y;
        if (gp_in.buttons & Gamepad::BUTTON_B)     in_report_.buttons[1] |= XInput::Buttons1::B;
        if (gp_in.buttons & Gamepad::BUTTON_RB)    in_report_.buttons[1] |= XInput::Buttons1::RB;

        // --- EJES Y GATILLOS ---
        in_report_.trigger_l = final_trig_l;
        in_report_.trigger_r = final_trig_r;
        in_report_.joystick_lx = stick_l_x;
        in_report_.joystick_ly = Range::invert(stick_l_y);
        in_report_.joystick_rx = gp_in.joystick_rx;
        in_report_.joystick_ry = Range::invert(stick_r_y);

        tud_xinput::send_report((uint8_t*)&in_report_, sizeof(XInput::InReport));
    }
}
