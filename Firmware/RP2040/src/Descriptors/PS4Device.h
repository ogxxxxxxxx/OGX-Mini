#ifndef _PS4_DEVICE_DESCRIPTORS_H_
#define _PS4_DEVICE_DESCRIPTORS_H_

#include <stdint.h>
#include <string.h>

namespace PS4Dev
{
    static constexpr uint8_t JOYSTICK_MID = 0x80;
    static constexpr uint8_t JOYSTICK_MIN = 0x00;
    static constexpr uint8_t JOYSTICK_MAX = 0xFF;

    enum Hat : uint8_t
    {
        HAT_UP         = 0x00,
        HAT_UP_RIGHT   = 0x01,
        HAT_RIGHT      = 0x02,
        HAT_DOWN_RIGHT = 0x03,
        HAT_DOWN       = 0x04,
        HAT_DOWN_LEFT  = 0x05,
        HAT_LEFT       = 0x06,
        HAT_UP_LEFT    = 0x07,
        HAT_CENTER     = 0x0F,
    };

    struct __attribute__((packed)) TouchpadXY
    {
        uint8_t counter : 7;
        uint8_t unpressed : 1;
        uint8_t data[3];

        void set_x(uint16_t x) {
            data[0] = x & 0xff;
            data[1] = (data[1] & 0xf0) | ((x >> 8) & 0xf);
        }

        void set_y(uint16_t y) {
            data[1] = (data[1] & 0x0f) | ((y & 0xf) << 4);
            data[2] = y >> 4;
        }
    };

    struct __attribute__((packed)) TouchpadData {
        TouchpadXY p1;
        TouchpadXY p2;
    };

    struct __attribute__((packed)) PSSensor {
        int16_t x, y, z;
    };

    struct __attribute__((packed)) PSSensorData {
        PSSensor gyroscope;
        PSSensor accelerometer;
        uint8_t sensorReserved[5];
        uint8_t batteryLevel : 4;
        uint8_t charging : 1;
        uint8_t headphones : 1;
        uint8_t microphone : 1;
        uint8_t padding : 1;
        uint8_t sensorReserved2[2];
    };

    // Estructura InReport corregida para compatibilidad total con Warzone/Ricochet
    struct __attribute__((packed)) InReport
    {
        uint8_t reportID; // 0x01
        uint8_t leftStickX;
        uint8_t leftStickY;
        uint8_t rightStickX;
        uint8_t rightStickY;

        uint8_t dpad : 4;
        uint8_t buttonWest : 1;   // Square
        uint8_t buttonSouth : 1;  // Cross
        uint8_t buttonEast : 1;   // Circle
        uint8_t buttonNorth : 1;  // Triangle

        uint8_t buttonL1 : 1;
        uint8_t buttonR1 : 1;
        uint8_t buttonL2 : 1;
        uint8_t buttonR2 : 1;
        uint8_t buttonShare : 1;
        uint8_t buttonOptions : 1;
        uint8_t buttonL3 : 1;
        uint8_t buttonR3 : 1;

        uint8_t buttonPS : 1;
        uint8_t buttonTouchpad : 1;
        uint8_t reportCounter : 6;

        uint8_t leftTrigger;
        uint8_t rightTrigger;

        uint16_t timestamp;
        uint8_t battery;

        PSSensor gyroscope;
        PSSensor accelerometer;

        uint8_t reserved[5];
        uint8_t status[3];
        uint8_t tpad_count;
        TouchpadData touchpadData;
        uint8_t mystery[12]; // Relleno hasta 64 bytes
    };

    static_assert(sizeof(InReport) == 64, "PS4Dev::InReport debe medir 64 bytes");

    // --- STRINGS USB ---
    static const uint16_t STRING_LANGUAGE[]    = { (3 << 8) | 0x04, 0x0409 };
    static const char* STRING_MANUFACTURER     = "Sony Interactive Entertainment";
    static const char* STRING_PRODUCT          = "Wireless Controller";
    static const char* STRING_SERIAL           = "000000000001";
    static const char* STRING_VERSION          = "1.0";

    // --- DEVICE DESCRIPTOR ---
    static const uint8_t DEVICE_DESCRIPTORS[] = {
        0x12, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40,
        0x4C, 0x05, // Sony VID
        0xCC, 0x09, // DS4 V2 PID
        0x00, 0x01, 0x01, 0x02, 0x03, 0x01
    };

    // --- CALIBRATION DATA (HID GET_REPORT 0x02) ---
    // Warzone SIEMPRE pide esto. Si no respondes esto en tu main.cpp, no funcionará.
    static const uint8_t CALIBRATION_DATA_0x02[37] = {
        0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    // --- HID REPORT DESCRIPTOR (Oficial Sony) ---
    static const uint8_t REPORT_DESCRIPTORS[] = {
        0x05, 0x01, 0x09, 0x05, 0xa1, 0x01, 0x85, 0x01, 0x09, 0x30, 0x09, 0x31, 0x09, 0x32, 0x09, 0x35,
        0x15, 0x00, 0x26, 0xff, 0x00, 0x75, 0x08, 0x95, 0x04, 0x81, 0x02, 0x09, 0x39, 0x15, 0x00, 0x25,
        0x07, 0x35, 0x00, 0x46, 0x3b, 0x01, 0x65, 0x14, 0x75, 0x04, 0x95, 0x01, 0x81, 0x42, 0x65, 0x00,
        0x05, 0x09, 0x19, 0x01, 0x29, 0x0e, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x0e, 0x81, 0x02,
        0x06, 0x00, 0xff, 0x09, 0x20, 0x75, 0x06, 0x95, 0x01, 0x81, 0x02, 0x05, 0x01, 0x09, 0x33, 0x09,
        0x34, 0x15, 0x00, 0x26, 0xff, 0x00, 0x75, 0x08, 0x95, 0x02, 0x81, 0x02, 0x06, 0x00, 0xff, 0x09,
        0x21, 0x95, 0x36, 0x81, 0x02, 0x85, 0x05, 0x09, 0x22, 0x95, 0x1f, 0x91, 0x02, 0x85, 0x03, 0x0a,
        0x21, 0x27, 0x95, 0x2f, 0xb1, 0x02, 0xc0
    };

    // --- CONFIGURATION DESCRIPTORS ---
    static const uint8_t CONFIGURATION_DESCRIPTORS[] = {
        0x09, 0x02, 0x29, 0x00, 0x01, 0x01, 0x00, 0x80, 0xFA, // 500mA
        0x09, 0x04, 0x00, 0x00, 0x02, 0x03, 0x00, 0x00, 0x00,
        0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22, sizeof(REPORT_DESCRIPTORS) & 0xFF, (sizeof(REPORT_DESCRIPTORS) >> 8) & 0xFF,
        0x07, 0x05, 0x81, 0x03, 0x40, 0x00, 0x01, // EP IN 1ms
        0x07, 0x05, 0x02, 0x03, 0x40, 0x00, 0x01  // EP OUT 1ms
    };
}
#endif
