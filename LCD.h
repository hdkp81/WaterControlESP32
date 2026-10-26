#ifndef LCD_H
#define LCD_H

#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "ConfigManager.h"
#include "SharedConfig.h" 

extern ConfigManager config;   

// =====================================================
// DỮ LIỆU HIỂN THỊ (DÙNG CHUNG TOÀN HỆ THỐNG)
// =====================================================
struct DisplayData {
    float temps[MAX_SENSORS]; 
    float tds;
    float ph; 
};

// =====================================================
// DỮ LIỆU CẢNH BÁO (DÙNG CHUNG)
// =====================================================
struct AlertData {
    bool active = false;
    char line1[17] = {0};   
    char line2[17] = {0};
};

// =====================================================
// LỚP QUẢN LÝ LCD (HEADER-ONLY SINGLETON)
// =====================================================
class LCDController {
private:
    LiquidCrystal_I2C lcd;
    bool lcdFound;
    bool welcomeShown;
    unsigned long welcomeStart;
    unsigned long lastLcdRefresh;
    unsigned long lastSwitchTime;
    bool showAlertScreen;
    unsigned long alertStartTime;
    unsigned long holdUntil = 0;   // Giữ thông báo tạm thời (vd mật khẩu AP)

    static constexpr unsigned long LCD_REFRESH_RATE = 500;
    static constexpr unsigned long WELCOME_TIME_MS = 3000;
    static constexpr unsigned long ALERT_DELAY_MS = 20000;
    static constexpr unsigned long ALERT_CYCLE_MS = 10000;

    LCDController() : lcd(0x27, 16, 2), lcdFound(false), welcomeShown(false),
                      welcomeStart(0), lastLcdRefresh(0), lastSwitchTime(0),
                      showAlertScreen(true), alertStartTime(0) {}

    String fmt(float v, int d) {
        if (v < -100) return "N/A";
        char buf[8];
        snprintf(buf, sizeof(buf), "%.*f", d, v);
        return String(buf);
    }

    void drawAlert(const AlertData& a) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(a.line1);
        lcd.setCursor(0, 1);
        lcd.print(a.line2);
    }

    void drawData(const DisplayData& d) {
        lcd.clear();
        uint8_t numSensors = config.getNumTempSensors();

        // --- TRƯỜNG HỢP 1: 1 CẢM BIẾN NHIỆT ---
        if (numSensors == 1) {
            lcd.setCursor(0, 0);
            lcd.print("T:");
            lcd.print(fmt(d.temps[0], 1));
            lcd.print((char)223); lcd.print("C"); 
            lcd.print(" pH:");
            lcd.print(fmt(d.ph, 1));              

            lcd.setCursor(0, 1);
            lcd.print("TDS: ");
            lcd.print(fmt(d.tds, 0));
            lcd.print(" ppm");
        } 
        // --- TRƯỜNG HỢP 2 & 3: >= 2 CẢM BIẾN NHIỆT ---
        else {
            lcd.setCursor(0, 0);
            lcd.print("T1:"); 
            lcd.print(fmt(d.temps[0], 1));
            lcd.print(" T2:"); 
            lcd.print(fmt(d.temps[1], 1));

            lcd.setCursor(0, 1);
            
            // TRƯỜNG HỢP 3: Lớn hơn hoặc bằng 3 cảm biến
            if (numSensors >= 3) {
                if ((millis() / 3000) % 2 == 0) {
                    lcd.print("T3:");
                    lcd.print(fmt(d.temps[2], 1));
                    lcd.print("  pH:");
                    lcd.print(fmt(d.ph, 1));
                } else {
                    lcd.print("TDS:");
                    lcd.print(fmt(d.tds, 0));
                    lcd.print(" pH:");
                    lcd.print(fmt(d.ph, 1));
                }
            } 
            // TRƯỜNG HỢP 2: Đúng 2 cảm biến
            else {
                lcd.print("TDS:");
                lcd.print(fmt(d.tds, 0));
                lcd.print(" pH:");
                lcd.print(fmt(d.ph, 1));
            }
        }
    }

public:
    static LCDController& getInstance() {
        static LCDController instance;
        return instance;
    }

    void init() {
        Wire.begin();
        if (Wire.beginTransmission(0x27), Wire.endTransmission() != 0) {
            lcdFound = false;
            return;
        }

        lcdFound = true;
        lcd.init();
        lcd.backlight();

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(" HDKP XIN CHAO ");
        lcd.setCursor(0, 1);
        lcd.print(" Water Control ");

        welcomeStart = millis();
        welcomeShown = false;
    }

    bool isAvailable() {
        return lcdFound;
    }

    // Hiện 2 dòng chữ (tối đa 16 ký tự/dòng) và giữ holdMs mili-giây
    void showMessage(const char* line1, const char* line2, unsigned long holdMs) {
        if (!lcdFound) return;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(line1);
        lcd.setCursor(0, 1);
        lcd.print(line2);
        welcomeShown = true;
        holdUntil = millis() + holdMs;
        if (holdUntil == 0) holdUntil = 1;
    }

    void update(const DisplayData& d, const AlertData& a, bool hasActiveAlert) {
        if (!lcdFound) return;

        unsigned long now = millis();

        // 0. ĐANG GIỮ THÔNG BÁO TẠM THỜI
        if (holdUntil != 0) {
            if ((long)(now - holdUntil) < 0) return;
            holdUntil = 0;
            lcd.clear();
        }

        // 1. CHÀO MỪNG
        if (!welcomeShown) {
            if (now - welcomeStart >= WELCOME_TIME_MS) {
                welcomeShown = true;
                lcd.clear();
                lastSwitchTime = now;
            }
            return;
        }

        // ⛔ CHỐNG NHÁY
        if (now - lastLcdRefresh < LCD_REFRESH_RATE) {
            return; 
        }
        lastLcdRefresh = now; 

        // 2. KHÔNG CÓ CẢNH BÁO
        if (!hasActiveAlert) {
            alertStartTime = 0;
            showAlertScreen = true;
            drawData(d);
            return;
        }

        // 3. CÓ CẢNH BÁO – HIỆN ALERT NGAY
        if (alertStartTime == 0) {
            alertStartTime = now;
            lastSwitchTime = now; 
            showAlertScreen = true;
            drawAlert(a);
            return;
        }

        // 4. CHƯA ĐỦ 20s → CHỈ HIỆN ALERT
        if (now - alertStartTime < ALERT_DELAY_MS) {
            drawAlert(a);
            return;
        }

        // 5. SAU 20s → LUÂN PHIÊN
        if (now - lastSwitchTime >= ALERT_CYCLE_MS) {
            lastSwitchTime = now;
            showAlertScreen = !showAlertScreen;
            lcd.clear(); 
        }

        if (showAlertScreen) {
            drawAlert(a);
        } else {
            drawData(d);
        }
    }
};

// =====================================================
// API LCD
// =====================================================
inline void lcdInit() {
    LCDController::getInstance().init();
}

inline void lcdShowMessage(const char* line1, const char* line2, unsigned long holdMs) {
    LCDController::getInstance().showMessage(line1, line2, holdMs);
}

inline bool lcdIsAvailable() {
    return LCDController::getInstance().isAvailable();
}

inline void lcdUpdate(const DisplayData& data, const AlertData& alert, bool hasActiveAlert) {
    LCDController::getInstance().update(data, alert, hasActiveAlert);
}

#endif // LCD_H