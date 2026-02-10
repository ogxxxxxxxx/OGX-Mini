#ifndef _PS4_DEVICE_H_
#define _PS4_DEVICE_H_

#include <cstdint>
#include <cstring>
#include "USBDevice/DeviceDriver/DeviceDriver.h"
#include "PS4Device.h" // Asegúrate de que apunte al archivo 1

// MbedTLS (NECESARIO PARA FIRMAR)
#include "mbedtls/rsa.h"
#include "mbedtls/sha256.h"
#include "mbedtls/pk.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"

class PS4Device : public DeviceDriver
{
public:
    void initialize() override;
    void process(const uint8_t idx, Gamepad& gamepad) override;

    uint16_t get_report_cb(uint8_t itf, uint8_t report_id,
                           hid_report_type_t report_type,
                           uint8_t *buffer, uint16_t reqlen) override;

    void set_report_cb(uint8_t itf, uint8_t report_id,
                       hid_report_type_t report_type,
                       uint8_t const *buffer, uint16_t bufsize) override;

    bool vendor_control_xfer_cb(uint8_t rhport, uint8_t stage,
                                tusb_control_request_t const *request) override;

    const uint16_t* get_descriptor_string_cb(uint8_t index, uint16_t langid) override;
    const uint8_t* get_descriptor_device_cb() override;
    const uint8_t* get_hid_descriptor_report_cb(uint8_t itf) override;
    const uint8_t* get_descriptor_configuration_cb(uint8_t index) override;
    const uint8_t* get_descriptor_device_qualifier_cb() override;

private:
    // Reportes
    PS4Dev::InReport report_in_;
    PS4Dev::InReport0x11 report_0x11_;
    uint8_t report_counter_;

    // === AUTENTICACIÓN PS4 (Host-less) ===
    void auth_init();       // Cargar llaves RSA
    void auth_process();    // Firmar nonce

    // Contexto MbedTLS
    mbedtls_rsa_context rsa_ctx_;
    mbedtls_entropy_context entropy_;
    mbedtls_ctr_drbg_context ctr_drbg_;
    bool keys_loaded_;

    // Variables de Estado Auth
    uint8_t nonce_id_;
    uint8_t nonce_buffer_[256];     // Buffer para recibir el desafío (Nonce)
    uint8_t signature_buffer_[1024]; // Buffer para guardar la firma (Signature)
    
    // Estado
    enum AuthState {
        AUTH_IDLE,
        AUTH_WAITING_NONCE,
        AUTH_CALCULATING_SIGNATURE,
        AUTH_READY_TO_SEND
    };
    volatile AuthState auth_state_;
    uint8_t send_chunk_counter_; // Para enviar la firma en pedazos
};

#endif // _PS4_DEVICE_H_
