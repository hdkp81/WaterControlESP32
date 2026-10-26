#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include <ctype.h>
#include <math.h>
#include <cstddef>

// Định nghĩa hằng số mặc định
#define DEFAULT_MIN_TEMP 20.0f
#define DEFAULT_MAX_TEMP 30.0f
#define DEFAULT_TDS_FACTOR 1.0f

// =====================================================
// CẤU TRÚC DỮ LIỆU LƯU PREFERENCES (Thay thế EEPROM)
// Khóa ranh giới Byte (Chống lệch dữ liệu khi đọc từ Flash)
// =====================================================
#pragma pack(push, 1) 
struct ConfigData {
    uint32_t version = 100; // Phiên bản struct để tránh lỗi biên dịch và lệch bộ nhớ

    // Ngưỡng nhiệt độ
    float tempSensorMinThresholds[3] = {DEFAULT_MIN_TEMP, DEFAULT_MIN_TEMP, DEFAULT_MIN_TEMP};
    float tempSensorMaxThresholds[3] = {DEFAULT_MAX_TEMP, DEFAULT_MAX_TEMP, DEFAULT_MAX_TEMP};

    // Hệ số TDS
    float TdsFactor = DEFAULT_TDS_FACTOR;

    // ThingSpeak API Key
    char thingSpeakAPIKey[32] = {0};

    // MCP Token
    char mcpToken[512] = {0};

    // ====== BỔ SUNG ======
    uint8_t numTempSensors = 3;   // 1..3
};
#pragma pack(pop) // Mở lại ranh giới Byte chuẩn

// ======================================================
// HÀM TIỆN ÍCH (Static)
// ======================================================
static bool startsWithIgnoreCase(const String& s, const String& prefix) {
    if (s.length() < prefix.length()) return false;
    for (size_t i = 0; i < prefix.length(); ++i) {
        if (tolower(s.charAt(i)) != tolower(prefix.charAt(i)))
            return false;
    }
    return true;
}

static void normalizeSpaces(String& s) {
    s.trim();
    String out; 
    out.reserve(s.length());
    bool lastSpace = false;
    
    for (size_t i = 0; i < s.length(); ++i) {
        char c = s.charAt(i);
        bool isSpace = (c == ' ' || c == '\t' || c == '\r');
        if (isSpace) {
            if (!lastSpace) out += ' ';
            lastSpace = true;
        } else {
            out += c;
            lastSpace = false;
        }
    }
    s = out;
}

static bool tryParseFloat(String s, float& outVal) {
    s.trim();
    s.replace(',', '.');
    if (s.isEmpty()) return false;
    
    char* endptr = nullptr;
    outVal = strtof(s.c_str(), &endptr);
    return endptr && *endptr == '\0' && isfinite(outVal);
}


// =====================================================
// CLASS CONFIG MANAGER
// =====================================================
class ConfigManager {
public:
    ConfigManager() : calibrationMode(false) {}

    inline void loadConfig() {
        Preferences prefs;
        prefs.begin("sys_config", true); // Mở chế độ Read-Only

        // Kiểm tra phiên bản Struct để chống vỡ bộ nhớ
        if (prefs.isKey("cfg_data")) {
            size_t bytesRead = prefs.getBytes("cfg_data", &_currentConfig, sizeof(_currentConfig));
            
            if (bytesRead != sizeof(ConfigData)) {
                Serial.println(F("⚠️ Kích thước Struct thay đổi! Xóa và nạp cấu hình gốc..."));
                loadDefaults();                                                                                          
            }
        } else {
            loadDefaults();            
        }
        prefs.end();

        // Chốt chặn cuối mảng char bằng \0 để chống tràn vùng nhớ (Buffer Overflow)                                                                                                                        
        _currentConfig.thingSpeakAPIKey[sizeof(_currentConfig.thingSpeakAPIKey) - 1] = '\0';
        _currentConfig.mcpToken[sizeof(_currentConfig.mcpToken) - 1] = '\0';

        // Filter dữ liệu rác TDS
        if (!isfinite(_currentConfig.TdsFactor) || _currentConfig.TdsFactor <= 0.0f || _currentConfig.TdsFactor > 10.0f) {
            _currentConfig.TdsFactor = DEFAULT_TDS_FACTOR;
        }

        // Filter dữ liệu rác Nhiệt độ
        for (int i = 0; i < 3; i++) {
            if (!isfinite(_currentConfig.tempSensorMinThresholds[i])) _currentConfig.tempSensorMinThresholds[i] = DEFAULT_MIN_TEMP;
            if (!isfinite(_currentConfig.tempSensorMaxThresholds[i])) _currentConfig.tempSensorMaxThresholds[i] = DEFAULT_MAX_TEMP;
            
            // Tự động sửa lỗi nếu MIN >= MAX bị lưu nhầm từ bản build cũ
            if (_currentConfig.tempSensorMinThresholds[i] >= _currentConfig.tempSensorMaxThresholds[i]) {
                _currentConfig.tempSensorMinThresholds[i] = DEFAULT_MIN_TEMP;
                _currentConfig.tempSensorMaxThresholds[i] = DEFAULT_MAX_TEMP;
            }
        }

        if (_currentConfig.numTempSensors < 1 || _currentConfig.numTempSensors > 3) {
            _currentConfig.numTempSensors = 1;
        }
    }

    inline void saveConfig() {
        Preferences prefs;
        prefs.begin("sys_config", false); // Chế độ Read/Write
        prefs.putBytes("cfg_data", &_currentConfig, sizeof(_currentConfig));
        prefs.end();
    }

    inline float getMinTempThreshold(uint8_t sensorIndex) const {
        return (sensorIndex < 3) ? _currentConfig.tempSensorMinThresholds[sensorIndex] : 0.0f;
    }

    inline float getMaxTempThreshold(uint8_t sensorIndex) const {
        return (sensorIndex < 3) ? _currentConfig.tempSensorMaxThresholds[sensorIndex] : 0.0f;
    }

    inline float getTdsFactor() const { return _currentConfig.TdsFactor; }

    inline uint8_t getNumTempSensors() const { return _currentConfig.numTempSensors; }          

    inline bool setNumTempSensors(uint8_t n) {
        if (n < 1 || n > 3) return false;
        _currentConfig.numTempSensors = n;
        saveConfig();
        return true;
    }            

    inline const char* getThingSpeakAPIKey() const { return _currentConfig.thingSpeakAPIKey; }

    inline const char* getMcpToken() const { return _currentConfig.mcpToken; }

    inline void printConfig() {
        Serial.println(F("\n--- CONFIG ---"));
        for (int i = 0; i < _currentConfig.numTempSensors; i++) {
            Serial.print(F(" S")); Serial.print(i);
            Serial.print(F(" MIN=")); Serial.print(_currentConfig.tempSensorMinThresholds[i]);
            Serial.print(F(" MAX=")); Serial.println(_currentConfig.tempSensorMaxThresholds[i]);
        }
        Serial.print(F(" TDS Factor: ")); Serial.println(_currentConfig.TdsFactor);
        Serial.print(F(" TS Key: ")); Serial.println(strlen(_currentConfig.thingSpeakAPIKey) ? F("[SET]") : F("[EMPTY]"));
        Serial.print(F(" MCP Token: ")); Serial.println(strlen(_currentConfig.mcpToken) ? F("[SET]") : F("[EMPTY]"));
        Serial.print(F(" Num Temp Sensors: ")); Serial.println(_currentConfig.numTempSensors);
        Serial.println(F("----------------"));
    }

    inline bool setTempThreshold(uint8_t sensorIndex, bool isMin, float value) {
        if (sensorIndex >= 3 || !isfinite(value)) return false;
        
        // 1. Cập nhật thẳng giá trị mà không cần xét MAX cũ
        if (isMin) {
            _currentConfig.tempSensorMinThresholds[sensorIndex] = value;
        } else {
            _currentConfig.tempSensorMaxThresholds[sensorIndex] = value;
        }
        
        // 2. Chốt chặn an toàn thông minh: Tự động đảo chiều nếu người dùng lỡ nhập MIN > MAX
        if (_currentConfig.tempSensorMinThresholds[sensorIndex] > _currentConfig.tempSensorMaxThresholds[sensorIndex]) {
            float temp = _currentConfig.tempSensorMinThresholds[sensorIndex];
            _currentConfig.tempSensorMinThresholds[sensorIndex] = _currentConfig.tempSensorMaxThresholds[sensorIndex];
            _currentConfig.tempSensorMaxThresholds[sensorIndex] = temp;
        }
        
        saveConfig();
        return true;
    }

    // Đặt đồng thời MIN và MAX (tránh lỗi tự đảo chiều khi cập nhật lần lượt từng giá trị)
    inline bool setTempRange(uint8_t sensorIndex, float minV, float maxV) {
        if (sensorIndex >= 3 || !isfinite(minV) || !isfinite(maxV) || minV >= maxV) return false;
        _currentConfig.tempSensorMinThresholds[sensorIndex] = minV;
        _currentConfig.tempSensorMaxThresholds[sensorIndex] = maxV;
        saveConfig();
        return true;
    }

    inline bool setTdsFactor(float value) {
        if (!isfinite(value) || value <= 0.0f || value > 10.0f) return false;
        _currentConfig.TdsFactor = value;
        saveConfig();
        return true;
    }
    
    inline bool setThingSpeakAPIKey(const String& key) {
        if (key.length() >= sizeof(_currentConfig.thingSpeakAPIKey)) return false;
        key.toCharArray(_currentConfig.thingSpeakAPIKey, sizeof(_currentConfig.thingSpeakAPIKey));
        saveConfig();
        return true;
    }

    inline bool setMcpToken(const String& token) {
        if (token.length() >= sizeof(_currentConfig.mcpToken)) return false;
        token.toCharArray(_currentConfig.mcpToken, sizeof(_currentConfig.mcpToken));
        saveConfig();
        return true;
    }

    inline void handleSerialCommand() {
        if (!Serial.available()) return;
        String input = Serial.readStringUntil('\n');
        normalizeSpaces(input);

        if (calibrationMode) {
            if (input.equalsIgnoreCase("CANCEL")) {
                calibrationMode = false;
                Serial.println(F("❌ Thoát chế độ hiệu chuẩn TDS"));
            } else {
                float v;
                if (tryParseFloat(input, v) && v > 0) {
                    _currentConfig.TdsFactor = v;
                    saveConfig();
                    calibrationMode = false;
                    Serial.println("✅ Đã lưu TDS Factor: " + String(v));
                } else {
                    Serial.println(F("⚠ Giá trị không hợp lệ"));
                }
            }
            return;
        }

        if (input.equalsIgnoreCase("CALTDS")) {
            calibrationMode = true;
            Serial.println(F("Nhập hệ số TDS mới hoặc CANCEL"));
            return;
        }

        if (startsWithIgnoreCase(input, "SETKEY ")) {
            String v = input.substring(7); v.trim();
            if (setThingSpeakAPIKey(v)) Serial.println(F("✅ Đã lưu ThingSpeak API Key"));
            else Serial.println(F("⚠ API Key không hợp lệ"));
            return;
        }

        if (startsWithIgnoreCase(input, "SETMCP ")) {
            String v = input.substring(7); v.trim();
            if (setMcpToken(v)) Serial.println(F("✅ Đã lưu MCP token"));
            else Serial.println(F("⚠ MCP token không hợp lệ"));
            return;
        }

        if (startsWithIgnoreCase(input, "CALTEMP ")) {
            String cmd = input.substring(8); normalizeSpaces(cmd);
            int p1 = cmd.indexOf(' '); 
            int p2 = cmd.indexOf(' ', p1 + 1);
            
            if (p1 <= 0 || p2 <= 0) { Serial.println(F("Ví dụ: CALTEMP S0 MIN 25")); return; }
            
            int idx = cmd.substring(1, p1).toInt();
            String type = cmd.substring(p1 + 1, p2);
            String valS = cmd.substring(p2 + 1);
            
            float v;
            if (idx < 0 || idx > 2 || !tryParseFloat(valS, v)) { Serial.println(F("⚠ Tham số sai")); return; }

            if (type.equalsIgnoreCase("MIN")) _currentConfig.tempSensorMinThresholds[idx] = v;
            else if (type.equalsIgnoreCase("MAX")) _currentConfig.tempSensorMaxThresholds[idx] = v;
            else { Serial.println(F("⚠ Phải là MIN hoặc MAX")); return; }

            Serial.println(F("✔ Đã cập nhật – gõ SAVE để lưu"));
            printConfig();
            return;
        }

        if (startsWithIgnoreCase(input, "SETNUMTEMP ")) {
            String v = input.substring(11); v.trim();
            int n = v.toInt();
            if (setNumTempSensors(n)) { 
                Serial.print(F("✅ Đã đặt số cảm biến nhiệt: ")); 
                Serial.println(n); 
            } else { 
                Serial.println(F("⚠ Chỉ cho phép 1..3")); 
            }
            return;
        }

        if (input.equalsIgnoreCase("SAVE")) {
            saveConfig(); 
            Serial.println(F("✅ Đã lưu bằng Preferences")); 
            printConfig();
        } else if (input.equalsIgnoreCase("CANCEL")) {
            loadConfig(); 
            Serial.println(F("❌ Đã hủy thay đổi")); 
            printConfig();
        } else if (input.equalsIgnoreCase("RESET")) {
            Preferences prefs; 
            prefs.begin("sys_config", false); 
            prefs.clear(); 
            prefs.end();
            Serial.println(F("⚠ Preferences đã bị xóa hoàn toàn (gồm cả mật khẩu admin & AP, sẽ tự sinh mới) – Vui lòng reset board"));
        } else if (input.equalsIgnoreCase("HELP")) {
            printHelp_(); 
            printConfig();
        } else {
            Serial.println(F("⚠ Lệnh không hợp lệ – gõ HELP"));
        }
    }

private:
    ConfigData _currentConfig;
    bool calibrationMode = false;   

    // Hàm tạo dữ liệu trắng nếu Flash hỏng hoặc lần nạp Firmware đầu tiên
    inline void loadDefaults() {
        _currentConfig = ConfigData();   // đặt lại về mặc định (tránh memset trên struct có khởi tạo mặc định)
        _currentConfig.version = 100;
        _currentConfig.TdsFactor = DEFAULT_TDS_FACTOR;
        _currentConfig.numTempSensors = 3; // Reset về mặc định là 3 để an toàn
        
        for (int i = 0; i < 3; i++) {
            _currentConfig.tempSensorMinThresholds[i] = DEFAULT_MIN_TEMP;
            _currentConfig.tempSensorMaxThresholds[i] = DEFAULT_MAX_TEMP;
        }
    }
    
    inline void printHelp_() {
        Serial.println(F("\n--- LENH ---"));
        Serial.println(F(" CALTDS"));
        Serial.println(F(" CALTEMP S0|S1|S2 MIN x"));
        Serial.println(F(" CALTEMP S0|S1|S2 MAX x"));
        Serial.println(F(" SETKEY <ThingSpeakKey>"));
        Serial.println(F(" SETMCP <MCP_Token>"));
        Serial.println(F(" SETNUMTEMP <1..3>"));
        Serial.println(F(" SAVE | CANCEL | RESET"));
    }
};

#endif // CONFIGMANAGER_H