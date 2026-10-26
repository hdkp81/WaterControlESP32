#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <esp_system.h>

// ================= GLOBAL LIMITS =================
constexpr uint8_t MAX_SENSORS = 3; 

// ================= HARDWARE (SENSORS & ACTUATORS) =================
constexpr uint8_t ONE_WIRE_BUS = 4;
constexpr uint8_t TDS_PIN = 36;       // ADC1
constexpr uint8_t pH_SENSOR_PIN = 34; // ADC1

// CẢNH BÁO KCS: Chân 5 là Strapping Pin. Nếu dùng Relay Active-High, bơm sẽ giật nhẹ lúc boot.
constexpr uint8_t RELAY_PIN = 5;      
constexpr float DEFAULT_PH_THRESHOLD = 6.0f;

// ================= RELAY (SOURCE OF TRUTH) ====================
constexpr uint8_t RELAY_PIN_0 = 32;
constexpr uint8_t RELAY_PIN_1 = 33;
constexpr uint8_t RELAY_PIN_2 = 25;

constexpr uint8_t RELAY_PINS[] = {RELAY_PIN_0, RELAY_PIN_1, RELAY_PIN_2};
constexpr uint8_t MAX_HARDWARE_RELAYS = sizeof(RELAY_PINS) / sizeof(RELAY_PINS[0]);

// ================= LED ======================
constexpr uint8_t LED_TDS_ALERT_PIN    = 2;  // Lưu ý: Strapping Pin & LED Onboard
constexpr uint8_t LED_TEMP_ALERT_PIN   = 26;
constexpr uint8_t LED_TDS_NORMAL_PIN   = 27;
constexpr uint8_t LED_SYSTEM_STATUS_PIN = 14;

// ================= TIMING ===================
// Dùng uint32_t tường minh thay cho unsigned long
constexpr uint32_t DATA_UPDATE_INTERVAL = 5000;

// ================= TDS PHYSICAL ==============
constexpr float VOLTAGE_SCALE       = 3.3f / 4095.0f;
constexpr uint8_t TDS_SAMPLE_COUNT  = 10;
constexpr float TDS_TEMP_COMP_COEFF = 0.02f;
constexpr float TDS_REFERENCE_TEMP  = 25.0f;

// ================= TDS LOGIC =================
constexpr uint8_t NUM_TDS_THRESHOLDS = 3; 

// ================= THINGSPEAK =================
constexpr char THINGSPEAK_SERVER[] = "api.thingspeak.com";

// ================= CẤU HÌNH RIÊNG TƯ (KHÔNG COMMIT) =================
// Copy secrets.example.h -> secrets.h rồi chỉnh. File secrets.h đã nằm trong .gitignore.
#if defined(__has_include)
  #if __has_include("secrets.h")
    #include "secrets.h"
  #endif
#endif


 // MQTT: mặc định TẮT cho đến khi người dùng chủ động cấu hình
#ifndef MQTT_HOST
#define MQTT_HOST ""
#endif

#ifndef MQTT_PORT
#define MQTT_PORT 8883
#endif

#ifndef MQTT_USER
#define MQTT_USER ""
#endif

#ifndef MQTT_PASS
#define MQTT_PASS ""
#endif

#ifndef MQTT_REMOTE_CONTROL
#define MQTT_REMOTE_CONTROL 0
#endif

// OTA: mặc định TẮT (rỗng) cho đến khi đặt mật khẩu >= 8 ký tự trong secrets.h
#ifndef OTA_PASSWORD
#define OTA_PASSWORD ""
#endif

#ifndef AI_SERVER_URL
#define AI_SERVER_URL ""
#endif

#ifndef AI_USER_ID
#define AI_USER_ID "web_user"
#endif

// ================= MẠNG & BẢO MẬT WEB SERVER =================
constexpr const char* AP_SSID = "ESP32-HD7-Ver1.1";
// Mật khẩu AP cấu hình WiFi: tự sinh ngẫu nhiên ở lần chạy đầu (lưu NVS, hiện trên LCD + Serial).
// Có thể ép cố định bằng #define AP_PASSWORD "..." (>= 8 ký tự) trong secrets.h.

// Mã thiết bị dùng trong topic MQTT: mặc định lấy từ MAC (khó đoán), hoặc #define MQTT_DEVICE_ID trong secrets.h
inline String getDeviceId() {
#ifdef MQTT_DEVICE_ID
    return String(MQTT_DEVICE_ID);
#else
    char buf[24];
    snprintf(buf, sizeof(buf), "aqua_%012llx", (unsigned long long)ESP.getEfuseMac());
    return String(buf);
#endif
}

// Sinh chuỗi ngẫu nhiên (bỏ ký tự dễ nhầm) dùng cho mật khẩu khởi tạo
inline String randomToken(size_t n) {
    static const char alphabet[] = "ABCDEFGHJKMNPQRSTUVWXYZabcdefghjkmnpqrstuvwxyz23456789";
    String s; s.reserve(n);
    for (size_t i = 0; i < n; i++) s += alphabet[esp_random() % (sizeof(alphabet) - 1)];
    return s;
}

// ================= AN TOÀN BƠM =================
constexpr uint32_t PUMP_MAX_RUN_MS  = 30UL * 60UL * 1000UL; // Auto: tối đa 30 phút chạy liên tục
constexpr uint32_t PUMP_COOLDOWN_MS = 10UL * 60UL * 1000UL; // Nghỉ 10 phút sau khi chạm giới hạn

extern String admin_user;
extern String admin_pass;

// Cổng mạng (Port) tối đa là 65535, dùng uint16_t (2 bytes)
constexpr uint16_t MAIN_SERVER_PORT = 80;
