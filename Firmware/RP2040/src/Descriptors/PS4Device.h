#ifndef _PS4_DEVICE_DESCRIPTORS_H_
#define _PS4_DEVICE_DESCRIPTORS_H_

#include <stdint.h>
#include <cstring>

namespace PS4Dev {

    static constexpr uint8_t JOYSTICK_MID = 0x80;

    namespace Buttons {
        // Mapeo corregido según tus pruebas en Gamepad Tester
        static constexpr uint16_t SQUARE    = 1u << 0;   // B0
        static constexpr uint16_t CROSS     = 1u << 1;   // B1
        static constexpr uint16_t CIRCLE    = 1u << 2;   // B2
        static constexpr uint16_t TRIANGLE  = 1u << 3;   // B3
        
        static constexpr uint16_t L1        = 1u << 4;   // B4
        static constexpr uint16_t R1        = 1u << 5;   // B5
        static constexpr uint16_t L2_DIG    = 1u << 6;   // B6 (Botón digital)
        static constexpr uint16_t R2_DIG    = 1u << 7;   // B7 (Botón digital)
        static constexpr uint16_t SHARE     = 1u << 8;   // B8
        static constexpr uint16_t OPTIONS   = 1u << 9;   // B9
        static constexpr uint16_t L3        = 1u << 10;  // B10
        static constexpr uint16_t R3        = 1u << 11;  // B11
        static constexpr uint16_t PS        = 1u << 12;  // B12
        static constexpr uint16_t TOUCHPAD  = 1u << 13;  // B13
    }

    namespace Hat {
        static constexpr uint8_t UP         = 0x00;
        static constexpr uint8_t UP_RIGHT   = 0x01;
        static constexpr uint8_t RIGHT      = 0x02;
        static constexpr uint8_t DOWN_RIGHT = 0x03;
        static constexpr uint8_t DOWN       = 0x04;
        static constexpr uint8_t DOWN_LEFT  = 0x05;
        static constexpr uint8_t LEFT       = 0x06;
        static constexpr uint8_t UP_LEFT    = 0x07;
        static constexpr uint8_t CENTER     = 0x08;
    }

    #pragma pack(push, 1)
    // Report de entrada: 9 bytes (el Report ID 0x01 lo añade la librería al enviar)
    struct InReport {
        uint16_t buttons;     // 16 bits de botones
        uint8_t  hat;         // D-pad (4 bits data + 4 bits padding en descriptor)
        uint8_t  joystick_lx; // Eje X Izquierdo
        uint8_t  joystick_ly; // Eje Y Izquierdo
        uint8_t  trigger_l;   // L2 Analógico (Eje Z)
        uint8_t  joystick_rx; // Eje X Derecho (Eje Rx)
        uint8_t  joystick_ry; // Eje Y Derecho (Eje Ry)
        uint8_t  trigger_r;   // R2 Analógico (Eje Rz)

        InReport() {
            std::memset(this, 0, sizeof(InReport));
            hat         = Hat::CENTER;
            joystick_lx = JOYSTICK_MID;
            joystick_ly = JOYSTICK_MID;
            joystick_rx = JOYSTICK_MID;
            joystick_ry = JOYSTICK_MID;
            trigger_l   = 0;
            trigger_r   = 0;
        }
    };
    static_assert(sizeof(InReport) == 9, "PS4Dev::InReport size must be 9 bytes");
    #pragma pack(pop)

    // Descriptors de identificación
    static const uint8_t DEVICE_DESCRIPTORS[] = {
        0x12, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40,
        0x4C, 0x05, // Vendor ID: Sony (0x054C)
        0xC4, 0x05, // Product ID: PS4 DualShock 4 (0x05C4) -> Para que Warzone lo vea
        0x00, 0x01, 0x01, 0x02, 0x00, 0x01
    };

    // HID Report Descriptor (Optimizado para compatibilidad máxima)
    static const uint8_t REPORT_DESCRIPTORS[] = {
        0x05, 0x01,        // Usage Page (Generic Desktop)
        0x09, 0x05,        // Usage (Game Pad)
        0xA1, 0x01,        // Collection (Application)
        0x85, 0x01,        //   Report ID (1)

        // 16 Botones
        0x05, 0x09,        //   Usage Page (Button)
        0x19, 0x01,        //   Usage Minimum (1)
        0x29, 0x10,        //   Usage Maximum (16)
        0x15, 0x00,        //   Logical Minimum (0)
        0x25, 0x01,        //   Logical Maximum (1)
        0x75, 0x01,        //   Report Size (1)
        0x95, 0x10,        //   Report Count (16)
        0x81, 0x02,        //   Input (Data,Var,Abs)

        // D-Pad (Hat Switch)
        0x05, 0x01,        //   Usage Page (Generic Desktop)
        0x09, 0x39,        //   Usage (Hat switch)
        0x15, 0x00,        //   Logical Minimum (0)
        0x25, 0x07,        //   Logical Maximum (7)
        0x35, 0x00,        //   Physical Minimum (0)
        0x46, 0x3B, 0x01,  //   Physical Maximum (315)
        0x75, 0x04,        //   Report Size (4)
        0x95, 0x01,        //   Report Count (1)
        0x81, 0x42,        //   Input (Data,Var,Abs,Null State)

        // Padding de 4 bits para alinear el Hat
        0x75, 0x04, 0x95, 0x01, 0x81, 0x03, 

        // 6 Ejes: LX, LY, L2, RX, RY, R2
        0x05, 0x01,        //   Usage Page (Generic Desktop)
        0x09, 0x30,        //   Usage (X)
        0x09, 0x31,        //   Usage (Y)
        0x09, 0x32,        //   Usage (Z)  -> L2
        0x09, 0x33,        //   Usage (Rx) -> RX
        0x09, 0x34,        //   Usage (Ry) -> RY
        0x09, 0x35,        //   Usage (Rz) -> R2
        0x15, 0x00,        
        0x26, 0xFF, 0x00,  
        0x75, 0x08,        
        0x95, 0x06,        
        0x81, 0x02,        

        0xC0               // End Collection
    };

    // Configuración USB (Ajustado el wTotalLength a 41 bytes)
    static const uint8_t CONFIGURATION_DESCRIPTORS[] = {
        0x09, 0x02, 0x29, 0x00, 0x01, 0x01, 0x00, 0x80, 0xFA,
        0x09, 0x04, 0x00, 0x00, 0x02, 0x03, 0x00, 0x00, 0x00,
        0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22, sizeof(REPORT_DESCRIPTORS), 0x00,
        0x07, 0x05, 0x02, 0x03, 0x40, 0x00, 0x01,
        0x07, 0x05, 0x81, 0x03, 0x40, 0x00, 0x01
    };

} // namespace PS4Dev
#endif
