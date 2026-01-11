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

    // Estado para AIM ASSIST (más lento/suave)
    static uint64_t        aim_last_time_ms = 0;
    static int16_t         aim_jitter_x = 0;
    static int16_t         aim_jitter_y = 0;

    if (gamepad.new_pad_in())
    {
        // Reinicia todo el reporte (el ctor pone report_size)
        in_report_ = XInput::InReport{};

        Gamepad::PadIn gp_in = gamepad.get_pad_in();
        const uint16_t btn   = gp_in.buttons;

        // =========================================================
        // 1. HAIR TRIGGERS
        // =========================================================
        const bool trig_l_pressed = (gp_in.trigger_l != 0);
        const bool trig_r_pressed = (gp_in.trigger_r != 0);

        uint8_t final_trig_l = trig_l_pressed ? 255 : 0;
        uint8_t final_trig_r = trig_r_pressed ? 255 : 0;

        // =========================================================
        // 2. STICKS BASE EN ESPACIO "FINAL" (el que ve el juego)
        // =========================================================
        int16_t out_lx = gp_in.joystick_lx;
        int16_t out_ly = Range::invert(gp_in.joystick_ly);
        int16_t out_rx = gp_in.joystick_rx;
        int16_t out_ry = Range::invert(gp_in.joystick_ry);

        auto clamp16 = [](int16_t v) -> int16_t {
            if (v < -32768) return -32768;
            if (v >  32767) return  32767;
            return v;
        };

        // =========================================================
        // 3. STICKY AIM (JITTER EN STICK IZQUIERDO SOLO CON R2)
        //
        //   - Solo si el stick está CERCA DEL CENTRO
        //   - Si empujas fuerte el stick, NO hay jitter.
        // =========================================================
        if (final_trig_r)   // solo cuando disparas con R2
        {
            // Zona de "apuntar fino" (cerca del centro)
            const int16_t AIM_ZONE = 12000; // ~36% del rango

            int16_t ax = (out_lx >= 0) ? out_lx : -out_lx;
            int16_t ay = (out_ly >= 0) ? out_ly : -out_ly;

            if (ax < AIM_ZONE && ay < AIM_ZONE)
            {
                // Solo aquí metemos jitter
                uint64_t now_ms = to_ms_since_boot(get_absolute_time());

                // Cambiamos el vector de jitter solo cada ~35 ms
                if (now_ms - aim_last_time_ms > 35)
                {
                    aim_last_time_ms = now_ms;

                    const int16_t JITTER = 6000; // fuerza del aim assist
                    aim_jitter_x = static_cast<int16_t>((rand() % (2 * JITTER + 1)) - JITTER);
                    aim_jitter_y = static_cast<int16_t>((rand() % (2 * JITTER + 1)) - JITTER);
                }

                out_lx = clamp16(out_lx + aim_jitter_x);
                out_ly = clamp16(out_ly + aim_jitter_y);
            }
            else
            {
                // Fuera de la zona fina, sin jitter
                aim_jitter_x = 0;
                aim_jitter_y = 0;
            }
        }
        else
        {
            // Sin disparar, no queremos jitter
            aim_jitter_x = 0;
            aim_jitter_y = 0;
        }

        // =========================================================
        // 4. ANTI-RECOIL DINÁMICO (EJE Y DERECHO, CUANDO R2)
        //
        //   - Primer 1.5 s: 12000
        //   - Después: 10000 y se queda así
        // =========================================================
        if (final_trig_r)
        {
            if (!is_shooting)
            {
                is_shooting     = true;
                shot_start_time = get_absolute_time();
            }

            int64_t time_shooting_us = absolute_time_diff_us(
                shot_start_time,
                get_absolute_time()
            );

            const int16_t RECOIL_STRONG = 12000; // hasta 1.5 s
            const int16_t RECOIL_AFTER  = 10000; // después de 1.5 s

            int16_t recoil_force =
                (time_shooting_us < 1500000) ? RECOIL_STRONG : RECOIL_AFTER;

            // Restamos para empujar la mira hacia ABAJO
            out_ry = clamp16(out_ry - recoil_force);
        }
        else
        {
            is_shooting = false;
        }

        // =========================================================
        // 5. DPAD NORMAL
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
        // L1 físico: SOLO macro (abajo), NO manda LB normal aquí

        // R1 normal
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

        // --- BOTÓN PLAYSTATION (SYS) ---
        // HOME + DPAD IZQ + DERECHA MANTENIDAS
        if (btn & Gamepad::BUTTON_SYS)
        {
            in_report_.buttons[1] |= XInput::Buttons1::HOME;
            in_report_.buttons[0] |= (XInput::Buttons0::DPAD_LEFT |
                                      XInput::Buttons0::DPAD_RIGHT);
        }

        // --- REMAP: SELECT → LB (L1 virtual) ---
        if (btn & Gamepad::BUTTON_BACK)
        {
            in_report_.buttons[1] |= XInput::Buttons1::LB;
            // No ponemos Buttons0::BACK aquí
        }

        // --- MUTE (BUTTON_MISC) → SELECT/BACK ---
        if (btn & Gamepad::BUTTON_MISC)
        {
            in_report_.buttons[0] |= XInput::Buttons0::BACK;
        }

        // =========================================================
        // 7. DROP SHOT (R2 sin L2) -> B
        // =========================================================
        if (final_trig_r && !final_trig_l)
        {
            in_report_.buttons[1] |= XInput::Buttons1::B;
        }

        // =========================================================
        // 8. MACRO L1 (LB físico) -> SPAM JUMP (A)
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

        in_report_.joystick_lx = out_lx;
        in_report_.joystick_ly = out_ly;
        in_report_.joystick_rx = out_rx;
        in_report_.joystick_ry = out_ry;

        // =========================================================
        // 10. ENVIAR REPORTE XINPUT
        // =========================================================
        if (tud_suspended())
        {
            tud_remote_wakeup();
        }

        tud_xinput::send_report(reinterpret_cast<uint8_t*>(&in_report_),
                                sizeof(XInput::InReport));
    }

    // =============================================================
    // 11. RUMBLE (igual que el original)
    // =============================================================
    if (tud_xinput::receive_report(reinterpret_cast<uint8_t*>(&out_report_),
                                   sizeof(XInput::OutReport)) &&
        out_report_.report_id == XInput::OutReportID::RUMBLE)
    {
        Gamepad::PadOut gp_out;
        gp_out.rumble_l = out_report_.rumble_l;
        gp_out.rumble_r = out_report_.rumble_r;
        gamepad.set_pad_out(gp_out);
    }
}
