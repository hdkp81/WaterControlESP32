#ifndef WATER_QUALITY_MONITOR_LIB_H
#define WATER_QUALITY_MONITOR_LIB_H

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "SharedConfig.h" 
#include "LCD.h" 
#include "ConfigManager.h"

constexpr float TEMP_HYSTERESIS = 0.1f; 
constexpr unsigned long TEMP_RELAY_DELAY_MS = 5000; 
constexpr unsigned long THINGSPEAK_INTERVAL_MS = 60000;
constexpr unsigned long SERIAL_LOG_INTERVAL_MS = 10000; 

#ifndef DEVICE_DISCONNECTED_C
#define DEVICE_DISCONNECTED_C -127.0f
#endif

extern float pH_value;
extern bool system_mode;
extern ConfigManager config;   

#ifndef AI_HISTORY_SIZE
#define AI_HISTORY_SIZE 30
#endif   

static portMUX_TYPE sensorDataMux = portMUX_INITIALIZER_UNLOCKED;

struct SensorData {
    float temps[MAX_SENSORS]; 
    float tds;
    bool valid;
};

struct AIResult {
    String quality;
    String action;
    String message;
    String prediction;
    String risk;
    int healthScore; 
    int eta;   
    int confidence;  
};

enum class RelayControlMode {
    AUTO,
    MANUAL
};

class SensorManager {
private:
    bool pumpStatus = false;
    uint32_t lastSampleTime = 0;
    int sampleIndex = 0;
    int phArray[9];
    uint32_t lastTdsReadTime = 0;
    float cachedTDS = 0.0;
    uint32_t pumpOnSince = 0;
    uint32_t pumpLockoutUntil = 0;
    bool pumpLockout = false;
    bool sensorFaultLogged = false;
    
public:
    void begin() {
        pinMode(RELAY_PIN, OUTPUT);
        digitalWrite(RELAY_PIN, HIGH); 
        pinMode(pH_SENSOR_PIN, INPUT);
    }

    void readSensors() {
        if (millis() - lastSampleTime < 30) return; 
        lastSampleTime = millis();

        if (sampleIndex == 0) {
            analogRead(pH_SENSOR_PIN); 
            sampleIndex++;
            return;
        }

        phArray[sampleIndex - 1] = analogRead(pH_SENSOR_PIN);
        sampleIndex++;

        if (sampleIndex > 9) { 
            sampleIndex = 0; 

            for (int i = 0; i < 8; i++) {
                for (int j = i + 1; j < 9; j++) {
                    if (phArray[i] > phArray[j]) {
                        int temp = phArray[i];
                        phArray[i] = phArray[j];
                        phArray[j] = temp;
                    }
                }
            }

            int spread = phArray[8] - phArray[0];
            if (spread > 500) {
                pH_value = -1; 
                return;
            }

            float avgAnalog = 0;
            for(int i = 2; i <= 6; i++) {
                avgAnalog += phArray[i];
            }
            avgAnalog /= 5.0;

            float espVoltage = avgAnalog * (3.3 / 4095.0);
            float sensorVoltage = espVoltage * 1.5;

            if(sensorVoltage < 0.2 || sensorVoltage > 4.8) {
                pH_value = -1;
                return;
            }

            float rawPH = 7.0 + ((2.71 - sensorVoltage) / 0.18);

            static float filteredPH = -1.0; 
            if (filteredPH < 0.0 || pH_value == -1) {
                filteredPH = rawPH; 
            } else {
                float alpha = 0.15;
                filteredPH = alpha * rawPH + (1.0 - alpha) * filteredPH;
            }

            float tempPh = filteredPH;
            if (tempPh < -1.0 || tempPh > 13.5) {
                tempPh = -1; 
                filteredPH = -1; 
            } else {
                tempPh = constrain(tempPh, 0.0, 14.0);
            }
            
            portENTER_CRITICAL(&sensorDataMux);
            pH_value = tempPh;
            portEXIT_CRITICAL(&sensorDataMux);
        }
    }

    float readTDS(float currentTemp) {
        if (millis() - lastTdsReadTime >= 60000 || lastTdsReadTime == 0) {
            lastTdsReadTime = millis();

            analogRead(TDS_PIN); 
            delay(10);
            long analogSum = 0;
            for (int i = 0; i < TDS_SAMPLE_COUNT; i++) {
                analogSum += analogRead(TDS_PIN);
                delay(2);
            }

            float voltage = (analogSum / (float)TDS_SAMPLE_COUNT) * VOLTAGE_SCALE;
            if (voltage < 0.1) voltage = 0; 

            float rawTds = voltage * config.getTdsFactor() * 1000;
            float compTemp = (currentTemp > -50) ? currentTemp : 25.0;
            cachedTDS = rawTds / (1.0 + (TDS_TEMP_COMP_COEFF * (compTemp - TDS_REFERENCE_TEMP)));
        }
        return cachedTDS; 
    }

    void controlPump(bool status) {
        if (status && !pumpStatus) pumpOnSince = millis();
        pumpStatus = status;
        digitalWrite(RELAY_PIN, status ? LOW : HIGH); 
        Serial.println("💧 Pump " + String(status ? "ON" : "OFF"));
    }

    // Chế độ AUTO có lớp bảo vệ:
    //  - Cảm biến pH lỗi  -> TẮT bơm (fail-safe) thay vì giữ nguyên trạng thái.
    //  - Chạy liên tục quá PUMP_MAX_RUN_MS -> tắt và nghỉ PUMP_COOLDOWN_MS.
    void autoControl(float threshold) {
        uint32_t now = millis();

        if (pH_value <= 0.5 || pH_value >= 14.0) {
            if (!sensorFaultLogged) {
                Serial.println("⚠️ Lỗi cảm biến pH. Auto tạm dừng và TẮT bơm an toàn!");
                sensorFaultLogged = true;
            }
            if (pumpStatus) controlPump(false);
            return;
        }
        sensorFaultLogged = false;

        if (pumpStatus && (uint32_t)(now - pumpOnSince) >= PUMP_MAX_RUN_MS) {
            controlPump(false);
            pumpLockout = true;
            pumpLockoutUntil = now + PUMP_COOLDOWN_MS;
            Serial.println("⏱️ AUTO: Bơm chạy quá thời gian tối đa, tắt và nghỉ để bảo vệ.");
            return;
        }
        if (pumpLockout) {
            if ((int32_t)(now - pumpLockoutUntil) < 0) return;
            pumpLockout = false;
        }

        if (pH_value > threshold && !pumpStatus) {
            controlPump(true);
            Serial.println("🤖 AUTO: Bật bơm - pH VƯỢT ngưỡng");
        } 
        else if (pH_value < (threshold - 0.5) && pumpStatus) {
            controlPump(false);
            Serial.println("🤖 AUTO: Tắt bơm - pH ĐÃ ỔN ĐỊNH");
        }
    }

    float getPHValue() { return pH_value; }
    bool getPumpStatus() { return pumpStatus; }
};

class WaterQualityAI {
private:
    float phHistory[AI_HISTORY_SIZE];
    float tdsHistory[AI_HISTORY_SIZE];
    float tempHistory[AI_HISTORY_SIZE];

    unsigned long timeHistory[AI_HISTORY_SIZE];
    int indexHistory = 0;
    bool historyFilled = false;

    float lastPh = 7.0f;
    float lastTds = 300.0f;
    float lastTemp = 25.0f;

    bool dirtyWaterAlarm = false;
    bool acidAlarm = false;
    bool alkalineAlarm = false;
    int spikeCountPh = 0;

    float adaptiveFilter(float oldVal, float newVal) {
        float diff = fabsf(newVal - oldVal);
        float alpha;

        if(diff > 1.0f) alpha = 0.50f;
        else if(diff > 0.3f) alpha = 0.25f;
        else alpha = 0.08f;

        return oldVal * (1.0f - alpha) + newVal * alpha;
    }

    float calculateSlope(float data[]) {
        int n = historyFilled ? AI_HISTORY_SIZE : indexHistory;
        if(n < 5) return 0;

        double sumX = 0, sumY = 0, sumXY = 0, sumXX = 0;
        unsigned long firstTime = 0;

        for(int i = 0; i < n; i++) {
            int idx = historyFilled ? (indexHistory + i) % AI_HISTORY_SIZE : i;
            if(i == 0) firstTime = timeHistory[idx];

            float x = (timeHistory[idx] - firstTime) / 60000.0f;
            float y = data[idx];

            sumX += x; sumY += y; sumXY += x * y; sumXX += x * x;
        }

        double denom = (n * sumXX) - (sumX * sumX);
        if(fabs(denom) < 0.0001) return 0;
        return (n * sumXY - sumX * sumY) / denom;
    }

    int calculateConfidence(float ph, float tds) {
        int confidence = 100;
        if(ph < 0 || ph > 14) confidence -= 60;
        if(tds < 0) confidence -= 60;
        if(fabs(ph - lastPh) > 2.0) confidence -= 20;
        if(fabs(tds - lastTds) > 250) confidence -= 20;
        return constrain(confidence, 0, 100);
    }

    int calculateRiskScore(float ph, float tds, float temp, float phSlope, float tdsSlope) {
        int score = 0;
        if(ph < 6.5f) score += (int)((6.5f - ph) * 20.0f);
        if(ph > 8.5f) score += (int)((ph - 8.5f) * 20.0f);
        if(tds > 450) score += constrain((int)((tds - 450) / 8), 0, 40);
        if(temp > 34) score += constrain((int)((temp - 34) * 5), 0, 20);
        
        if(tdsSlope > 5) score += 15;
        if(phSlope < -0.03f) score += 10;
        if(phSlope > 0.03f) score += 10;
        return constrain(score, 0, 100);
    }

public:
    AIResult analyze(float ph, float tds, float temp) {
        AIResult result;

        result.quality = "TỐT";
        result.message = "Hệ thống ổn định";
        result.action = "NONE";
        result.prediction = "Ổn định";
        result.risk = "THẤP";
        result.eta = 999;
        result.confidence = 100;

        if(ph < 0 || ph > 14.0f) {
            result.quality = "LỖI";
            result.message = "Mất kết nối cảm biến pH";
            result.action = "CHECK_PH_SENSOR";
            result.prediction = "AI tạm dừng";
            result.risk = "CAO";
            result.eta = 0;
            result.confidence = 0;
            return result;
        }

        bool snapPh = false;         

        if(indexHistory > 0 || historyFilled) {
            if(fabsf(tds - lastTds) > 180) tds = lastTds;
            if(fabsf(ph - lastPh) > 1.5f) {
                spikeCountPh++;
                if (spikeCountPh < 5) ph = lastPh;
                else { spikeCountPh = 0; snapPh = true; }
            } else spikeCountPh = 0; 
        }

        float cleanPh = snapPh ? ph : adaptiveFilter(lastPh, ph);
        float cleanTds = adaptiveFilter(lastTds, tds);
        float cleanTemp = adaptiveFilter(lastTemp, temp);

        phHistory[indexHistory] = cleanPh;
        tdsHistory[indexHistory] = cleanTds;
        tempHistory[indexHistory] = cleanTemp;
        timeHistory[indexHistory] = millis();
        indexHistory++;

        if(indexHistory >= AI_HISTORY_SIZE) {
            indexHistory = 0;
            historyFilled = true;
        }

        float phSlope = calculateSlope(phHistory);
        float tdsSlope = calculateSlope(tdsHistory);

        result.confidence = calculateConfidence(cleanPh, cleanTds);
        int rScore = calculateRiskScore(cleanPh, cleanTds, cleanTemp, phSlope, tdsSlope);

        if(rScore < 25) result.risk = "THẤP";
        else if(rScore < 55) { result.risk = "TRUNG BÌNH"; result.quality = "CHÚ Ý"; }
        else { result.risk = "CAO"; result.quality = "XẤU"; }

        if(tdsSlope > 0.5f) {
            result.prediction = "TDS đang tăng";
            float gap = 800.0f - cleanTds;
            result.eta = (gap > 0) ? (int)(gap / tdsSlope) : 0;
        }
        else if(phSlope < -0.02f) {
            result.prediction = "pH đang giảm";
            float gap = cleanPh - 6.0f;
            result.eta = (gap > 0) ? (int)(gap / fabsf(phSlope)) : 0;
        }
        else if(phSlope > 0.02f) {
            result.prediction = "pH đang tăng";
            float gap = 8.5f - cleanPh;
            result.eta = (gap > 0) ? (int)(gap / phSlope) : 0;
        }
        result.eta = constrain(result.eta, 0, 10080);

        String detailedMsg = "";

        if(cleanTds > 800) dirtyWaterAlarm = true;
        else if(cleanTds < 700) dirtyWaterAlarm = false;

        if(cleanPh < 6.5f) acidAlarm = true;
        else if(cleanPh > 6.8f) acidAlarm = false;

        if(cleanPh > 8.5f) alkalineAlarm = true;
        else if(cleanPh < 8.2f) alkalineAlarm = false;

        if(dirtyWaterAlarm) { result.action = "THAY_NUOC"; detailedMsg += "[Nước bẩn - TDS cao vượt ngưỡng]. "; }
		
        if(acidAlarm) { 
            if(result.action == "NONE") result.action = "TANG_PH"; 
            detailedMsg += "[Cảnh báo: Nước đang có tính AXIT - Độ pH quá thấp!]. "; 
        } 
        else if(alkalineAlarm) { 
            if(result.action == "NONE") result.action = "GIAM_PH"; 
            detailedMsg += "[Cảnh báo: Nước đang có tính KIỀM - Độ pH quá cao!]. "; 
        }

        if(cleanTemp > 34.0f) detailedMsg += "[Nhiệt độ nước đang quá nóng]. ";
        if (detailedMsg != "") result.message = detailedMsg;

        lastPh = cleanPh; lastTds = cleanTds; lastTemp = cleanTemp;
        return result;
    }
};

extern SensorManager sensorManager;

// =====================================================
// LỚP 3: QUẢN LÝ CHẤT LƯỢNG NƯỚC CHÍNH (MONITOR)
// =====================================================
class WaterQualityMonitor {
private:
    DisplayData _display{};
    AlertData   _alert{};
    bool _relayState[MAX_HARDWARE_RELAYS]{};
    RelayControlMode _relayMode[MAX_HARDWARE_RELAYS]{};
    unsigned long _tempViolationStart[MAX_HARDWARE_RELAYS]{};
    uint8_t _tdsCounters[NUM_TDS_THRESHOLDS]{};
    uint8_t _tdsWarningLevel = 0;
    uint8_t _currentAlertLevel = 0;

    int getRelayPin(int idx) const {
        switch (idx) {
            case 0: return RELAY_PIN_0;
            case 1: return RELAY_PIN_1;
            case 2: return RELAY_PIN_2;
            default: return -1;
        }
    }

    // Chỉ ghi flash khi giá trị thực sự thay đổi (tránh mòn flash)
    void saveRelayState(uint8_t idx, bool on) {
        Preferences prefs;
        if (!prefs.begin("relay_cfg", false)) return;
        String key = "rl_" + String(idx);
        if (prefs.getBool(key.c_str(), !on) != on) prefs.putBool(key.c_str(), on);
        prefs.end();
    }

    void toggleRelay(uint8_t idx, bool on) {
        if (idx >= MAX_HARDWARE_RELAYS) return;
        _relayState[idx] = on;
        digitalWrite(getRelayPin(idx), on ? HIGH : LOW);
        Serial.printf("⚡ [ACTION] Relay %d chuyển sang trạng thái: %s\n", idx+1, on ? "ON" : "OFF");
    }

    void checkTemperature(float temp, uint8_t idx, float minT, float maxT) {
        if (_relayMode[idx] == RelayControlMode::MANUAL) return;
        if (temp == DEVICE_DISCONNECTED_C || temp < -50) return; 
        
        unsigned long now = millis();
        bool shouldTurnOff = false, shouldTurnOn = false;

        if (temp > maxT + TEMP_HYSTERESIS) {
            if (_tempViolationStart[idx] == 0) _tempViolationStart[idx] = now;
            if (now - _tempViolationStart[idx] >= TEMP_RELAY_DELAY_MS) {
                shouldTurnOff = true;
                _currentAlertLevel = (_currentAlertLevel < 1) ? 1 : _currentAlertLevel;
                if (!_alert.active) { _alert.active = true; snprintf(_alert.line1, 17, "CANH BAO NHIET"); snprintf(_alert.line2, 17, "S%d QUA NONG", idx + 1); }
            }
        } else if (temp < minT - TEMP_HYSTERESIS) {
            if (_tempViolationStart[idx] == 0) _tempViolationStart[idx] = now;
            if (now - _tempViolationStart[idx] >= TEMP_RELAY_DELAY_MS) {
                shouldTurnOn = true;
                _currentAlertLevel = (_currentAlertLevel < 1) ? 1 : _currentAlertLevel;
                if (!_alert.active) { _alert.active = true; snprintf(_alert.line1, 17, "CANH BAO NHIET"); snprintf(_alert.line2, 17, "S%d QUA LANH", idx + 1); }            
            }
        } else _tempViolationStart[idx] = 0;

        if (shouldTurnOff && _relayState[idx]) toggleRelay(idx, false);
        if (shouldTurnOn && !_relayState[idx]) toggleRelay(idx, true);
    }

    void checkTDS(float tds, const float* th, const int* cnt) {
        _tdsWarningLevel = 0;
        for (uint8_t i = 0; i < NUM_TDS_THRESHOLDS; i++) {
            if (tds > th[i]) {
                if (_tdsCounters[i] < cnt[i]) _tdsCounters[i]++;
                if (cnt[i] > 0 && _tdsCounters[i] >= cnt[i]) _tdsWarningLevel = i + 1; 
            } else _tdsCounters[i] = 0;
        }

        if (_tdsWarningLevel > 0) {
            _currentAlertLevel = max(_currentAlertLevel, (uint8_t)(_tdsWarningLevel >= 2 ? 2 : 1));
            if (!_alert.active) {
                _alert.active = true;
                snprintf(_alert.line1, 17, "CANH BAO NUOC"); 
                if (_tdsWarningLevel >= 2) snprintf(_alert.line2, 17, "TDS RAT CAO:%.0f", tds);
                else snprintf(_alert.line2, 17, "TDS CAO: %.0f", tds);
            }
        }
    }

public:
    WaterQualityMonitor() {
        // Chỉ đặt trạng thái an toàn (tất cả OFF). NVS chưa sẵn sàng ở giai đoạn khởi tạo toàn cục.
        for (size_t i = 0; i < MAX_HARDWARE_RELAYS; i++) {
            _relayState[i] = false;
            _relayMode[i] = RelayControlMode::AUTO;
            _tempViolationStart[i] = 0;
            pinMode(getRelayPin(i), OUTPUT);
            digitalWrite(getRelayPin(i), LOW);
        }
    }

    // Gọi 1 lần trong setup(): khôi phục trạng thái relay đã lưu lần cuối
    void begin() {
        Preferences prefs;
        if (!prefs.begin("relay_cfg", true)) return;
        for (size_t i = 0; i < MAX_HARDWARE_RELAYS; i++) {
            String key = "rl_" + String(i);
            if (prefs.getBool(key.c_str(), false)) toggleRelay(i, true);
        }
        prefs.end();
    }

    void update(const float* temps, float tds, const float* minT, const float* maxT, const float* tdsTh, const int* tdsCnt) {
        uint8_t currentNumSensors = config.getNumTempSensors();                                                                                                              
        portENTER_CRITICAL(&sensorDataMux);
        memset(_display.temps, 0, sizeof(_display.temps));
        for(int i=0; i<currentNumSensors && i < MAX_SENSORS; i++) _display.temps[i] = temps[i];
        _display.tds = tds; _display.ph = pH_value;
        portEXIT_CRITICAL(&sensorDataMux);

        _currentAlertLevel = 0; _alert.active = false; 

        for (size_t i = 0; i < currentNumSensors && i < MAX_HARDWARE_RELAYS; i++) checkTemperature(temps[i], i, minT[i], maxT[i]);
        for (size_t i = currentNumSensors; i < MAX_HARDWARE_RELAYS; i++) { if (_relayState[i]) toggleRelay(i, false); }                                                                                                                    

        checkTDS(tds, tdsTh, tdsCnt);

        static unsigned long lastLogTime = 0;
        if (millis() - lastLogTime >= SERIAL_LOG_INTERVAL_MS) {
            lastLogTime = millis();
            Serial.println("\n------------------------------------------------");
            Serial.println("📊 [MONITOR] TRẠNG THÁI HỆ THỐNG");
            Serial.printf("🧪 pH   : %.2f | Bơm: %s | Chế độ: %s\n", pH_value, sensorManager.getPumpStatus() ? "ON" : "OFF", system_mode ? "MANUAL" : "AUTO");
            Serial.print("🌡  TEMP : ");
            for (size_t i = 0; i < currentNumSensors ; i++) Serial.printf("[S%d: %.2f°C]  ", i+1, temps[i]);
            Serial.println();
            Serial.printf("💧 TDS  : %.2f ppm (Cảnh báo mức: %d)\n", tds, _tdsWarningLevel);
            Serial.print("🔌 RELAY: ");
            for (size_t i = 0; i < MAX_HARDWARE_RELAYS; i++) {
                 if (i < currentNumSensors) Serial.printf("[R%d: %s]  ", i+1, _relayState[i] ? "ON" : "OFF");
                 else Serial.printf("[R%d: DIS]  ", i+1);                                                                                                                  
            }
            Serial.println();

            if (_alert.active) { Serial.println("⚠️  [ALERT] PHÁT HIỆN CẢNH BÁO:"); Serial.printf("    -> %s\n", _alert.line1); Serial.printf("    -> %s\n", _alert.line2); }
            else if (_tdsWarningLevel > 0) Serial.printf("⚠️  [ALERT-BG] TDS Mức %d\n", _tdsWarningLevel);
            else Serial.println("✅  [STATUS] Hệ thống ổn định.");
            Serial.println("------------------------------------------------");
        }
    }

    SensorData getLatestData() {
        SensorData dataSnapshot; 
        uint8_t n = config.getNumTempSensors();
        portENTER_CRITICAL(&sensorDataMux);
        for (int i = 0; i < n && i < MAX_SENSORS; i++) dataSnapshot.temps[i] = _display.temps[i];
        for (int i = n; i < MAX_SENSORS; i++) dataSnapshot.temps[i] = DEVICE_DISCONNECTED_C; 
        dataSnapshot.tds = _display.tds; dataSnapshot.valid = true;
        portEXIT_CRITICAL(&sensorDataMux);
        return dataSnapshot;
    }

    static void thingSpeakTask(void *pvParameters) {
        WaterQualityMonitor* monitor = (WaterQualityMonitor*)pvParameters;
        HTTPClient http; String thingSpeakURL = "https://" + String(THINGSPEAK_SERVER) + "/update";

        for (;;) {
            String apiKey = config.getThingSpeakAPIKey();
            uint8_t currentNumSensors = config.getNumTempSensors();                                                                                                                                                                                                                                                          
            if (WiFi.status() != WL_CONNECTED) { Serial.println("🌐 [WiFi] Mất kết nối, đang đợi..."); vTaskDelay(5000 / portTICK_PERIOD_MS); continue; }
            if (apiKey.length() == 0) { vTaskDelay(THINGSPEAK_INTERVAL_MS / portTICK_PERIOD_MS); continue; } // Chưa cấu hình key
            SensorData sensorReadings = monitor->getLatestData();
            String payload = "api_key=" + apiKey + "&field1=" + String(sensorReadings.tds, 2);
            for (int i = 0; i < currentNumSensors && i < MAX_SENSORS; i++) {
                if (sensorReadings.temps[i] > -100) payload += "&field" + String(i + 2) + "=" + String(sensorReadings.temps[i], 2);
            }
            payload += "&field5=" + String(monitor->getRelayState(0)) + "&field6=" + String(monitor->getRelayState(1)) + "&field7=" + String(monitor->getRelayState(2)) ;
            if (pH_value >= 0) payload += "&field8=" + String(pH_value, 2);

            if (http.begin(thingSpeakURL)) { 
                http.addHeader("Content-Type", "application/x-www-form-urlencoded");
                int httpCode = http.POST(payload);
                if (httpCode == 200) Serial.println(F("☁️ [ThingSpeak] Gửi dữ liệu thành công (200 OK)."));
                else Serial.printf("❌ [ThingSpeak] Lỗi gửi: %d\n", httpCode);
                http.end();
            }
            vTaskDelay(THINGSPEAK_INTERVAL_MS / portTICK_PERIOD_MS);
        }
    }

    bool getRelayState(uint8_t i) const { return i < MAX_HARDWARE_RELAYS ? _relayState[i] : false; }
    uint8_t getTDSWarningState() const { return _tdsWarningLevel; }
    DisplayData getDisplayData() const { return _display; }
    AlertData getAlertData() { AlertData a = _alert; _alert.active = false; return a; }
    bool isAnyTempAlert() const { return _alert.active; }
    bool setRelayManual(uint8_t idx, bool on) { if (idx >= MAX_HARDWARE_RELAYS) return false; _relayMode[idx] = RelayControlMode::MANUAL; toggleRelay(idx, on); saveRelayState(idx, on); return true; }
    void setRelayAuto(uint8_t idx) { if (idx < MAX_HARDWARE_RELAYS) _relayMode[idx] = RelayControlMode::AUTO; }
    bool hasActiveAlert() const { return _currentAlertLevel > 0; }

    String buildAlertPayload() const {
        String summary = (_tdsWarningLevel >= 2) ? "TDS rat cao" : (_tdsWarningLevel == 1 ? "TDS cao" : "Canh bao nhiet do");
        return "{\"schema\":\"xiaozhi.sensor.alert.v1\",\"severity\":\"" + String(_currentAlertLevel == 2 ? "critical" : "warning") + "\",\"summary\":\"" + summary + "\"}";
    }
};

#endif // WATER_QUALITY_MONITOR_LIB_H