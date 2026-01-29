#include <cstring>
#include "USBDevice/DeviceDriver/XInput/tud_xinput/tud_xinput.h"
#include "USBDevice/DeviceDriver/XInput/XInput.h"

// --- INCLUDES ---
#include "Board/Config.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
// ----------------

static uint32_t turbo_tick = 0;

// VARIABLES ESTÁTICAS PARA EL AIMBOT
static int16_t aim_x = 0;
static int16_t aim_y = 0;
static int safety_timer = 0; 

// Función auxiliar para limitar valores (Clamping)
// Evita que si sumas dos números grandes, el mando se vuelva loco
int32_t clamp_axis(int32_t value) {
    if (value > 32767) return 32767;
    if (value < -32768) return -32768;
    return value;
}

void XInputDevice::initialize() 
{
    class_driver_ = *tud_xinput::class_driver();
}

void XInputDevice::process(const uint8_t idx, Gamepad& gamepad)
{
    // ====================================================================
    // 1. LEER DATOS DEL AIMBOT (UART)
    // ====================================================================
    #ifdef AIMBOT_UART_ID
    while (uart_is_readable(AIMBOT_UART_ID)) {
        uint8_t byte1 = uart_getc(AIMBOT_UART_ID);

        if (byte1 == 0xA5) {
            uint8_t data[4];
            uart_read_blocking(AIMBOT_UART_ID, data, 4);

            aim_x = (int16_t)((data[0] << 8) | data[1]);
            aim_y = (int16_t)((data[2] << 8) | data[3]);
            
            safety_timer = 200; // 200ms de vida para la orden

            #ifdef LED_INDICATOR_PIN
            gpio_xor_mask(1u << LED_INDICATOR_PIN); 
            #endif
        }
    }
    #endif

    // ====================================================================
    // 2. SISTEMA DE SEGURIDAD
    // ====================================================================
    if (safety_timer > 0) {
        safety_timer--;
    } else {
        aim_x = 0;
        aim_y = 0;
    }

    // ====================================================================
    // 3. PROCESAR INPUT FÍSICO
    // ====================================================================
    bool physical_active = gamepad.new_pad_in();

    // Procesamos siempre si hay actividad física O del aimbot
    if (physical_active || aim_x != 0 || aim_y != 0)
    {
        in_report_.buttons[0] = 0;
        in_report_.buttons[1] = 0;

        Gamepad::PadIn gp_in = gamepad.get_pad_in();

        // Mapeo de botones normal
        switch (gp_in.dpad) {
            case Gamepad::DPAD_UP:         in_report_.buttons[0] |= XInput::Buttons0::DPAD_UP; break;
            case Gamepad::DPAD_DOWN:       in_report_.buttons[0] |= XInput::Buttons0::DPAD_DOWN; break;
            case Gamepad::DPAD_LEFT:       in_report_.buttons[0] |= XInput::Buttons0::DPAD_LEFT; break;
            case Gamepad::DPAD_RIGHT:      in_report_.buttons[0] |= XInput::Buttons0::DPAD_RIGHT; break;
            case Gamepad::DPAD_UP_LEFT:    in_report_.buttons[0] |= XInput::Buttons0::DPAD_UP | XInput::Buttons0::DPAD_LEFT; break;
            case Gamepad::DPAD_UP_RIGHT:   in_report_.buttons[0] |= XInput::Buttons0::DPAD_UP | XInput::Buttons0::DPAD_RIGHT; break;
            case Gamepad::DPAD_DOWN_LEFT:  in_report_.buttons[0] |= XInput::Buttons0::DPAD_DOWN | XInput::Buttons0::DPAD_LEFT; break;
            case Gamepad::DPAD_DOWN_RIGHT: in_report_.buttons[0] |= XInput::Buttons0::DPAD_DOWN | XInput::Buttons0::DPAD_RIGHT; break;
        }
        if (gp_in.buttons & Gamepad::BUTTON_START) in_report_.buttons[0] |= XInput::Buttons0::START;
        if (gp_in.buttons & Gamepad::BUTTON_BACK)  in_report_.buttons[1] |= XInput::Buttons1::LB;
        if (gp_in.buttons & Gamepad::BUTTON_L3)    in_report_.buttons[0] |= XInput::Buttons0::L3;
        if (gp_in.buttons & Gamepad::BUTTON_R3)    in_report_.buttons[0] |= XInput::Buttons0::R3;
        if (gp_in.buttons & Gamepad::BUTTON_MISC)  in_report_.buttons[0] |= XInput::Buttons0::BACK;
        if (gp_in.buttons & Gamepad::BUTTON_X)     in_report_.buttons[1] |= XInput::Buttons1::X;
        if (gp_in.buttons & Gamepad::BUTTON_A)     in_report_.buttons[1] |= XInput::Buttons1::A;
        if (gp_in.buttons & Gamepad::BUTTON_Y)     in_report_.buttons[1] |= XInput::Buttons1::Y;
        if (gp_in.buttons & Gamepad::BUTTON_B)     in_report_.buttons[1] |= XInput::Buttons1::B;
        if (gp_in.buttons & Gamepad::BUTTON_RB)    in_report_.buttons[1] |= XInput::Buttons1::RB;
        if (gp_in.buttons & Gamepad::BUTTON_SYS)   in_report_.buttons[1] |= XInput::Buttons1::HOME;
        
        if (gp_in.buttons & Gamepad::BUTTON_LB) {
            turbo_tick++;
            if ((turbo_tick / 5) % 2 == 0) in_report_.buttons[1] |= XInput::Buttons1::A; 
        } else {
            turbo_tick = 0; 
        }

        in_report_.trigger_l = (gp_in.trigger_l > 13) ? 255 : 0;
        in_report_.trigger_r = (gp_in.trigger_r > 13) ? 255 : 0;
        
        // Sticks Izquierdos (Movimiento personaje)
        in_report_.joystick_lx = gp_in.joystick_lx;
        in_report_.joystick_ly = Range::invert(gp_in.joystick_ly);


        // ====================================================================
        // 4. MEZCLA INTELIGENTE (TU MOVIMIENTO + AIMBOT)
        // ====================================================================
        
        // 1. Tomamos tu movimiento físico real
        int32_t final_rx = gp_in.joystick_rx;
        int32_t final_ry = Range::invert(gp_in.joystick_ry);

        // 2. Le SUMAMOS el movimiento del aimbot (si existe)
        #ifdef AIMBOT_UART_ID
        if (aim_x != 0 || aim_y != 0) {
            final_rx += aim_x;
            final_ry += aim_y;
        }
        #endif

        // 3. Limitamos para que no se pase del máximo (Clamp)
        // Si tú das +30000 y el aimbot da +5000, daría 35000 (error).
        // Esto lo recorta a 32767.
        in_report_.joystick_rx = (int16_t)clamp_axis(final_rx);
        in_report_.joystick_ry = (int16_t)clamp_axis(final_ry);

        // ====================================================================

        if (tud_suspended()) tud_remote_wakeup();
        tud_xinput::send_report((uint8_t*)&in_report_, sizeof(XInput::InReport));
    }

    if (tud_xinput::receive_report(reinterpret_cast<uint8_t*>(&out_report_), sizeof(XInput::OutReport)) &&
        out_report_.report_id == XInput::OutReportID::RUMBLE)
    {
        Gamepad::PadOut gp_out;
        gp_out.rumble_l = out_report_.rumble_l;
        gp_out.rumble_r = out_report_.rumble_r;
        gamepad.set_pad_out(gp_out);
    }
}

// CALLBACKS
uint16_t XInputDevice::get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen) 
{
    std::memcpy(buffer, &in_report_, sizeof(XInput::InReport));
    return sizeof(XInput::InReport);
}
void XInputDevice::set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize) {}
bool XInputDevice::vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request) { return false; }
const uint16_t * XInputDevice::get_descriptor_string_cb(uint8_t index, uint16_t langid) 
{
    const char *value = reinterpret_cast<const char*>(XInput::DESC_STRING[index]);
    return get_string_descriptor(value, index);
}
const uint8_t * XInputDevice::get_descriptor_device_cb() { return XInput::DESC_DEVICE; }
const uint8_t * XInputDevice::get_hid_descriptor_report_cb(uint8_t itf) { return nullptr; }
const uint8_t * XInputDevice::get_descriptor_configuration_cb(uint8_t index) { return XInput::DESC_CONFIGURATION; }
const uint8_t * XInputDevice::get_descriptor_device_qualifier_cb() { return nullptr; }
