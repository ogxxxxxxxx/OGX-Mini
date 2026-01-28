#include <cstring>
#include "USBDevice/DeviceDriver/XInput/tud_xinput/tud_xinput.h"
#include "USBDevice/DeviceDriver/XInput/XInput.h"

// Contador para la ráfaga del Turbo
static uint32_t turbo_tick = 0;

void XInputDevice::initialize() 
{
    class_driver_ = *tud_xinput::class_driver();
}

void XInputDevice::process(const uint8_t idx, Gamepad& gamepad)
{
    if (gamepad.new_pad_in())
    {
        // Reiniciar estados de botones en cada reporte
        in_report_.buttons[0] = 0;
        in_report_.buttons[1] = 0;

        Gamepad::PadIn gp_in = gamepad.get_pad_in();

        // --- 1. DPAD (Cruceta) ---
        switch (gp_in.dpad)
        {
            case Gamepad::DPAD_UP:         in_report_.buttons[0] = XInput::Buttons0::DPAD_UP; break;
            case Gamepad::DPAD_DOWN:       in_report_.buttons[0] = XInput::Buttons0::DPAD_DOWN; break;
            case Gamepad::DPAD_LEFT:       in_report_.buttons[0] = XInput::Buttons0::DPAD_LEFT; break;
            case Gamepad::DPAD_RIGHT:      in_report_.buttons[0] = XInput::Buttons0::DPAD_RIGHT; break;
            case Gamepad::DPAD_UP_LEFT:    in_report_.buttons[0] = XInput::Buttons0::DPAD_UP | XInput::Buttons0::DPAD_LEFT; break;
            case Gamepad::DPAD_UP_RIGHT:   in_report_.buttons[0] = XInput::Buttons0::DPAD_UP | XInput::Buttons0::DPAD_RIGHT; break;
            case Gamepad::DPAD_DOWN_LEFT:  in_report_.buttons[0] = XInput::Buttons0::DPAD_DOWN | XInput::Buttons0::DPAD_LEFT; break;
            case Gamepad::DPAD_DOWN_RIGHT: in_report_.buttons[0] = XInput::Buttons0::DPAD_DOWN | XInput::Buttons0::DPAD_RIGHT; break;
            default: break;
        }

        // --- 2. REMAPEO DE BOTONES ---
        
        // Físico SELECT (BACK) -> Envía L1 (LB)
        if (gp_in.buttons & Gamepad::BUTTON_BACK)  in_report_.buttons[1] |= XInput::Buttons1::LB;
        
        // Físico MUTE (MISC) -> Envía SELECT (BACK)
        if (gp_in.buttons & Gamepad::BUTTON_MISC)  in_report_.buttons[0] |= XInput::Buttons0::BACK;

        // --- 3. MACRO TURBO: L1 (LB) -> EQUIS (A de Xbox/XInput) ---
        if (gp_in.buttons & Gamepad::BUTTON_LB) 
        {
            turbo_tick++;
            // Cada 5 ciclos cambia de estado (Presionado/Soltado)
            if ((turbo_tick / 5) % 2 == 0) {
                in_report_.buttons[1] |= XInput::Buttons1::A; 
            }
        } else {
            turbo_tick = 0; 
        }

        // --- 4. ASIGNACIÓN DE BOTONES RESTANTES ---
        if (gp_in.buttons & Gamepad::BUTTON_START) in_report_.buttons[0] |= XInput::Buttons0::START;
        if (gp_in.buttons & Gamepad::BUTTON_L3)    in_report_.buttons[0] |= XInput::Buttons0::L3;
        if (gp_in.buttons & Gamepad::BUTTON_R3)    in_report_.buttons[0] |= XInput::Buttons0::R3;

        if (gp_in.buttons & Gamepad::BUTTON_X)     in_report_.buttons[1] |= XInput::Buttons1::X;
        if (gp_in.buttons & Gamepad::BUTTON_A)     in_report_.buttons[1] |= XInput::Buttons1::A;
        if (gp_in.buttons & Gamepad::BUTTON_Y)     in_report_.buttons[1] |= XInput::Buttons1::Y;
        if (gp_in.buttons & Gamepad::BUTTON_B)     in_report_.buttons[1] |= XInput::Buttons1::B;
        if (gp_in.buttons & Gamepad::BUTTON_RB)    in_report_.buttons[1] |= XInput::Buttons1::RB;
        if (gp_in.buttons & Gamepad::BUTTON_SYS)   in_report_.buttons[1] |= XInput::Buttons1::HOME;

        // --- 5. GATILLOS ---
        in_report_.trigger_l = gp_in.trigger_l;
        in_report_.trigger_r = gp_in.trigger_r;

        // --- 6. STICKS Y ANTI-RECOIL ---
        // Stick Izquierdo normal
        in_report_.joystick_lx = gp_in.joystick_lx;
        in_report_.joystick_ly = Range::invert(gp_in.joystick_ly);
        
        // Stick Derecho (RX normal, RY con procesamiento para Anti-recoil)
        in_report_.joystick_rx = gp_in.joystick_rx;
        
        int16_t ry_final = Range::invert(gp_in.joystick_ry);

        // Lógica Anti-recoil: Activado al pulsar L2 y R2 simultáneamente
        // Nota: Ajustamos el umbral a 50 para que actúe rápido
        if (gp_in.trigger_l > 50 && gp_in.trigger_r > 50) 
        {
            // Fuerza del anti-recoil. Si no es suficiente, sube este valor a 3000 o 4000.
            const int16_t recoil_force = 2000; 
            
            // Verificamos desbordamiento antes de sumar
            if (ry_final > (32767 - recoil_force)) {
                ry_final = 32767;
            } else {
                ry_final += recoil_force;
            }
        }
        in_report_.joystick_ry = ry_final;

        // --- 7. ENVÍO DE DATOS ---
        if (tud_suspended()) {
            tud_remote_wakeup();
        }
        tud_xinput::send_report((uint8_t*)&in_report_, sizeof(XInput::InReport));
    }

    // Procesar Rumble (vibración) entrante
    if (tud_xinput::receive_report(reinterpret_cast<uint8_t*>(&out_report_), sizeof(XInput::OutReport)) &&
        out_report_.report_id == XInput::OutReportID::RUMBLE)
    {
        Gamepad::PadOut gp_out;
        gp_out.rumble_l = out_report_.rumble_l;
        gp_out.rumble_r = out_report_.rumble_r;
        gamepad.set_pad_out(gp_out);
    }
}

// Callbacks estándar de la clase
uint16_t XInputDevice::get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen) 
{
    std::memcpy(buffer, &in_report_, sizeof(XInput::InReport));
    return sizeof(XInput::InReport);
}

void XInputDevice::set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize) {}

bool XInputDevice::vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request) 
{
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
    return nullptr;
}

const uint8_t * XInputDevice::get_descriptor_configuration_cb(uint8_t index) 
{
    return XInput::DESC_CONFIGURATION;
}

const uint8_t * XInputDevice::get_descriptor_device_qualifier_cb() 
{
    return nullptr;
}
