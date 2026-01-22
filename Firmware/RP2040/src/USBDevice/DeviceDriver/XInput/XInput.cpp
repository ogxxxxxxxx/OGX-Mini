#include <cstring>
#include <cstdlib>
#include <cmath>
#include "pico/time.h"

#include "USBDevice/DeviceDriver/XInput/tud_xinput/tud_xinput.h"
#include "USBDevice/DeviceDriver/XInput/XInput.h"

void XInputDevice::initialize()
{
    class_driver_ = *tud_xinput::class_driver();
}

void XInputDevice::process(const uint8_t idx, Gamepad& gamepad)
{
    (void)idx;

    // ===================== ESTADOS =====================
    static absolute_time_t shot_start_time;
    static bool is_shooting = false;

    static uint64_t aim_last_time_ms = 0;
    static int16_t  aim_direction = 1;
    static bool     aim_active = false;
    static uint64_t aim_interval = 80;

    if (gamepad.new_pad_in())
    {
        in_report_ = XInput::InReport{};
        Gamepad::PadIn gp_in = gamepad.get_pad_in();
        const uint16_t btn = gp_in.buttons;
        uint64_t now_ms = to_ms_since_boot(get_absolute_time());

        // ===================== TRIGGERS =====================
        const bool trig_l = gp_in.trigger_l != 0;
        const bool trig_r = gp_in.trigger_r != 0;

        uint8_t final_trig_l = trig_l ? 255 : 0;
        uint8_t final_trig_r = trig_r ? 255 : 0;

        // ===================== STICKS BASE =====================
        int16_t base_lx = gp_in.joystick_lx;
        int16_t base_ly = Range::invert(gp_in.joystick_ly);
        int16_t base_rx = gp_in.joystick_rx;
        int16_t base_ry = Range::invert(gp_in.joystick_ry);

        auto clamp16 = [](int32_t v) -> int16_t {
            if (v < -32768) return -32768;
            if (v >  32767) return  32767;
            return (int16_t)v;
        };

        // Deadzone R-stick ~5%
        const int16_t R_DZ = 1600;
        if ((base_rx * base_rx + base_ry * base_ry) < (R_DZ * R_DZ))
        {
            base_rx = 0;
            base_ry = 0;
        }

        int16_t out_lx = base_lx;
        int16_t out_ly = base_ly;
        int16_t out_rx = base_rx;
        int16_t out_ry = base_ry;

        // =====================================================
        // AIM ASSIST HUMANIZADO (OFFLINE)
        // =====================================================
        {
            int16_t abs_lx = abs(base_lx);
            int32_t magR =
                (int32_t)base_rx * base_rx +
                (int32_t)base_ry * base_ry;

            // Solo si estás disparando y no girando fuerte
            if (final_trig_r && magR < (5000 * 5000))
            {
                if (!aim_active)
                {
                    aim_active = true;
                    aim_last_time_ms = now_ms;
                    aim_interval = 60 + (rand() % 40);
                    aim_direction = (rand() & 1) ? 1 : -1;
                }

                if (now_ms - aim_last_time_ms > aim_interval)
                {
                    aim_last_time_ms = now_ms;
                    aim_interval = 60 + (rand() % 40);
                    aim_direction = -aim_direction;
                }

                // Fuerza dinámica
                int16_t assist =
                    (abs_lx < 4000)  ? 1200 :
                    (abs_lx < 9000)  ? 700  :
                                       0;

                // Jitter humano
                int16_t jitter = (rand() % 200) - 100;

                out_lx = clamp16(out_lx + (aim_direction * assist) + jitter);
            }
            else
            {
                aim_active = false;
                aim_direction = 1;
            }
        }

        // =====================================================
        // ANTI-RECOIL HUMANIZADO
        // =====================================================
        {
            const int16_t RECOIL_MAX = 32000;
            const int64_t RAMP_US = 35000;
            const int64_t HOLD_US = 60000;

            const int16_t BASE_RECOIL = 7800;

            if (final_trig_r)
            {
                if (!is_shooting)
                {
                    is_shooting = true;
                    shot_start_time = get_absolute_time();
                }

                int64_t t_us = absolute_time_diff_us(
                    shot_start_time,
                    get_absolute_time()
                );

                // Escala por input del jugador
                int16_t player_pull = abs(base_ry);
                float scale =
                    (player_pull < 2000) ? 1.0f :
                    (player_pull < 6000) ? 0.7f :
                                           0.4f;

                int16_t recoil = BASE_RECOIL;

                if (t_us < RAMP_US)
                {
                    recoil = (BASE_RECOIL * t_us) / RAMP_US;
                }

                // Ruido por bala
                recoil += (rand() % 300) - 150;
                recoil = (int16_t)(recoil * scale);

                // Si ya compensas fuerte, no intervenir
                if (base_ry < -4500)
                {
                    out_ry = base_ry;
                }
                else
                {
                    out_ry = clamp16(base_ry - recoil);
                }
            }
            else
            {
                is_shooting = false;
            }
        }

        // ===================== DPAD =====================
        switch (gp_in.dpad)
        {
            case Gamepad::DPAD_UP:    in_report_.buttons[0] |= XInput::Buttons0::DPAD_UP; break;
            case Gamepad::DPAD_DOWN:  in_report_.buttons[0] |= XInput::Buttons0::DPAD_DOWN; break;
            case Gamepad::DPAD_LEFT:  in_report_.buttons[0] |= XInput::Buttons0::DPAD_LEFT; break;
            case Gamepad::DPAD_RIGHT: in_report_.buttons[0] |= XInput::Buttons0::DPAD_RIGHT; break;
            default: break;
        }

        // ===================== BOTONES =====================
        if (btn & Gamepad::BUTTON_A) in_report_.buttons[1] |= XInput::Buttons1::A;
        if (btn & Gamepad::BUTTON_B) in_report_.buttons[1] |= XInput::Buttons1::B;
        if (btn & Gamepad::BUTTON_X) in_report_.buttons[1] |= XInput::Buttons1::X;
        if (btn & Gamepad::BUTTON_Y) in_report_.buttons[1] |= XInput::Buttons1::Y;

        if (btn & Gamepad::BUTTON_LB) in_report_.buttons[1] |= XInput::Buttons1::LB;
        if (btn & Gamepad::BUTTON_RB) in_report_.buttons[1] |= XInput::Buttons1::RB;
        if (btn & Gamepad::BUTTON_START) in_report_.buttons[0] |= XInput::Buttons0::START;
        if (btn & Gamepad::BUTTON_BACK)  in_report_.buttons[0] |= XInput::Buttons0::BACK;

        if (btn & Gamepad::BUTTON_L3) in_report_.buttons[0] |= XInput::Buttons0::L3;
        if (btn & Gamepad::BUTTON_R3) in_report_.buttons[0] |= XInput::Buttons0::R3;

        // ===================== ASIGNAR =====================
        in_report_.trigger_l = final_trig_l;
        in_report_.trigger_r = final_trig_r;
        in_report_.joystick_lx = out_lx;
        in_report_.joystick_ly = out_ly;
        in_report_.joystick_rx = out_rx;
        in_report_.joystick_ry = out_ry;

        if (tud_suspended())
            tud_remote_wakeup();

        tud_xinput::send_report(
            reinterpret_cast<uint8_t*>(&in_report_),
            sizeof(XInput::InReport)
        );
    }

    // ===================== RUMBLE =====================
    if (tud_xinput::receive_report(
            reinterpret_cast<uint8_t*>(&out_report_),
            sizeof(XInput::OutReport)) &&
        out_report_.report_id == XInput::OutReportID::RUMBLE)
    {
        Gamepad::PadOut gp_out;
        gp_out.rumble_l = out_report_.rumble_l;
        gp_out.rumble_r = out_report_.rumble_r;
        gamepad.set_pad_out(gp_out);
    }
}
