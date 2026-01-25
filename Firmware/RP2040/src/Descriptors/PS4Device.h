#ifndef _PS4_DEVICE_DESCRIPTORS_H_
#define _PS4_DEVICE_DESCRIPTORS_H_

#include <stdint.h>

namespace PS4Dev
{
    static constexpr uint8_t JOYSTICK_MID = 0x80;
    static constexpr uint8_t JOYSTICK_MIN = 0x00;
    static constexpr uint8_t JOYSTICK_MAX = 0xFF;

    enum Hat : uint8_t
    {
        HAT_UP        = 0x00,
        HAT_UP_RIGHT  = 0x01,
        HAT_RIGHT     = 0x02,
        HAT_DOWN_RIGHT= 0x03,
        HAT_DOWN      = 0x04,
        HAT_DOWN_LEFT = 0x05,
        HAT_LEFT      = 0x06,
        HAT_UP_LEFT   = 0x07,
        HAT_CENTER    = 0x08, // Ajustado a 0x08 que es el estándar de Sony
    };

    // Estructuras auxiliares para que el tamaño final sea de 64 bytes
    struct __attribute__((packed)) TouchpadXY
    {
        uint8_t data[3];
    };

    struct __attribute__((packed)) TouchpadData
    {
        uint8_t counter;
        TouchpadXY p1;
        TouchpadXY p2;
    };

    // ====== ESTRUCTURA CORREGIDA PARA EL COMPILADOR ======
    struct __attribute__((packed)) InReport
    {
        uint8_t reportID;      // [0]
        uint8_t leftStickX;    // [1]
        uint8_t leftStickY;    // [2]
        uint8_t rightStickX;   // [3]
        uint8_t rightStickY;   // [4]

        // Byte 5: D-pad y Botones principales
        uint8_t dpad : 4;         // Los errores decían que faltaba 'dpad'
        uint8_t buttonWest : 1;   // Square
        uint8_t buttonSouth : 1;  // Cross
        uint8_t buttonEast : 1;   // Circle
        uint8_t buttonNorth : 1;  // Triangle

        // Byte 6: Gatillos y frontales
        uint8_t buttonL1 : 1;
        uint8_t buttonR1 : 1;
        uint8_t buttonL2 : 1;
        uint8_t buttonR2 : 1;
        uint8_t buttonSelect : 1; // Share
        uint8_t buttonStart : 1;  // Options
        uint8_t buttonL3 : 1;
        uint8_t buttonR3 : 1;

        // Byte 7: Home y Touchpad
        uint8_t buttonHome : 1;     // PS Button
        uint8_t buttonTouchpad : 1;
        uint8_t reportCounter : 6;

        uint8_t leftTrigger;  // [8]
        uint8_t rightTrigger; // [9]

        // El resto hasta completar 64 bytes
        uint8_t padding[54]; 
    };

    static_assert(sizeof(InReport) == 64, "PS4Dev::InReport debe medir 64 bytes");

    // Identidad de Sony para asegurar botones de PlayStation en Warzone
    static const uint8_t STRING_LANGUAGE[]     = { 0x09, 0x04 };
    static const uint8_t STRING_MANUFACTURER[] = "Sony Interactive Entertainment";
    static const uint8_t STRING_PRODUCT[]      = "Wireless Controller";
    static const uint8_t STRING_VERSION[]      = "1.0";

    static const uint8_t* const STRING_DESCRIPTORS[] = {
        STRING_LANGUAGE, STRING_MANUFACTURER, STRING_PRODUCT, STRING_VERSION
    };

    static const uint8_t DEVICE_DESCRIPTORS[] =
    {
        0x12, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40,
        0x4C, 0x05, // Sony VID
        0xCC, 0x09, // DS4 V2 PID
        0x00, 0x01, 0x01, 0x02, 0x00, 0x01
    };

    // Report descriptor estándar de PS4
    static const uint8_t REPORT_DESCRIPTORS[] =
    {
        0x05, 0x01, 0x09, 0x05, 0xA1, 0x01, 0x85, 0x01, 0x09, 0x30, 0x09, 0x31,
        0x09, 0x32, 0x09, 0x35, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95,
        0x04, 0x81, 0x02, 0x09, 0x39, 0x15, 0x00, 0x25, 0x07, 0x35, 0x00, 0x46,
        0x3B, 0x01, 0x65, 0x14, 0x75, 0x04, 0x95, 0x01, 0x81, 0x42, 0x65, 0x00,
        0x05, 0x09, 0x19, 0x01, 0x29, 0x0E, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01,
        0x95, 0x0E, 0x81, 0x02, 0x06, 0x00, 0xFF, 0x09, 0x20, 0x75, 0x06, 0x95,
        0x01, 0x81, 0x02, 0x05, 0x01, 0x09, 0x33, 0x09, 0x34, 0x15, 0x00, 0x26,
        0xFF, 0x00, 0x75, 0x08, 0x95, 0x02, 0x81, 0x02, 0x06, 0x00, 0xFF, 0x09,
        0x21, 0x95, 0x36, 0x81, 0x02, 0x85, 0x05, 0x09, 0x22, 0x95, 0x1F, 0x91,
        0x02, 0x85, 0x03, 0x0A, 0x21, 0x27, 0x95, 0x2F, 0xB1, 0x02, 0xC0
    };

    static const uint8_t CONFIGURATION_DESCRIPTORS[] =
    {
        0x09, 0x02, 0x29, 0x00, 0x01, 0x01, 0x00, 0x80, 0x32, 0x09, 0x04, 0x00,
        0x00, 0x02, 0x03, 0x00, 0x00, 0x00, 0x09, 0x21, 0x11, 0x01, 0x00, 0x01,
        0x22, sizeof(REPORT_DESCRIPTORS) & 0xFF, (sizeof(REPORT_DESCRIPTORS) >> 8) & 0xFF,
        0x07, 0x05, 0x81, 0x03, 0x40, 0x00, 0x01, 0x07, 0x05, 0x02, 0x03, 0x40,
        0x00, 0x01
    };

} // namespace PS4Dev

#endif
