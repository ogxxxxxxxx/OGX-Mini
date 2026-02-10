#include <cstring>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>  // ← AGREGAR ESTA LÍNEA PARA printf

#include "pico/time.h" // make_timeout_time_ms, time_reached

#include "USBDevice/DeviceDriver/PS4/PS4.h"

// --------------------------------------------------------------------------------
// FEATURE REPORTS ESTÁTICOS (CON ID AL INICIO - CRÍTICO PARA WARZONE)
// --------------------------------------------------------------------------------

// Report 0x02: Calibración del controlador (37 bytes con ID incluido)
static constexpr uint8_t output_0x02[] = {
    0x02, // <--- ID REQUERIDO
    0xfe, 0xff, 0x0e, 0x00, 0x04, 0x00, 0xd4, 0x22,
    0x2a, 0xdd, 0xbb, 0x22, 0x5e, 0xdd, 0x81, 0x22, 
    0x84, 0xdd, 0x1c, 0x02, 0x1c, 0x02, 0x85, 0x1f,
    0xb0, 0xe0, 0xc6, 0x20, 0xb5, 0xe0, 0xb1, 0x20,
    0x83, 0xdf, 0x0c, 0x00
};

// Report 0x03: Definición del controlador (48 bytes con ID incluido)
static constexpr uint8_t output_0x03[] = {
    0x03, // <--- ID REQUERIDO
    0x21, 0x27, 0x04, 0xcf, 0x00, 0x2c, 0x56,
    0x08, 0x00, 0x3d, 0x00, 0xe8, 0x03, 0x04, 0x00,
    0xff, 0x7f, 0x0d, 0x0d, 0x00, 0x00, 0x00, 0x00,
    0x0D, 0x84, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Report 0xA3: Versión de firmware y fecha (49 bytes con ID incluido)
static constexpr uint8_t output_0xa3[] = {
    0xa3, // <--- ID REQUERIDO
    0x4a, 0x75, 0x6e, 0x20, 0x20, 0x39, 0x20, 0x32,  // "Jun  9 2"
    0x30, 0x31, 0x37, 0x00, 0x00, 0x00, 0x00, 0x00,  // "017"
    0x31, 0x32, 0x3a, 0x33, 0x36, 0x3a, 0x34, 0x31,  // "12:36:41"
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x08, 0xb4, 0x01, 0x00, 0x00, 0x00,
    0x07, 0xa0, 0x10, 0x20, 0x00, 0xa0, 0x02, 0x00
};

// --------------------------------------------------------------------------------
// HELPERS: MATEMÁTICAS Y CURVAS
// --------------------------------------------------------------------------------

static inline uint8_t map_signed_to_uint8(float signed_val)
{
    if (signed_val >= 0.99f) return 255;
    if (signed_val <= -0.99f) return 0;

    float mapped = signed_val * 127.5f + 127.5f;
    int out = static_cast<int>(std::round(mapped));
    
    if (out < 0) out = 0;
    if (out > 255) out = 255;
    
    return static_cast<uint8_t>(out);
}

static inline void apply_stick_steam_radial(int16_t in_x, int16_t in_y,
                                            float deadzone_fraction, float gamma, float sensitivity,
                                            uint8_t &out_x, uint8_t &out_y)
{
    constexpr float INT16_MAX_F = 32767.0f;
    constexpr float SNAP_TO_EDGE_THRESHOLD = 0.93f; 
    
    float vx = static_cast<float>(in_x) / INT16_MAX_F; 
    float vy = static_cast<float>(in_y) / INT16_MAX_F; 

    float mag = std::sqrt(vx*vx + vy*vy);

    if (mag <= deadzone_fraction || mag < 0.001f)
    {
        out_x = 128;
        out_y = 128;
        return;
    }

    if (mag > 1.0f) mag = 1.0f;

    if (mag >= SNAP_TO_EDGE_THRESHOLD)
    {
        float ux = vx / mag;
        float uy = vy / mag;
        
        out_x = map_signed_to_uint8(ux);
        out_y = map_signed_to_uint8(uy);
        return;
    }

    float adj = (mag - deadzone_fraction) / (1.0f - deadzone_fraction);
    adj = std::fmax(0.0f, std::fmin(1.0f, adj));

    float out_frac = std::pow(adj, gamma);
    out_frac *= sensitivity;
    
    if (out_frac > 1.0f) out_frac = 1.0f;

    float scale = out_frac / mag; 
    float sx = vx * scale;
    float sy = vy * scale;

    if (sx >  1.0f) sx =  1.0f;
    if (sx < -1.0f) sx = -1.0f;
    if (sy >  1.0f) sy =  1.0f;
    if (sy < -1.0f) sy = -1.0f;

    out_x = map_signed_to_uint8(sx);
    out_y = map_signed_to_uint8(sy);
}

// --------------------------------------------------------------------------------
// MÉTODOS DE LA CLASE PS4Device
// --------------------------------------------------------------------------------

void PS4Device::initialize()
{
    class_driver_ =
    {
        .name             = TUD_DRV_NAME("PS4"),
        .init             = hidd_init,
        .deinit           = hidd_deinit,
        .reset            = hidd_reset,
        .open             = hidd_open,
        .control_xfer_cb  = hidd_control_xfer_cb,
        .xfer_cb          = hidd_xfer_cb,
        .sof              = nullptr
    };
    
    // Inicializar contador de reportes
    report_counter_ = 0;
    
    printf("[PS4] Dispositivo inicializado\n");
}

void PS4Device::process(const uint8_t idx, Gamepad& gamepad)
{
    (void)idx;

    // Variables estáticas para MACROS
    static bool     mutePrev          = false;
    static absolute_time_t muteEndTime; 
    static bool     muteActive        = false;
    static constexpr uint32_t MUTE_MS = 483;

    static bool     psPrev            = false;
    static absolute_time_t psEndTime; 
    static bool     psActive          = false;
    static constexpr uint32_t PS_MS   = 350;

    Gamepad::PadIn gp_in = gamepad.get_pad_in();
    const uint16_t btn   = gp_in.buttons;

    const bool mutePressed  = (btn & Gamepad::BUTTON_MISC) != 0; 
    const bool psPressed    = (btn & Gamepad::BUTTON_SYS)  != 0; 
    const bool sharePressed = (btn & Gamepad::BUTTON_BACK) != 0;

    // Lógica Macro MUTE
    if (mutePressed && !mutePrev)
    {
        muteActive = true;
        muteEndTime = make_timeout_time_ms(MUTE_MS);
    }
    mutePrev = mutePressed;

    // Lógica Macro PS
    if (psPressed && !psPrev)
    {
        psActive = true;
        psEndTime = make_timeout_time_ms(PS_MS);
    }
    psPrev = psPressed;

    if (muteActive && time_reached(muteEndTime)) muteActive = false;
    if (psActive && time_reached(psEndTime))     psActive = false;

    // Construcción del reporte
    std::memset(&report_in_, 0, sizeof(report_in_));
    report_in_.reportID = 0x01;

    // Incrementar contador (anti-ban)
    report_counter_ = (report_counter_ + 1) & 0x3F; // 6 bits (0-63)
    report_in_.reportCounter = report_counter_;

    // Touchpad limpio
    report_in_.gamepad.touchpadActive = 0;
    report_in_.gamepad.touchpadData.p1.unpressed = 1;
    report_in_.gamepad.touchpadData.p2.unpressed = 1;

    // Sticks analógicos
    constexpr float left_deadzone   = 0.03f; 
    constexpr float right_deadzone  = 0.02f; 
    constexpr float left_gamma      = 1.8f;
    constexpr float right_gamma     = 1.3f;
    constexpr float both_sensitivity = 1.10f; 

    apply_stick_steam_radial(gp_in.joystick_lx, gp_in.joystick_ly,
                             left_deadzone, left_gamma, both_sensitivity,
                             report_in_.leftStickX, report_in_.leftStickY);

    apply_stick_steam_radial(gp_in.joystick_rx, gp_in.joystick_ry,
                             right_deadzone, right_gamma, both_sensitivity,
                             report_in_.rightStickX, report_in_.rightStickY);

    // D-PAD
    switch (gp_in.dpad)
    {
        case Gamepad::DPAD_UP:          report_in_.dpad = PS4Dev::HAT_UP;         break;
        case Gamepad::DPAD_UP_RIGHT:    report_in_.dpad = PS4Dev::HAT_UP_RIGHT;   break;
        case Gamepad::DPAD_RIGHT:       report_in_.dpad = PS4Dev::HAT_RIGHT;      break;
        case Gamepad::DPAD_DOWN_RIGHT:  report_in_.dpad = PS4Dev::HAT_DOWN_RIGHT; break;
        case Gamepad::DPAD_DOWN:        report_in_.dpad = PS4Dev::HAT_DOWN;       break;
        case Gamepad::DPAD_DOWN_LEFT:   report_in_.dpad = PS4Dev::HAT_DOWN_LEFT;  break;
        case Gamepad::DPAD_LEFT:        report_in_.dpad = PS4Dev::HAT_LEFT;       break;
        case Gamepad::DPAD_UP_LEFT:     report_in_.dpad = PS4Dev::HAT_UP_LEFT;    break;
        default:                        report_in_.dpad = PS4Dev::HAT_CENTER;     break;
    }

    // Botones principales
    const bool baseSquare = (btn & Gamepad::BUTTON_X) != 0;
    const bool baseCircle = (btn & Gamepad::BUTTON_B) != 0;

    report_in_.buttonWest  = (baseSquare || muteActive) ? 1 : 0;
    report_in_.buttonEast  = (baseCircle || muteActive) ? 1 : 0;
    report_in_.buttonSouth = (btn & Gamepad::BUTTON_A)  ? 1 : 0;
    report_in_.buttonNorth = (btn & Gamepad::BUTTON_Y)  ? 1 : 0;

    // Triggers/Shoulders
    const bool physL1 = (btn & Gamepad::BUTTON_LB) != 0; 
    const bool physR1 = (btn & Gamepad::BUTTON_RB) != 0; 
    const bool physL2 = gp_in.trigger_l;
    const bool physR2 = gp_in.trigger_r; 

    bool virtL1 = physL1;
    bool virtR1 = false;
    bool virtL2 = false;
    bool virtR2 = false;
    uint8_t trigL_val = 0;
    uint8_t trigR_val = 0;

    if (physR1)
    {
        virtR2 = true;
        trigR_val = 0xFF;
    }

    if (physR2)
    {
        virtL2 = true;
        trigL_val = 0xFF;
    }

    if (physL2)
    {
        virtR1 = true;
    }
    
    if (psActive)
    {
        virtR1 = true;
        virtL2 = true;
        trigL_val = 0xFF;
        report_in_.buttonNorth = 1;
    }

    report_in_.buttonL1 = virtL1 ? 1 : 0;
    report_in_.buttonR1 = virtR1 ? 1 : 0;
    report_in_.buttonL2 = virtL2 ? 1 : 0;
    report_in_.buttonR2 = virtR2 ? 1 : 0;
    report_in_.leftTrigger  = trigL_val;
    report_in_.rightTrigger = trigR_val;

    // Otros botones
    report_in_.buttonL3 = (btn & Gamepad::BUTTON_L3) ? 1 : 0;
    report_in_.buttonR3 = (btn & Gamepad::BUTTON_R3) ? 1 : 0;

    report_in_.buttonSelect   = sharePressed ? 1 : 0;
    report_in_.buttonStart    = (btn & Gamepad::BUTTON_START) ? 1 : 0;
    report_in_.buttonHome     = psPressed ? 1 : 0;
    report_in_.buttonTouchpad = sharePressed ? 1 : 0;

    // Enviar USB
    if (tud_suspended())
    {
        tud_remote_wakeup();
    }

    if (tud_hid_ready())
    {
        tud_hid_report(
            0, 
            reinterpret_cast<uint8_t*>(&report_in_),
            sizeof(PS4Dev::InReport)
        );
    }
}

// --------------------------------------------------------------------------------
// CALLBACKS - CON LOGGING PARA DEBUG
// --------------------------------------------------------------------------------

uint16_t PS4Device::get_report_cb(uint8_t itf, uint8_t report_id,
                                  hid_report_type_t report_type,
                                  uint8_t *buffer, uint16_t reqlen)
{
    (void)itf;
    
    // ============ LOGGING PARA DEBUG ============
    const char* type_name = "UNKNOWN";
    if (report_type == HID_REPORT_TYPE_INPUT) type_name = "INPUT";
    else if (report_type == HID_REPORT_TYPE_OUTPUT) type_name = "OUTPUT";
    else if (report_type == HID_REPORT_TYPE_FEATURE) type_name = "FEATURE";
    
    printf("[PS4] GET_REPORT: ID=0x%02X Type=%s Len=%d\n", 
           report_id, type_name, reqlen);
    // ============================================
    
    // Feature Reports
    if (report_type == HID_REPORT_TYPE_FEATURE)
    {
        if (report_id == 0x02) // Calibración
        {
            printf("[PS4] -> Enviando Report 0x02 (Calibracion)\n");
            uint16_t len = std::min<uint16_t>(reqlen, sizeof(output_0x02));
            std::memcpy(buffer, output_0x02, len);
            return len;
        }
        else if (report_id == 0x03) // Definición del controlador
        {
            printf("[PS4] -> Enviando Report 0x03 (Definicion)\n");
            uint16_t len = std::min<uint16_t>(reqlen, sizeof(output_0x03));
            std::memcpy(buffer, output_0x03, len);
            return len;
        }
        else if (report_id == 0xA3) // Versión de firmware
        {
            printf("[PS4] -> Enviando Report 0xA3 (Version)\n");
            uint16_t len = std::min<uint16_t>(reqlen, sizeof(output_0xa3));
            std::memcpy(buffer, output_0xa3, len);
            return len;
        }
        else
        {
            printf("[PS4] !!! ADVERTENCIA: Report 0x%02X NO IMPLEMENTADO !!!\n", report_id);
        }
    }
    
    // Input Reports
    if (report_type == HID_REPORT_TYPE_INPUT)
    {
        uint16_t len = std::min<uint16_t>(reqlen, sizeof(PS4Dev::InReport));
        std::memcpy(buffer, &report_in_, len);
        return len;
    }
    
    return 0;
}

void PS4Device::set_report_cb(uint8_t itf, uint8_t report_id,
                              hid_report_type_t report_type,
                              uint8_t const *buffer, uint16_t bufsize)
{
    (void)itf; (void)report_id; (void)report_type; (void)buffer; (void)bufsize;
}

bool PS4Device::vendor_control_xfer_cb(uint8_t rhport, uint8_t stage,
                                       tusb_control_request_t const *request)
{
    (void)rhport; (void)stage; (void)request;
    return false;
}

const uint16_t* PS4Device::get_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void)langid;
    
    // Index 0 es la tabla de idiomas
    if (index == 0)
    {
        static uint16_t lang_descriptor[2];
        lang_descriptor[0] = (0x03 << 8) | (2 * 1 + 2);
        lang_descriptor[1] = 0x0409; // English (US)
        return lang_descriptor;
    }
    
    // Índices 1-4: strings reales
    const char* value = reinterpret_cast<const char*>(PS4Dev::STRING_DESCRIPTORS[index]);
    return get_string_descriptor(value, index);
}

const uint8_t* PS4Device::get_descriptor_device_cb()
{
    return PS4Dev::DEVICE_DESCRIPTORS;
}

const uint8_t* PS4Device::get_hid_descriptor_report_cb(uint8_t itf)
{
    (void)itf;
    return PS4Dev::REPORT_DESCRIPTORS;
}

const uint8_t* PS4Device::get_descriptor_configuration_cb(uint8_t index)
{
    (void)index;
    return PS4Dev::CONFIGURATION_DESCRIPTORS;
}

const uint8_t* PS4Device::get_descriptor_device_qualifier_cb()
{
    return nullptr;
}
