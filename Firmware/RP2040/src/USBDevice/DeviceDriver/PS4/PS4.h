#ifndef _PS4_DEFINITIONS_H_
#define _PS4_DEFINITIONS_H_

#include <stdint.h>

// Definiciones de Report IDs
#define PS4_REPORT_ID_INPUT         0x01
#define PS4_REPORT_ID_INPUT_EXT     0x11 // Importante para Warzone
#define PS4_REPORT_ID_CALIBRATION   0x02
#define PS4_REPORT_ID_DEFINITION    0x03
#define PS4_REPORT_ID_MAC_ADDRESS   0x12
#define PS4_REPORT_ID_VERSION       0xA3

// Auth Feature Reports (Seguridad)
#define PS4_AUTH_REPORT_ID_SET_PAYLOAD      0xF0 // Consola manda nonce
#define PS4_AUTH_REPORT_ID_GET_NONCE        0xF1 // Consola pide firma
#define PS4_AUTH_REPORT_ID_GET_SIGNING      0xF2 // Estado de firma
#define PS4_AUTH_REPORT_ID_RESET            0xF3 // Reset

// Estructura del Reporte 0x11 (78 bytes)
typedef struct __attribute__((packed)) {
    uint8_t report_id;      // 0x11
    uint8_t data[73];       // Datos de sticks, botones, sensores
    uint32_t crc32;         // CRC32 final
} ps4_input_report_0x11_t;

// Estructura del Reporte 0x01 (64 bytes)
typedef struct __attribute__((packed)) {
    uint8_t report_id;
    uint8_t left_stick_x;
    uint8_t left_stick_y;
    uint8_t right_stick_x;
    uint8_t right_stick_y;
    uint8_t dpad_buttons;   // 4 bits dpad, 4 bits shapes
    uint8_t misc_buttons;   // Triggers, Shoulders, Share, Option, L3, R3
    uint8_t sys_buttons;    // PS, Touchpad, Counter
    uint8_t left_trigger;
    uint8_t right_trigger;
    uint8_t timestamp[2];
    uint8_t battery;
    int16_t gyro[3];
    int16_t accel[3];
    uint8_t ext[35];
} ps4_input_report_0x01_t;

#endif // _PS4_DEFINITIONS_H_
