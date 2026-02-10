#ifndef _PS4_DEVICE_DESCRIPTORS_H_
#define _PS4_DEVICE_DESCRIPTORS_H_

#include <stdint.h>

namespace PS4Dev
{
    // ============ CONSTANTES BÁSICAS ============
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
        HAT_CENTER    = 0x08, // Nota: 0x0F también es center, 0x08 es estándar
    };

    // ============ ESTRUCTURAS DE REPORTES ============
    
    // Report 0x01 (Estándar 64 bytes)
    struct __attribute__((packed)) InReport
    {
        uint8_t reportID;
        uint8_t leftStickX;
        uint8_t leftStickY;
        uint8_t rightStickX;
        uint8_t rightStickY;
        uint8_t dpad : 4;
        uint8_t buttonWest : 1;
        uint8_t buttonSouth : 1;
        uint8_t buttonEast : 1;
        uint8_t buttonNorth : 1;
        uint8_t buttonL1 : 1;
        uint8_t buttonR1 : 1;
        uint8_t buttonL2 : 1;
        uint8_t buttonR2 : 1;
        uint8_t buttonSelect : 1;
        uint8_t buttonStart : 1;
        uint8_t buttonL3 : 1;
        uint8_t buttonR3 : 1;
        uint8_t buttonHome : 1;
        uint8_t buttonTouchpad : 1;
        uint8_t reportCounter : 6;
        uint8_t leftTrigger;
        uint8_t rightTrigger;
        uint8_t miscData[54]; // Relleno para llegar a 64 bytes
    };

    // Report 0x11 (Extendido 78 bytes - Warzone)
    struct __attribute__((packed)) InReport0x11
    {
        uint8_t reportID;     // 0x11
        uint8_t data[77];     // Datos crudos (ejes, botones, CRC32)
    };

    // ============ DESCRIPTORES USB ============

    static const uint8_t STRING_LANGUAGE[]     = { 0x09, 0x04 };
    static const uint8_t STRING_MANUFACTURER[] = "Sony Computer Entertainment";
    static const uint8_t STRING_PRODUCT[]      = "Wireless Controller";
    static const uint8_t STRING_SERIAL[]       = "123456789012";
    static const uint8_t STRING_VERSION[]      = "1.0";

    static const uint8_t* const STRING_DESCRIPTORS[] = {
        STRING_LANGUAGE,
        STRING_MANUFACTURER,
        STRING_PRODUCT,
        STRING_SERIAL,
        STRING_VERSION
    };

    static const uint8_t DEVICE_DESCRIPTORS[] =
    {
        0x12,       // bLength
        0x01,       // bDescriptorType (Device)
        0x00, 0x02, // bcdUSB 2.00
        0x00,       // bDeviceClass
        0x00,       // bDeviceSubClass
        0x00,       // bDeviceProtocol
        0x40,       // bMaxPacketSize0 (64 bytes)
        
        0x4C, 0x05, // idVendor  0x054C (Sony)
        0xC4, 0x05, // idProduct 0x05C4 (DualShock 4 Gen 1 - MEJOR PARA PC)
        0x00, 0x01, // bcdDevice 1.00

        0x01,       // iManufacturer
        0x02,       // iProduct
        0x03,       // iSerialNumber
        0x01        // bNumConfigurations
    };

    // Descriptor HID CRÍTICO con Auth Reports (0xF0-0xF3)
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
        0x81, 0x02,        //   Input (Data,Var,Abs)

        0x09, 0x39,        //   Usage (Hat switch)
        0x15, 0x00,        //   Logical Minimum (0)
        0x25, 0x07,        //   Logical Maximum (7)
        0x35, 0x00,        //   Physical Minimum (0)
        0x46, 0x3B, 0x01,  //   Physical Maximum (315)
        0x65, 0x14,        //   Unit (System: English Rotation)
        0x75, 0x04,        //   Report Size (4)
        0x95, 0x01,        //   Report Count (1)
        0x81, 0x42,        //   Input (Data,Var,Abs,Null State)

        0x65, 0x00,        //   Unit (None)
        0x05, 0x09,        //   Usage Page (Button)
        0x19, 0x01,        //   Usage Minimum (0x01)
        0x29, 0x0E,        //   Usage Maximum (0x0E)
        0x15, 0x00,        //   Logical Minimum (0)
        0x25, 0x01,        //   Logical Maximum (1)
        0x75, 0x01,        //   Report Size (1)
        0x95, 0x0E,        //   Report Count (14)
        0x81, 0x02,        //   Input (Data,Var,Abs)

        0x06, 0x00, 0xFF,  //   Usage Page (Vendor Defined 0xFF00)
        0x09, 0x20,        //   Usage (0x20)
        0x75, 0x06,        //   Report Size (6)
        0x95, 0x01,        //   Report Count (1)
        0x81, 0x02,        //   Input (Data,Var,Abs)

        0x05, 0x01,        //   Usage Page (Generic Desktop Ctrls)
        0x09, 0x33,        //   Usage (Rx)
        0x09, 0x34,        //   Usage (Ry)
        0x15, 0x00,        //   Logical Minimum (0)
        0x26, 0xFF, 0x00,  //   Logical Maximum (255)
        0x75, 0x08,        //   Report Size (8)
        0x95, 0x02,        //   Report Count (2)
        0x81, 0x02,        //   Input (Data,Var,Abs)

        0x06, 0x00, 0xFF,  //   Usage Page (Vendor Defined 0xFF00)
        0x09, 0x21,        //   Usage (0x21)
        0x95, 0x36,        //   Report Count (54)
        0x81, 0x02,        //   Input (Data,Var,Abs)

        // Report 0x11 (Importante para Warzone)
        0x85, 0x11,        //   Report ID (17)
        0x09, 0x21,        //   Usage (0x21)
        0x95, 0x4D,        //   Report Count (77)
        0x81, 0x02,        //   Input (Data,Var,Abs)

        // --- AUTH FEATURE REPORTS (CRÍTICO PARA OGX MINI) ---
        0x06, 0xF0, 0xFF,  // Usage Page (Vendor Defined 0xFFF0)
        0x09, 0x40,        // Usage (0x40)
        0xA1, 0x01,        // Collection (Application)
        
        0x85, 0xF0,        // Report ID (240) - Set Auth Payload
        0x09, 0x47,        // Usage (0x47)
        0x95, 0x3F,        // Report Count (63)
        0xB1, 0x02,        // Feature (Data,Var,Abs)

        0x85, 0xF1,        // Report ID (241) - Get Signature Nonce
        0x09, 0x48,        // Usage (0x48)
        0x95, 0x3F,        // Report Count (63)
        0xB1, 0x02,        // Feature (Data,Var,Abs)

        0x85, 0xF2,        // Report ID (242) - Get Signing State
        0x09, 0x49,        // Usage (0x49)
        0x95, 0x0F,        // Report Count (15)
        0xB1, 0x02,        // Feature (Data,Var,Abs)

        0x85, 0xF3,        // Report ID (243) - Reset Auth
        0x0A, 0x01, 0x47,  // Usage (0x4701)
        0x95, 0x07,        // Report Count (7)
        0xB1, 0x02,        // Feature (Data,Var,Abs)
        
        0xC0,              // End Collection (Feature)
        // ----------------------------------------------------

        0xC0               // End Collection (Application)
    };

    static const uint8_t CONFIGURATION_DESCRIPTORS[] =
    {
        0x09, 0x02, 0x29, 0x00, 0x01, 0x01, 0x00, 0x80, 0xFA, // Configuration
        0x09, 0x04, 0x00, 0x00, 0x02, 0x03, 0x00, 0x00, 0x00, // Interface
        0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22, sizeof(REPORT_DESCRIPTORS), 0x00, // HID
        0x07, 0x05, 0x81, 0x03, 0x40, 0x00, 0x01, // Endpoint IN
        0x07, 0x05, 0x02, 0x03, 0x40, 0x00, 0x01  // Endpoint OUT
    };
}

#endif // _PS4_DEVICE_DESCRIPTORS_H_
