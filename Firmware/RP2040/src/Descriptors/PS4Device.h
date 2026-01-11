#ifndef _PS4_DEVICE_DESCRIPTORS_H_
#define _PS4_DEVICE_DESCRIPTORS_H_

#include <stdint.h>

namespace PS4Dev
{
    // Tamaño del endpoint y reporte
    #define PS4_ENDPOINT_SIZE 64
    #define ENDPOINT0_SIZE    64
    #define GAMEPAD_INTERFACE 0
    #define GAMEPAD_ENDPOINT  1
    #define GAMEPAD_SIZE      64

    #define LSB(n) ((n) & 0xFF)
    #define MSB(n) (((n) >> 8) & 0xFF)

    // --- HAT (d-pad) valores DS4 (4 bits) ---
    #define PS4_HAT_UP        0x00
    #define PS4_HAT_UPRIGHT   0x01
    #define PS4_HAT_RIGHT     0x02
    #define PS4_HAT_DOWNRIGHT 0x03
    #define PS4_HAT_DOWN      0x04
    #define PS4_HAT_DOWNLEFT  0x05
    #define PS4_HAT_LEFT      0x06
    #define PS4_HAT_UPLEFT    0x07
    #define PS4_HAT_NOTHING   0x0F   // "centrado"

    // --- Botones en máscara de 16 bits (estilo GP2040) ---
    #define PS4_MASK_SQUARE   (1U <<  0)
    #define PS4_MASK_CROSS    (1U <<  1)
    #define PS4_MASK_CIRCLE   (1U <<  2)
    #define PS4_MASK_TRIANGLE (1U <<  3)
    #define PS4_MASK_L1       (1U <<  4)
    #define PS4_MASK_R1       (1U <<  5)
    #define PS4_MASK_L2       (1U <<  6)
    #define PS4_MASK_R2       (1U <<  7)
    #define PS4_MASK_SELECT   (1U <<  8)   // Share
    #define PS4_MASK_START    (1U <<  9)   // Options
    #define PS4_MASK_L3       (1U << 10)
    #define PS4_MASK_R3       (1U << 11)
    #define PS4_MASK_PS       (1U << 12)
    #define PS4_MASK_TP       (1U << 13)

    // Sticks 0–255
    #define PS4_JOYSTICK_MIN 0x00
    #define PS4_JOYSTICK_MID 0x80
    #define PS4_JOYSTICK_MAX 0xFF

    // Valor central para sticks
    static constexpr uint8_t JOYSTICK_MID = PS4_JOYSTICK_MID;

    // -----------------------------
    // Namespaces de conveniencia
    // -----------------------------

    namespace Hat
    {
        static constexpr uint8_t UP         = PS4_HAT_UP;
        static constexpr uint8_t UP_RIGHT   = PS4_HAT_UPRIGHT;
        static constexpr uint8_t RIGHT      = PS4_HAT_RIGHT;
        static constexpr uint8_t DOWN_RIGHT = PS4_HAT_DOWNRIGHT;
        static constexpr uint8_t DOWN       = PS4_HAT_DOWN;
        static constexpr uint8_t DOWN_LEFT  = PS4_HAT_DOWNLEFT;
        static constexpr uint8_t LEFT       = PS4_HAT_LEFT;
        static constexpr uint8_t UP_LEFT    = PS4_HAT_UPLEFT;
        static constexpr uint8_t CENTER     = PS4_HAT_NOTHING;
    }

    namespace Buttons
    {
        static constexpr uint16_t SQUARE    = PS4_MASK_SQUARE;   // B0
        static constexpr uint16_t CROSS     = PS4_MASK_CROSS;    // B1
        static constexpr uint16_t CIRCLE    = PS4_MASK_CIRCLE;   // B2
        static constexpr uint16_t TRIANGLE  = PS4_MASK_TRIANGLE; // B3

        static constexpr uint16_t L1        = PS4_MASK_L1;       // B4
        static constexpr uint16_t R1        = PS4_MASK_R1;       // B5
        static constexpr uint16_t L2        = PS4_MASK_L2;       // B6 (digital)
        static constexpr uint16_t R2        = PS4_MASK_R2;       // B7 (digital)

        // Alias para mantener compatibilidad con código anterior
        static constexpr uint16_t L2_DIG    = PS4_MASK_L2;
        static constexpr uint16_t R2_DIG    = PS4_MASK_R2;

        static constexpr uint16_t SHARE     = PS4_MASK_SELECT;   // B8
        static constexpr uint16_t OPTIONS   = PS4_MASK_START;    // B9
        static constexpr uint16_t L3        = PS4_MASK_L3;       // B10
        static constexpr uint16_t R3        = PS4_MASK_R3;       // B11
        static constexpr uint16_t PS        = PS4_MASK_PS;       // B12
        static constexpr uint16_t TOUCHPAD  = PS4_MASK_TP;       // B13
    }

    // ---------------------------------
    // Estructuras DS4 (copiadas de GP2040-CE)
    // ---------------------------------

    struct __attribute__((packed)) TouchpadXY {
        uint8_t counter   : 7;
        uint8_t unpressed : 1;
        // 12-bit X seguido de 12-bit Y
        uint8_t data[3];

        void set_x(uint16_t x) {
            data[0] = x & 0xFF;
            data[1] = (data[1] & 0xF0) | ((x >> 8) & 0x0F);
        }

        void set_y(uint16_t y) {
            data[1] = (data[1] & 0x0F) | ((y & 0x0F) << 4);
            data[2] = y >> 4;
        }
    };

    struct __attribute__((packed)) TouchpadData {
        TouchpadXY p1;
        TouchpadXY p2;
    };

    struct __attribute__((packed)) PSSensor {
        int16_t x;
        int16_t y;
        int16_t z;
    };

    struct __attribute__((packed)) PSSensorData {
        uint16_t battery;
        PSSensor gyroscope;
        PSSensor accelerometer;
        uint8_t  misc[4];

        uint8_t  powerLevel : 4;
        uint8_t  charging   : 1;
        uint8_t  headphones : 1;
        uint8_t  microphone : 1;
        uint8_t  extension  : 1;

        uint8_t  extData0   : 1;
        uint8_t  extData1   : 1;
        uint8_t  notConnected : 1;
        uint8_t  extData3   : 5;

        uint8_t  misc2;
    };

    // Reporte principal de entrada del DS4 (64 bytes)
    struct __attribute__((packed)) PS4Report
    {
        // 0
        uint8_t reportID;      // Siempre 1 para entrada estándar

        // 1-4: sticks
        uint8_t leftStickX;
        uint8_t leftStickY;
        uint8_t rightStickX;
        uint8_t rightStickY;

        // 5: 4 bits d-pad + 4 bits de los 4 botones frontales (según descriptor)
        uint8_t dpad : 4;

        // 14 bits para los botones
        uint16_t buttonWest     : 1; // Cuadrado
        uint16_t buttonSouth    : 1; // Cruz
        uint16_t buttonEast     : 1; // Círculo
        uint16_t buttonNorth    : 1; // Triángulo
        uint16_t buttonL1       : 1;
        uint16_t buttonR1       : 1;
        uint16_t buttonL2       : 1;
        uint16_t buttonR2       : 1;
        uint16_t buttonSelect   : 1; // Share
        uint16_t buttonStart    : 1; // Options
        uint16_t buttonL3       : 1;
        uint16_t buttonR3       : 1;
        uint16_t buttonHome     : 1; // PS
        uint16_t buttonTouchpad : 1; // Touchpad click

        // 6 bits de contador de reporte (no nos preocupa mucho)
        uint8_t reportCounter : 6;
        uint8_t : 2;

        // Triggers analógicos
        uint8_t leftTrigger;
        uint8_t rightTrigger;

        // Vendor / sensores / touchpad / etc. (54 bytes)
        union {
            uint8_t miscData[54];

            struct __attribute__((packed)) {
                uint16_t     axisTiming;
                PSSensorData sensorData;

                uint8_t touchpadActive : 2;
                uint8_t padding        : 6;
                uint8_t tpadIncrement;

                TouchpadData touchpadData;

                uint8_t mystery2[21];
            } gamepad;
        };
    };

    using InReport = PS4Report;

    static_assert(sizeof(InReport) == 64, "PS4Dev::InReport debe medir 64 bytes");

    // -----------------------------
    // Helpers para mantener compatibilidad con PS4.cpp
    // -----------------------------

    // Alias de nombres que usa el driver actual
    // (se reemplazan en preprocesado: report_in_.joystick_lx → report_in_.leftStickX)
    #define joystick_lx leftStickX
    #define joystick_ly leftStickY
    #define joystick_rx rightStickX
    #define joystick_ry rightStickY
    #define trigger_l   leftTrigger
    #define trigger_r   rightTrigger
    #define hat         dpad  // si en algún lado se usa report_in_.hat

    inline void setHat(InReport& report, uint8_t hatValue)
    {
        report.dpad = hatValue;
    }

    inline void setButtons(InReport& report, uint16_t btnMask)
    {
        report.buttonWest     = (btnMask & Buttons::SQUARE)   ? 1 : 0;
        report.buttonSouth    = (btnMask & Buttons::CROSS)    ? 1 : 0;
        report.buttonEast     = (btnMask & Buttons::CIRCLE)   ? 1 : 0;
        report.buttonNorth    = (btnMask & Buttons::TRIANGLE) ? 1 : 0;
        report.buttonL1       = (btnMask & Buttons::L1)       ? 1 : 0;
        report.buttonR1       = (btnMask & Buttons::R1)       ? 1 : 0;
        report.buttonL2       = (btnMask & Buttons::L2)       ? 1 : 0;
        report.buttonR2       = (btnMask & Buttons::R2)       ? 1 : 0;
        report.buttonSelect   = (btnMask & Buttons::SHARE)    ? 1 : 0;
        report.buttonStart    = (btnMask & Buttons::OPTIONS)  ? 1 : 0;
        report.buttonL3       = (btnMask & Buttons::L3)       ? 1 : 0;
        report.buttonR3       = (btnMask & Buttons::R3)       ? 1 : 0;
        report.buttonHome     = (btnMask & Buttons::PS)       ? 1 : 0;
        report.buttonTouchpad = (btnMask & Buttons::TOUCHPAD) ? 1 : 0;
    }

    // ---------------------------------
    // Cadenas USB (las podemos “disfrazar” como DS4)
    // ---------------------------------

    static const uint8_t STRING_LANGUAGE[]     = { 0x09, 0x04 };
    static const uint8_t STRING_MANUFACTURER[] = "Sony Interactive Entertainment";
    static const uint8_t STRING_PRODUCT[]      = "Wireless Controller";
    static const uint8_t STRING_VERSION[]      = "1.0";

    static const uint8_t* STRING_DESCRIPTORS[] __attribute__((unused)) =
    {
        STRING_LANGUAGE,
        STRING_MANUFACTURER,
        STRING_PRODUCT,
        STRING_VERSION
    };

    // ---------------------------------
    // Descriptor de dispositivo: DS4 v2 (054C:09CC)
    // ---------------------------------
    static const uint8_t DEVICE_DESCRIPTORS[] =
    {
        0x12,        // bLength
        0x01,        // bDescriptorType (Device)
        0x00, 0x02,  // bcdUSB 2.00
        0x00,        // bDeviceClass
        0x00,        // bDeviceSubClass
        0x00,        // bDeviceProtocol
        0x40,        // bMaxPacketSize0 64
        0x4C, 0x05,  // idVendor 0x054C (Sony)
        0xCC, 0x09,  // idProduct 0x09CC (DualShock 4 v2)
        0x00, 0x01,  // bcdDevice 1.00
        0x01,        // iManufacturer
        0x02,        // iProduct
        0x03,        // iSerialNumber
        0x01,        // bNumConfigurations
    };

    // ---------------------------------
    // REPORT_DESCRIPTORS: copiado del ps4_report_descriptor de GP2040-CE
    // (report ID 1, sticks, d-pad, botones, triggers, vendor, features, auth F0/F1/F2...)
    // ---------------------------------
    static const uint8_t REPORT_DESCRIPTORS[] =
    {
        0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
        0x09, 0x05,        // Usage (Game Pad)
        0xA1, 0x01,        // Collection (Application)
        0x85, 0x01,        //   Report ID (1)
        0x09, 0x30,        //   Usage (X)
        0x09, 0x31,        //   Usage (Y)
        0x09, 0x32,        //   Usage (Z)
        0x09, 0x35,        //   Usage (Rz)
        0x15, 0x00,        //   Logical Minimum (0)
        0x26, 0xFF, 0x00,  //   Logical Maximum (255)
        0x75, 0x08,        //   Report Size (8)
        0x95, 0x04,        //   Report Count (4)
        0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)

        0x09, 0x39,        //   Usage (Hat switch)
        0x15, 0x00,        //   Logical Minimum (0)
        0x25, 0x07,        //   Logical Maximum (7)
        0x35, 0x00,        //   Physical Minimum (0)
        0x46, 0x3B, 0x01,  //   Physical Maximum (315)
        0x65, 0x14,        //   Unit (System: English Rotation, Length: Centimeter)
        0x75, 0x04,        //   Report Size (4)
        0x95, 0x01,        //   Report Count (1)
        0x81, 0x42,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,Null State)

        0x65, 0x00,        //   Unit (None)
        0x05, 0x09,        //   Usage Page (Button)
        0x19, 0x01,        //   Usage Minimum (0x01)
        0x29, 0x0E,        //   Usage Maximum (0x0E)
        0x15, 0x00,        //   Logical Minimum (0)
        0x25, 0x01,        //   Logical Maximum (1)
        0x75, 0x01,        //   Report Size (1)
        0x95, 0x0E,        //   Report Count (14)
        0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)

        0x06, 0x00, 0xFF,  //   Usage Page (Vendor Defined 0xFF00)
        0x09, 0x20,        //   Usage (0x20)
        0x75, 0x06,        //   Report Size (6)
        0x95, 0x01,        //   Report Count (1)
        0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)

        0x05, 0x01,        //   Usage Page (Generic Desktop Ctrls)
        0x09, 0x33,        //   Usage (Rx)
        0x09, 0x34,        //   Usage (Ry)
        0x15, 0x00,        //   Logical Minimum (0)
        0x26, 0xFF, 0x00,  //   Logical Maximum (255)
        0x75, 0x08,        //   Report Size (8)
        0x95, 0x02,        //   Report Count (2)
        0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)

        0x06, 0x00, 0xFF,  //   Usage Page (Vendor Defined 0xFF00)
        0x09, 0x21,        //   Usage (0x21)
        0x95, 0x36,        //   Report Count (54)
        0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)

        // A partir de aquí son Feature/Output reports (rumble, auth, etc.)
        0x85, 0x05,        //   Report ID (5)
        0x09, 0x22,        //   Usage (0x22)
        0x95, 0x1F,        //   Report Count (31)
        0x91, 0x02,        //   Output (...)

        0x85, 0x03,        //   Report ID (3)
        0x0A, 0x21, 0x27,
        0x95, 0x2F,
        0xB1, 0x02,

        0x85, 0x02,
        0x09, 0x24,
        0x95, 0x24,
        0xB1, 0x02,

        0x85, 0x08,
        0x09, 0x25,
        0x95, 0x03,
        0xB1, 0x02,

        0x85, 0x10,
        0x09, 0x26,
        0x95, 0x04,
        0xB1, 0x02,

        0x85, 0x11,
        0x09, 0x27,
        0x95, 0x02,
        0xB1, 0x02,

        0x85, 0x12,
        0x06, 0x02, 0xFF,
        0x09, 0x21,
        0x95, 0x0F,
        0xB1, 0x02,

        0x85, 0x13,
        0x09, 0x22,
        0x95, 0x16,
        0xB1, 0x02,

        0x85, 0x14,
        0x06, 0x05, 0xFF,
        0x09, 0x20,
        0x95, 0x10,
        0xB1, 0x02,

        0x85, 0x15,
        0x09, 0x21,
        0x95, 0x2C,
        0xB1, 0x02,

        0x06, 0x80, 0xFF,
        0x85, 0x80,
        0x09, 0x20,
        0x95, 0x06,
        0xB1, 0x02,

        0x85, 0x81,
        0x09, 0x21,
        0x95, 0x06,
        0xB1, 0x02,

        0x85, 0x82,
        0x09, 0x22,
        0x95, 0x05,
        0xB1, 0x02,

        0x85, 0x83,
        0x09, 0x23,
        0x95, 0x01,
        0xB1, 0x02,

        0x85, 0x84,
        0x09, 0x24,
        0x95, 0x04,
        0xB1, 0x02,

        0x85, 0x85,
        0x09, 0x25,
        0x95, 0x06,
        0xB1, 0x02,

        0x85, 0x86,
        0x09, 0x26,
        0x95, 0x06,
        0xB1, 0x02,

        0x85, 0x87,
        0x09, 0x27,
        0x95, 0x23,
        0xB1, 0x02,

        0x85, 0x88,
        0x09, 0x28,
        0x95, 0x22,
        0xB1, 0x02,

        0x85, 0x89,
        0x09, 0x29,
        0x95, 0x02,
        0xB1, 0x02,

        0x85, 0x90,
        0x09, 0x30,
        0x95, 0x05,
        0xB1, 0x02,

        0x85, 0x91,
        0x09, 0x31,
        0x95, 0x03,
        0xB1, 0x02,

        0x85, 0x92,
        0x09, 0x32,
        0x95, 0x03,
        0xB1, 0x02,

        0x85, 0x93,
        0x09, 0x33,
        0x95, 0x0C,
        0xB1, 0x02,

        0x85, 0xA0,
        0x09, 0x40,
        0x95, 0x06,
        0xB1, 0x02,

        0x85, 0xA1,
        0x09, 0x41,
        0x95, 0x01,
        0xB1, 0x02,

        0x85, 0xA2,
        0x09, 0x42,
        0x95, 0x01,
        0xB1, 0x02,

        0x85, 0xA3,
        0x09, 0x43,
        0x95, 0x30,
        0xB1, 0x02,

        0x85, 0xA4,
        0x09, 0x44,
        0x95, 0x0D,
        0xB1, 0x02,

        0x85, 0xA5,
        0x09, 0x45,
        0x95, 0x15,
        0xB1, 0x02,

        0x85, 0xA6,
        0x09, 0x46,
        0x95, 0x15,
        0xB1, 0x02,

        0x85, 0xA7,
        0x09, 0x4A,
        0x95, 0x01,
        0xB1, 0x02,

        0x85, 0xA8,
        0x09, 0x4B,
        0x95, 0x01,
        0xB1, 0x02,

        0x85, 0xA9,
        0x09, 0x4C,
        0x95, 0x08,
        0xB1, 0x02,

        0x85, 0xAA,
        0x09, 0x4E,
        0x95, 0x01,
        0xB1, 0x02,

        0x85, 0xAB,
        0x09, 0x4F,
        0x95, 0x39,
        0xB1, 0x02,

        0x85, 0xAC,
        0x09, 0x50,
        0x95, 0x39,
        0xB1, 0x02,

        0x85, 0xAD,
        0x09, 0x51,
        0x95, 0x0B,
        0xB1, 0x02,

        0x85, 0xAE,
        0x09, 0x52,
        0x95, 0x01,
        0xB1, 0x02,

        0x85, 0xAF,
        0x09, 0x53,
        0x95, 0x02,
        0xB1, 0x02,

        0x85, 0xB0,
        0x09, 0x54,
        0x95, 0x3F,
        0xB1, 0x02,

        0xC0,              // End Collection (Gamepad)

        // Colección extra de AUTH (F0/F1/F2/F3), no la usamos en PC pero la dejamos
        0x06, 0xF0, 0xFF,  // Usage Page (Vendor Defined 0xFFF0)
        0x09, 0x40,        // Usage (0x40)
        0xA1, 0x01,        // Collection (Application)

        0x85, 0xF0,        //   Report ID (F0) AUTH
        0x09, 0x47,
        0x95, 0x3F,
        0xB1, 0x02,

        0x85, 0xF1,        //   Report ID (F1)
        0x09, 0x48,
        0x95, 0x3F,
        0xB1, 0x02,

        0x85, 0xF2,        //   Report ID (F2)
        0x09, 0x49,
        0x95, 0x0F,
        0xB1, 0x02,

        0x85, 0xF3,        //   Report ID (F3)
        0x0A, 0x01, 0x47,
        0x95, 0x07,
        0xB1, 0x02,

        0xC0               // End Collection (Auth)
    };

    // ---------------------------------
    // Descriptor de configuración
    // (mismo esquema que antes, solo actualiza wDescriptorLength)
    // ---------------------------------
    #define PS4_CONFIG1_DESC_SIZE (9+9+9+7+7)

    static const uint8_t CONFIGURATION_DESCRIPTORS[] =
    {
        // Configuration descriptor
        0x09,                       // bLength
        0x02,                       // bDescriptorType (Configuration)
        LSB(PS4_CONFIG1_DESC_SIZE), // wTotalLength
        MSB(PS4_CONFIG1_DESC_SIZE),
        0x01,                       // bNumInterfaces
        0x01,                       // bConfigurationValue
        0x00,                       // iConfiguration
        0x80,                       // bmAttributes (Bus powered)
        50,                         // bMaxPower (100mA)

        // Interface descriptor
        0x09,                       // bLength
        0x04,                       // bDescriptorType (Interface)
        GAMEPAD_INTERFACE,          // bInterfaceNumber
        0x00,                       // bAlternateSetting
        0x02,                       // bNumEndpoints
        0x03,                       // bInterfaceClass (HID)
        0x00,                       // bInterfaceSubClass
        0x00,                       // bInterfaceProtocol
        0x00,                       // iInterface

        // HID descriptor
        0x09,                       // bLength
        0x21,                       // bDescriptorType (HID)
        0x11, 0x01,                 // bcdHID 1.11
        0x00,                       // bCountryCode
        0x01,                       // bNumDescriptors
        0x22,                       // bDescriptorType (Report)
        LSB(sizeof(REPORT_DESCRIPTORS)), // wDescriptorLength
        MSB(sizeof(REPORT_DESCRIPTORS)),

        // Endpoint IN (device -> host)
        0x07,                       // bLength
        0x05,                       // bDescriptorType (Endpoint)
        (GAMEPAD_ENDPOINT | 0x80),  // bEndpointAddress (EP1 IN)
        0x03,                       // bmAttributes (Interrupt)
        GAMEPAD_SIZE, 0x00,         // wMaxPacketSize 64
        0x01,                       // bInterval 1ms

        // Endpoint OUT (host -> device)
        0x07,                       // bLength
        0x05,                       // bDescriptorType (Endpoint)
        0x02,                       // bEndpointAddress (EP2 OUT)
        0x03,                       // bmAttributes (Interrupt)
        0x40, 0x00,                 // wMaxPacketSize 64
        0x01,                       // bInterval 1ms
    };

} // namespace PS4Dev

#endif // _PS4_DEVICE_DESCRIPTORS_H_
