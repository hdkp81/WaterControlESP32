#ifndef OTA_UPDATE_H
#define OTA_UPDATE_H

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>

/**
 * @file OTAUpdate.h
 * @brief Cập nhật OTA (Over-The-Air) - Header-only.
 *
 * AN TOÀN: OTA chỉ được bật khi có mật khẩu >= 8 ký tự (OTA_PASSWORD trong secrets.h).
 * Không có mật khẩu => OTA bị tắt, tránh việc ai trong cùng mạng cũng nạp được firmware.
 */
class OTAUpdate {
private:
    bool enabled_ = false;

public:
    bool setup(const char* hostname, const char* password) {
        if (!password || strlen(password) < 8) {
            Serial.println(F("⚠ OTA TẮT: chưa đặt OTA_PASSWORD (>= 8 ký tự) trong secrets.h"));
            enabled_ = false;
            return false;
        }

        ArduinoOTA.setHostname(hostname);
        ArduinoOTA.setPassword(password);

        ArduinoOTA.onStart([]() {
            String type = (ArduinoOTA.getCommand() == U_FLASH) ? "firmware" : "filesystem";
            Serial.println("Bắt đầu cập nhật " + type);
        });

        ArduinoOTA.onEnd([]() {
            Serial.println(F("\nKết thúc cập nhật. Khởi động lại..."));
        });

        ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
            if (total >= 100) Serial.printf("Tiến độ: %u%%\n", (progress / (total / 100)));
        });

        ArduinoOTA.onError([](ota_error_t error) {
            Serial.printf("Lỗi OTA[%u]: ", error);
            if (error == OTA_AUTH_ERROR) Serial.println(F("Lỗi xác thực"));
            else if (error == OTA_BEGIN_ERROR) Serial.println(F("Lỗi khởi tạo"));
            else if (error == OTA_CONNECT_ERROR) Serial.println(F("Lỗi kết nối"));
            else if (error == OTA_RECEIVE_ERROR) Serial.println(F("Lỗi nhận dữ liệu"));
            else if (error == OTA_END_ERROR) Serial.println(F("Lỗi kết thúc"));
        });

        ArduinoOTA.begin();
        enabled_ = true;
        Serial.println(F("✅ Đã khởi tạo cập nhật OTA (có mật khẩu)."));
        return true;
    }

    // Gọi trong loop()
    void handle() {
        if (enabled_) ArduinoOTA.handle();
    }
};

#endif // OTA_UPDATE_H
