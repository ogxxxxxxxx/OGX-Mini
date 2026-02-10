#include "PS4.h"
#include <algorithm>
#include <cmath>
#include "pico/time.h"

// ============ ARRAYS DE CLAVES (PLACEHOLDERS) ============
// PEGA AQUÍ LOS BYTES DE TUS ARCHIVOS KEY.BIN / SERIAL.BIN
static const uint8_t KEY_N[256] = { /* 256 bytes de N */ 0x00 }; 
static const uint8_t KEY_E[256] = { /* bytes de E */ 0x01, 0x00, 0x01 }; // Usualmente 65537
static const uint8_t KEY_P[128] = { /* 128 bytes de P */ 0x00 };
static const uint8_t KEY_Q[128] = { /* 128 bytes de Q */ 0x00 };
// Serial del mando clonado (16 bytes)
static const uint8_t SERIAL_BYTES[16] = { 0x00 }; 
// Firma raíz (256 bytes)
static const uint8_t SIGNATURE_BYTES[256] = { 0x00 }; 

// ============ HERRAMIENTAS ============

static int my_rng(void* p_rng, unsigned char* p, size_t len) {
    (void)p_rng;
    for(size_t i=0; i<len; i++) p[i] = rand();
    return 0;
}

uint32_t calc_crc32(uint8_t const *buffer, size_t len) {
    uint32_t crc = 0xffffffff;
    for (size_t i = 0; i < len; i++) {
        uint8_t ch = buffer[i];
        for (size_t j = 0; j < 8; j++) {
            uint32_t b = (ch ^ crc) & 1;
            crc >>= 1;
            if (b) crc ^= 0xedb88320;
            ch >>= 1;
        }
    }
    return ~crc;
}

// ============ CLASE PS4DEVICE ============

void PS4Device::initialize()
{
    // Driver USB Base
    class_driver_ = {
        .name = TUD_DRV_NAME("PS4"),
        .init = hidd_init,
        .deinit = hidd_deinit,
        .reset = hidd_reset,
        .open = hidd_open,
        .control_xfer_cb = hidd_control_xfer_cb,
        .xfer_cb = hidd_xfer_cb,
        .sof = nullptr
    };

    report_counter_ = 0;
    auth_state_ = AUTH_IDLE;
    send_chunk_counter_ = 0;
    
    // Iniciar MbedTLS y cargar claves
    auth_init();
}

void PS4Device::auth_init() {
    mbedtls_rsa_init(&rsa_ctx_, MBEDTLS_RSA_PKCS_V21, MBEDTLS_MD_SHA256);
    mbedtls_entropy_init(&entropy_);
    mbedtls_ctr_drbg_init(&ctr_drbg_);

    // Seed Random
    mbedtls_ctr_drbg_seed(&ctr_drbg_, mbedtls_entropy_func, &entropy_, NULL, 0);

    // Cargar componentes RSA desde los arrays estáticos
    mbedtls_mpi_read_binary(&rsa_ctx_.N, KEY_N, sizeof(KEY_N));
    mbedtls_mpi_read_binary(&rsa_ctx_.E, KEY_E, sizeof(KEY_E));
    mbedtls_mpi_read_binary(&rsa_ctx_.P, KEY_P, sizeof(KEY_P));
    mbedtls_mpi_read_binary(&rsa_ctx_.Q, KEY_Q, sizeof(KEY_Q));

    // Completar RSA (calcula D, DP, DQ, QP)
    if (mbedtls_rsa_complete(&rsa_ctx_) == 0) {
        keys_loaded_ = true;
        printf("[PS4] Keys loaded OK\n");
    } else {
        keys_loaded_ = false;
        printf("[PS4] Error loading keys\n");
    }
}

void PS4Device::process(const uint8_t idx, Gamepad& gamepad)
{
    (void)idx;
    
    // Procesar autenticación si es necesario (Firma RSA)
    if (auth_state_ == AUTH_CALCULATING_SIGNATURE && keys_loaded_) {
        auth_process();
    }

    // === LÓGICA DE INPUT (JOYSTICKS Y BOTONES) ===
    // (Simplificada para brevedad, usa la lógica que tenías antes)
    Gamepad::PadIn gp_in = gamepad.get_pad_in();
    std::memset(&report_0x11_, 0, sizeof(report_0x11_));
    
    report_0x11_.reportID = 0x11;
    report_0x11_.data[0] = 0xC0; 
    
    // Sticks
    report_0x11_.data[2] = 128; // Center X
    report_0x11_.data[3] = 128; // Center Y
    report_0x11_.data[4] = 128; // Center RX
    report_0x11_.data[5] = 128; // Center RY
    
    // Mapeo básico de botones para que funcione (modifica con tu input real)
    uint8_t dpad = 0x08; // Center
    if (gp_in.dpad == Gamepad::DPAD_UP) dpad = 0x00;
    // ... mapear resto de dpad ...
    
    uint8_t shapes = 0;
    if (gp_in.buttons & Gamepad::BUTTON_A) shapes |= 0x20; // Cross
    if (gp_in.buttons & Gamepad::BUTTON_B) shapes |= 0x40; // Circle
    
    report_0x11_.data[6] = (shapes) | dpad; 
    
    // Timestamp y Sensores (necesario para Warzone)
    static uint16_t timestamp = 0;
    timestamp += 188;
    report_0x11_.data[11] = timestamp & 0xFF;
    report_0x11_.data[12] = (timestamp >> 8) & 0xFF;
    report_0x11_.data[19] = 0x20; // Gyro Z
    report_0x11_.data[25] = 0x40; // Accel Z (Gravedad)

    // CRC32 del Reporte Input
    uint8_t crc_buf[79];
    crc_buf[0] = 0xA1;
    std::memcpy(&crc_buf[1], &report_0x11_, 78);
    uint32_t crc = calc_crc32(crc_buf, 75);
    std::memcpy(&report_0x11_.data[73], &crc, 4);

    if (tud_hid_ready()) {
        tud_hid_report(0, &report_0x11_, sizeof(PS4Dev::InReport0x11));
    }
}

void PS4Device::auth_process() {
    // 1. Calcular Hash SHA256 del Nonce
    uint8_t hashed_nonce[32];
    if (mbedtls_sha256_ret(nonce_buffer_, 256, hashed_nonce, 0) != 0) return;

    // 2. Firmar Hash con RSA
    // El buffer de salida signature_buffer_ contendrá los 1064 bytes necesarios
    // Estructura respuesta: [Serial 16b] [Firma RSA 256b] [Signature Raiz 256b] [Padding]
    
    // Copiar Serial (Offset 0)
    // Nota: El protocolo de PS4 espera el Nonce ID primero si se usa el método viejo, 
    // pero GP2040 usa un buffer lineal. Vamos a construirlo como GP2040.
    
    // Limpiar buffer
    memset(signature_buffer_, 0, 1064);
    
    // A) Copiar Serial (16 bytes)
    memcpy(&signature_buffer_[0], SERIAL_BYTES, 16);
    
    // B) Generar Firma RSA (256 bytes) en offset 16
    // Usamos el buffer signature_buffer_ temporalmente para la firma
    int ret = mbedtls_rsa_rsassa_pss_sign(&rsa_ctx_, my_rng, NULL, MBEDTLS_MD_SHA256, 32, hashed_nonce, &signature_buffer_[16]);
    
    if (ret != 0) {
        printf("[PS4] RSA Sign failed: %d\n", ret);
        auth_state_ = AUTH_IDLE;
        return;
    }

    // C) Copiar Firma Raíz (256 bytes) en offset 16+256 = 272
    memcpy(&signature_buffer_[272], SIGNATURE_BYTES, 256);
    
    // Listo para enviar
    send_chunk_counter_ = 0;
    auth_state_ = AUTH_READY_TO_SEND;
}

uint16_t PS4Device::get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen)
{
    if (report_type == HID_REPORT_TYPE_FEATURE) {
        // 0xF2: Get Signing State (La consola pregunta: ¿Ya tienes la firma?)
        if (report_id == 0xF2) {
            buffer[0] = 0xF2;
            buffer[1] = nonce_id_;
            buffer[2] = (auth_state_ == AUTH_READY_TO_SEND) ? 0 : 16; // 0=Listo, 16=Ocupado
            memset(&buffer[3], 0, 9);
            
            // CRC32 del reporte F2
            uint32_t crc = calc_crc32(buffer, 12); // F2 + ID + Status + Padding
            memcpy(&buffer[12], &crc, 4);
            return 16;
        }
        
        // 0xF1: Get Signature Nonce (La consola pide la firma en trozos)
        if (report_id == 0xF1) {
            buffer[0] = 0xF1;
            buffer[1] = nonce_id_;
            buffer[2] = send_chunk_counter_;
            buffer[3] = 0;
            
            // Copiar 56 bytes del buffer de firma
            // Tamaño total: 1064 bytes / 56 bytes por paquete = 19 paquetes
            memcpy(&buffer[4], &signature_buffer_[send_chunk_counter_ * 56], 56);
            
            // CRC32
            uint32_t crc = calc_crc32(buffer, 60);
            memcpy(&buffer[60], &crc, 4);
            
            send_chunk_counter_++;
            if (send_chunk_counter_ >= 19) {
                auth_state_ = AUTH_IDLE; // Terminamos
                send_chunk_counter_ = 0;
            }
            return 64;
        }
    }
    return 0;
}

void PS4Device::set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize)
{
    if (report_type == HID_REPORT_TYPE_FEATURE && report_id == 0xF0) {
        // 0xF0: Set Auth Payload (La consola manda el Nonce en trozos)
        // Estructura: [F0] [ID] [Page] [0] [56 bytes data] [CRC32]
        
        uint8_t id = buffer[1];
        uint8_t page = buffer[2];
        
        // Verificar CRC
        uint32_t packet_crc = *((uint32_t*)&buffer[60]);
        if (calc_crc32(buffer, 60) != packet_crc) return; // CRC invalido

        if (page == 0) {
            nonce_id_ = id;
            auth_state_ = AUTH_WAITING_NONCE;
        }
        
        // Copiar datos al buffer de nonce
        // Nonce total es 256 bytes.
        // Pages 0-3 son 56 bytes cada una. Page 4 son 32 bytes + padding.
        if (page < 4) {
            memcpy(&nonce_buffer_[page * 56], &buffer[4], 56);
        } else if (page == 4) {
            memcpy(&nonce_buffer_[page * 56], &buffer[4], 32);
            // ¡NONCE COMPLETO! Calcular firma ahora
            auth_state_ = AUTH_CALCULATING_SIGNATURE;
        }
    }
}

// Stubs para otras funciones
bool PS4Device::vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request) { return false; }
const uint16_t* PS4Device::get_descriptor_string_cb(uint8_t index, uint16_t langid) { 
    if(index==0) { static uint16_t l[]={0x0304,0x0409}; return l;} 
    return (uint16_t*)PS4Dev::STRING_DESCRIPTORS[index]; 
}
const uint8_t* PS4Device::get_descriptor_device_cb() { return PS4Dev::DEVICE_DESCRIPTORS; }
const uint8_t* PS4Device::get_hid_descriptor_report_cb(uint8_t itf) { return PS4Dev::REPORT_DESCRIPTORS; }
const uint8_t* PS4Device::get_descriptor_configuration_cb(uint8_t index) { return PS4Dev::CONFIGURATION_DESCRIPTORS; }
const uint8_t* PS4Device::get_descriptor_device_qualifier_cb() { return nullptr; }
