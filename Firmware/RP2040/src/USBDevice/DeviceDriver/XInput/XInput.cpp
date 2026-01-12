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

    // L1 -> macro X (A) con cola de 1s
    static bool            l1_macro_active   = false;
    static absolute_time_t l1_end_time;
    static uint64_t        l1_last_tap_ms    = 0;
    static bool            l1_tap_state      = false;

    // AIM ASSIST (stick izquierdo)
    static uint64_t        aim_last_time_ms  = 0;
    static int16_t         aim_jitter_x      = 0;
    static int16_t         aim_jitter_y      = 0;

    // R2 turbo X (A) con bloqueo si hay L2
    static bool            r2_held           = false;
    static bool            r2_turbo_active   = false;
    static bool            r2_blocked_by_l2  = false;
    static uint64_t        r2_last_tap_ms    = 0;
    static bool            r2_tap_state      = false;

    // Triángulo turbo (doble tap)
    static bool            tri_was_pressed   = false;
    static bool            tri_turbo_active  = false;
    static uint64_t        tri_last_press_ms = 0;
    static uint64_t        tri_last_tap_ms   = 0;
    static bool            tri_tap_state     = false;

    if (gamepad.new_pad_in())
    {
        // Reinicia todo el reporte
        in_report_ = XInput::InReport{};

        Gamepad::PadIn gp_in = gamepad.get_pad_in();
        const uint16_t btn   = gp_in.buttons;

        absolute_time_t now_time = get_absolute_time();
        uint64_t        now_ms   = to_ms_since_boot(now_time);

        // =========================================================
        // 1. HAIR TRIGGERS
        // =========================================================
        const bool trig_l_pressed = (gp_in.trigger_l != 0);
        const bool trig_r_pressed = (gp_in.trigger_r != 0);

        uint8_t final_trig_l = trig_l_pressed ? 255 : 0;
        uint8_t final_trig_r = trig_r_pressed ? 255 : 0;

        // =========================================================
        // 2. STICKS BASE (espacio final, el que ve el juego)
        // =========================================================
        int16_t base_lx = gp_in.joystick_lx;
        int16_t base_ly = Range::invert(gp_in.joystick_ly);
        int16_t base_rx = gp_in.joystick_rx;
        int16_t base_ry = Range::invert(gp_in.joystick_ry);

        int16_t out_lx = base_lx;
        int16_t out_ly = base_ly;
        int16_t out_rx = base_rx;
        int16_t out_ry = base_ry;

        auto clamp16 = [](int16_t v) -> int16_t {
            if (v < -32768) return -32768;
            if (v >  32767) return  32767;
            return v;
        };

        // Magnitud^2 de cada stick (para límites 80% / 95%)
        int32_t magL2 = static_cast<int32_t>(base_lx) * base_lx +
                        static_cast<int32_t>(base_ly) * base_ly;
        int32_t magR2 = static_cast<int32_t>(base_rx) * base_rx +
                        static_cast<int32_t>(base_ry) * base_ry;

        // =========================================================
        // 3. R2 TURBO X (A) + BLOQUEO POR L2
        // =========================================================
        static const uint64_t R2_TURBO_INTERVAL_MS = 50;

        if (trig_r_pressed)
        {
            if (!r2_held)
            {
                r2_held          = true;
                r2_blocked_by_l2 = trig_l_pressed;
                r2_turbo_active  = !r2_blocked_by_l2;
                r2_last_tap_ms   = now_ms;
                r2_tap_state     = true;
            }
            else
            {
                if (trig_l_pressed)
                {
                    r2_blocked_by_l2 = true;
                    r2_turbo_active  = false;
                    r2_tap_state     = false;
                }
            }
        }
        else
        {
            r2_held          = false;
            r2_blocked_by_l2 = false;
            r2_turbo_active  = false;
            r2_tap_state     = false;
        }

        // =========================================================
        // 4. TRIÁNGULO TURBO (doble tap ≤ 300 ms)
        // =========================================================
        static const uint64_t TRI_DOUBLE_TAP_MAX_MS = 300;
        static const uint64_t TRI_TURBO_INTERVAL_MS = 50;

        bool tri_pressed = (btn & Gamepad::BUTTON_Y) != 0;

        if (tri_pressed)
        {
            if (!tri_was_pressed)
            {
                if (now_ms - tri_last_press_ms <= TRI_DOUBLE_TAP_MAX_MS)
                {
                    tri_turbo_active = true;
                    tri_last_tap_ms  = now_ms;
                    tri_tap_state    = true;
                }
                else
                {
                    tri_turbo_active = false;
                    tri_tap_state    = false;
                }

                tri_last_press_ms = now_ms;
                tri_was_pressed   = true;
            }
        }
        else
        {
            tri_was_pressed  = false;
            tri_turbo_active = false;
            tri_tap_state    = false;
        }

        // =========================================================
        // 5. AIM ASSIST (stick izquierdo, solo con R2)
        // =========================================================
        {
            static const int16_t AIM_CENTER_MAX  = 26000;  // ~0.8 * 32767
            static const int32_t AIM_CENTER_MAX2 =
                static_cast<int32_t>(AIM_CENTER_MAX) * AIM_CENTER_MAX;

            if (final_trig_r && magL2 <= AIM_CENTER_MAX2)
            {
                if (now_ms - aim_last_time_ms > 35)
                {
                    aim_last_time_ms = now_ms;

                    const int16_t JITTER = 6000;
                    aim_jitter_x = static_cast<int16_t>(
                        (rand() % (2 * JITTER + 1)) - JITTER
                    );
                    aim_jitter_y = static_cast<int16_t>(
                        (rand() % (2 * JITTER + 1)) - JITTER
                    );
                }

                out_lx = clamp16(static_cast<int16_t>(out_lx + aim_jitter_x));
                out_ly = clamp16(static_cast<int16_t>(out_ly + aim_jitter_y));
            }
            else
            {
                aim_jitter_x = 0;
                aim_jitter_y = 0;
                out_lx       = base_lx;
                out_ly       = base_ly;
            }
        }

        // =========================================================
        // 6. ANTI-RECOIL (stick derecho Y, con R2 PERO NO en turbo)
        //
        //   - Solo si el stick derecho está dentro del 95% del recorrido.
        //   - Primer 1.5 s: 12500.
        //   - Después: 10000.
        //   - Siempre hacia ABAJO (cambiado aquí).
        // =========================================================
        {
            static const int16_t RECOIL_MAX   = 31100; // ~0.95 * 32767
            static const int32_t RECOIL_MAX2 =
                static_cast<int32_t>(RECOIL_MAX) * RECOIL_MAX;

            if (final_trig_r && !r2_turbo_active)
            {
                if (!is_shooting)
                {
                    is_shooting     = true;
                    shot_start_time = now_time;
                }

                out_ry = base_ry;

                if (magR2 <= RECOIL_MAX2)
                {
                    int64_t time_shooting_us = absolute_time_diff_us(
                        shot_start_time,
                        now_time
                    );

                    static const int64_t STRONG_DURATION_US = 1500000; // 1.5 s
                    static const int16_t RECOIL_STRONG      = 12500;
                    static const int16_t RECOIL_WEAK        = 10000;

                    int16_t recoil_force =
                        (time_shooting_us < STRONG_DURATION_US)
                            ? RECOIL_STRONG
                            : RECOIL_WEAK;

                    int32_t mag  = (base_ry >= 0) ? base_ry : -base_ry;
                    mag += recoil_force;
                    if (mag > RECOIL_MAX) mag = RECOIL_MAX;

                    // ***** CAMBIO: siempre hacia ABAJO *****
                    int32_t sign = -1;   // antes dependía de base_ry
                    out_ry = (int16_t)(sign * mag);
                }
            }
            else
            {
                is_shooting = false;
                out_ry      = base_ry;
            }
        }

        // =========================================================
        // 7. DPAD
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
        // 8. BOTONES BÁSICOS + REMAPS
        // =========================================================
        if (btn & Gamepad::BUTTON_RB)
            in_report_.buttons[1] |= XInput::Buttons1::RB;

        if (btn & Gamepad::BUTTON_X)
            in_report_.buttons[1] |= XInput::Buttons1::X;
        if (btn & Gamepad::BUTTON_A)
            in_report_.buttons[1] |= XInput::Buttons1::A;
        if (btn & Gamepad::BUTTON_B)
            in_report_.buttons[1] |= XInput::Buttons1::B;

        if (tri_pressed)
            in_report_.buttons[1] |= XInput::Buttons1::Y;

        if (btn & Gamepad::BUTTON_L3)
            in_report_.buttons[0] |= XInput::Buttons0::L3;
        if (btn & Gamepad::BUTTON_R3)
            in_report_.buttons[0] |= XInput::Buttons0::R3;

        if (btn & Gamepad::BUTTON_START)
            in_report_.buttons[0] |= XInput::Buttons0::START;

        // Botón PS: HOME + flecha IZQ y DER mantenidas
        if (btn & Gamepad::BUTTON_SYS)
        {
            in_report_.buttons[1] |= XInput::Buttons1::HOME;
            in_report_.buttons[0] |= (XInput::Buttons0::DPAD_LEFT |
                                      XInput::Buttons0::DPAD_RIGHT);
        }

        // SELECT → LB
        if (btn & Gamepad::BUTTON_BACK)
        {
            in_report_.buttons[1] |= XInput::Buttons1::LB;
        }

        // MUTE → BACK
        if (btn & Gamepad::BUTTON_MISC)
        {
            in_report_.buttons[0] |= XInput::Buttons0::BACK;
        }

        // =========================================================
        // 9. L1 MACRO → X/A TURBO + COLA 1s
        // =========================================================
        static const uint64_t L1_TURBO_INTERVAL_MS = 50;

        if (btn & Gamepad::BUTTON_LB)
        {
            l1_macro_active = true;
            l1_end_time     = make_timeout_time_ms(1000);
        }

        if (l1_macro_active)
        {
            if (now_ms - l1_last_tap_ms > L1_TURBO_INTERVAL_MS)
            {
                l1_last_tap_ms = now_ms;
                l1_tap_state   = !l1_tap_state;
            }

            if (l1_tap_state)
                in_report_.buttons[1] |= XInput::Buttons1::A;

            if (!(btn & Gamepad::BUTTON_LB) && time_reached(l1_end_time))
            {
                l1_macro_active = false;
                l1_tap_state    = false;
            }
        }

        // =========================================================
        // 10. R2 TURBO X (A) – salida
        // =========================================================
        if (r2_turbo_active)
        {
            if (now_ms - r2_last_tap_ms > R2_TURBO_INTERVAL_MS)
            {
                r2_last_tap_ms = now_ms;
                r2_tap_state   = !r2_tap_state;
            }

            if (r2_tap_state)
                in_report_.buttons[1] |= XInput::Buttons1::A;
        }

        // =========================================================
        // 11. TRIÁNGULO TURBO – salida
        // =========================================================
        if (tri_turbo_active)
        {
            if (now_ms - tri_last_tap_ms > TRI_TURBO_INTERVAL_MS)
            {
                tri_last_tap_ms = now_ms;
                tri_tap_state   = !tri_tap_state;
            }

            if (tri_tap_state)
                in_report_.buttons[1] |= XInput::Buttons1::Y;
        }

        // =========================================================
        // 12. ASIGNAR TRIGGERS Y STICKS FINALES
        // =========================================================
        in_report_.trigger_l   = final_trig_l;
        in_report_.trigger_r   = final_trig_r;
        in_report_.joystick_lx = out_lx;
        in_report_.joystick_ly = out_ly;
        in_report_.joystick_rx = out_rx;
        in_report_.joystick_ry = out_ry;

        // =========================================================
        // 13. ENVIAR REPORTE XINPUT
        // =========================================================
        if (tud_suspended())
            tud_remote_wakeup();

        tud_xinput::send_report(reinterpret_cast<uint8_t*>(&in_report_),
                                sizeof(XInput::InReport));
    }

    // =============================================================
    // 14. RUMBLE
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
