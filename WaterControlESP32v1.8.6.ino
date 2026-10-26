#include <Arduino.h>
#include <WiFi.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>

// ===== QUẢN LÝ WEB VÀ LƯU =====
#include <Preferences.h> 
#include "WebServerManager.h"
#include <HTTPClient.h>      // Gửi HTTP POST nhật ký AI
#include <ArduinoJson.h>     // Đóng gói dữ liệu AI

// ===== MODULES =====
#include "SharedConfig.h"
#include "ConfigManager.h"
#include "WaterQualityMonitorLib.h"
#include "LCD.h"
#include "WiFiPortal.h"
#include "MCPService.h"
#include "MQTTManager.h" 
#include "OTAUpdate.h"       // OTA (chỉ bật khi có OTA_PASSWORD)

void sendLogToPC(); // khai báo trước

// =================================================
// CẤU HÌNH & HẰNG SỐ
// =================================================
constexpr float TDS_THRESHOLDS[]     = {170.0f, 600.0f, 800.0f}; 
constexpr int   TDS_WARNING_COUNTS[] = {0, 5, 5};
const unsigned long wiFiCheckInterval = 30000; 
const unsigned long LOG_INTERVAL = 10 * 60 * 1000; // 10 phút (tính bằng mili-giây)

// =================================================
// ĐỐI TƯỢNG TOÀN CỤC 
// =================================================
ConfigManager config;
WaterQualityMonitor monitor; 
SensorManager sensorManager; 

WiFiPortal portal;
MCPService mcpService(monitor, config); 
MQTTManager mqtt(getDeviceId(), RELAY_PIN, &monitor); // ID lấy từ MAC hoặc MQTT_DEVICE_ID

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature dsSensors(&oneWire);

WebServer mainServer(MAIN_SERVER_PORT);
WebServerManager serverManager(&mainServer, &portal, &sensorManager);

WaterQualityAI waterAI;
AIResult currentAIResult; 
OTAUpdate ota;

// =================================================
// BIẾN TOÀN CỤC
// =================================================
float tempValues[MAX_SENSORS] = {0};
float tdsValueRaw = 0;
float tdsValueCompensated = 0;

uint8_t numSensors = 3;

bool mcpInitialized = false; 
unsigned long lastDataUpdateTime = 0;
unsigned long lastWiFiCheck = 0;
unsigned long lastAITime = 0;
unsigned long lastLogTime = 0;

// Tài khoản web: nạp / tự sinh ngẫu nhiên ở lần chạy đầu trong WebServerManager::setupMainServer()
String admin_user = "admin";
String admin_pass = "";

float pH_value = 7.0;
bool system_mode = false; // false: Auto, true: Manual
float pH_threshold = DEFAULT_PH_THRESHOLD; 

// =================================================
// SETUP & HELPER
// =================================================
void initSensors() {
    dsSensors.begin();
    
    // 🌟 KCS FIX: Khai báo chân ADC để đọc tín hiệu
    pinMode(TDS_PIN, INPUT);
    pinMode(pH_SENSOR_PIN, INPUT); 

    // Cấu hình LED cảnh báo
    pinMode(LED_TEMP_ALERT_PIN, OUTPUT);
    pinMode(LED_TDS_ALERT_PIN, OUTPUT);
    digitalWrite(LED_TEMP_ALERT_PIN, LOW);
    digitalWrite(LED_TDS_ALERT_PIN, LOW);
}

void controlLEDs() {
    digitalWrite(LED_TEMP_ALERT_PIN, monitor.isAnyTempAlert());
    digitalWrite(LED_TDS_ALERT_PIN, monitor.getTDSWarningState() > 0);
}

void setup() {
    Serial.begin(115200);

    Serial.println(F("\n🔌 Đang khởi động hệ thống giám sát nước..."));

    // Lấy cấu hình pH
    Preferences prefs;
    prefs.begin("sys_config", true); 
    pH_threshold = prefs.getFloat("ph_thresh", DEFAULT_PH_THRESHOLD); 
    prefs.end(); 
    
    if (isnan(pH_threshold) || pH_threshold < 0 || pH_threshold > 14) {
        pH_threshold = DEFAULT_PH_THRESHOLD; 
    }

    config.loadConfig();
    config.printConfig();
    numSensors = config.getNumTempSensors();

    initSensors();
    monitor.begin();   // Khôi phục trạng thái relay đã lưu (NVS đã sẵn sàng)
    lcdInit();
    sensorManager.begin(); 

    portal.begin("ESP32-HD7-Ver1.1", "", 30000); 

    if (!portal.isInAPMode()) {
        serverManager.setupMainServer();
        Serial.println("✅ Web Server Quản lý đã sẵn sàng!");
        
        // OTA chỉ chạy khi có WiFi VÀ có OTA_PASSWORD (>= 8 ký tự)
        ota.setup("HDKP-Wifi-OTA", OTA_PASSWORD);
    }

    xTaskCreatePinnedToCore(WaterQualityMonitor::thingSpeakTask, "ThingSpeakTask", 8192, &monitor, 1, NULL, 0);
    Serial.println(F("🚀 ThingSpeak Task running on Core 0"));

    if (strlen(config.getMcpToken()) == 0) {
        Serial.println("⚠ MCP token chưa được cấu hình. Dùng SETMCP <token>");
    } else {
        String mcpEndpoint = String(config.getMcpToken());
        mcpService.begin(mcpEndpoint.c_str());
        mcpInitialized = true;
        Serial.println("🚀 MCP Đã khởi tạo");
    }
    
    mqtt.begin();
    Serial.println("🧠 Khởi động hệ thống MQTT AI...");
}

// =================================================
// LOOP
// =================================================
void loop() {
    unsigned long now = millis();

    // --- NHÓM 1: CÁC HÀM NỀN TẢNG (LUÔN CHẠY DÙ CÓ MẠNG HAY KHÔNG) ---
    portal.loop();
    config.handleSerialCommand();
    serverManager.handleClients(); 

    // Đọc cảm biến pH và tự động bật bơm chạy liên tục 13/10/26
    sensorManager.readSensors();

    // 🌟 KCS FIX: KHÓA MUTEX TRƯỚC KHI ĐỌC GIÁ TRỊ PH RA NGOÀI ĐỂ TRÁNH NHIỄU GIẬT ĐỘT NGỘT
    portENTER_CRITICAL(&sensorDataMux);
    float safePh = sensorManager.getPHValue(); 
    portEXIT_CRITICAL(&sensorDataMux);
    
    pH_value = safePh; // Gán lại an toàn vào biến toàn cục

    if (!system_mode) {
        sensorManager.autoControl(pH_threshold);
    }
    //========================================================

    // --- NHÓM 2: KẾT NỐI MẠNG (CHỈ CHẠY KHI CÓ WIFI) ---
    if (WiFi.status() == WL_CONNECTED) {
        mqtt.loop();       
        ota.handle();

        if (mcpInitialized) {
            mcpService.loop();
        }

        if (now - lastWiFiCheck >= wiFiCheckInterval) {
            lastWiFiCheck = now;
        }
    } else {
        if (now - lastWiFiCheck >= wiFiCheckInterval) {
            Serial.println(F("⚠ Mất kết nối WiFi. Đang kết nối lại..."));
            WiFi.reconnect();
            lastWiFiCheck = now;
        }
    }

    // --- NHÓM 3: CẬP NHẬT TDS, TEMP, GỬI MQTT & LCD ---
    if (now - lastDataUpdateTime >= DATA_UPDATE_INTERVAL) { 
        lastDataUpdateTime = now;
        
        numSensors = config.getNumTempSensors();

        // A. Đọc Nhiệt độ
        dsSensors.requestTemperatures();
        for (int i = 0; i < numSensors && i < MAX_SENSORS; i++) {
            float t = dsSensors.getTempCByIndex(i);
            tempValues[i] = (t < -100 || t == 85.0) ? DEVICE_DISCONNECTED_C : t;
        }

        // B. Đọc TDS
        // 🌟 KCS FIX: Gọi hàm đọc TDS mới (Tự động bật/tắt nguồn chân 15 và bù nhiệt độ)
        float compTemp = (tempValues[0] > -50) ? tempValues[0] : 25.0;
        tdsValueCompensated = sensorManager.readTDS(compTemp);

        // C. Cập nhật Monitor Logic
        float minTemps[MAX_SENSORS], maxTemps[MAX_SENSORS]; 
        for(int i=0; i<numSensors && i < MAX_SENSORS; i++) {
            minTemps[i] = config.getMinTempThreshold(i);
            maxTemps[i] = config.getMaxTempThreshold(i);
        }

        // 1. Chụp lại trạng thái rơ-le thủ công
        bool userR0 = monitor.getRelayState(0);
        bool userR1 = monitor.getRelayState(1);
        bool userR2 = monitor.getRelayState(2);

        // 2. Cập nhật logic thư viện
        monitor.update(tempValues, tdsValueCompensated, minTemps, maxTemps, TDS_THRESHOLDS, TDS_WARNING_COUNTS);

        // 3. Giữ an toàn trạng thái thủ công
        if (system_mode) {
            monitor.setRelayManual(0, userR0);
            monitor.setRelayManual(1, userR1);
            monitor.setRelayManual(2, userR2);
        }

        // Gửi toàn bộ dữ liệu lên MQTT
        if (WiFi.status() == WL_CONNECTED) {
            mqtt.publishSensor(pH_value, currentAIResult); 
        }

        // D. Cập nhật Output
        controlLEDs();
        
        AlertData currentAlert = monitor.getAlertData();
        lcdUpdate(monitor.getDisplayData(), currentAlert, currentAlert.active);
    }

    // --- NHÓM 4: PHÂN TÍCH AI CHUYÊN SÂU (30s/LẦN) ---
    if (lastAITime == 0 || now - lastAITime > 30000) {
        lastAITime = (now == 0) ? 1 : now; 
        
        currentAIResult = waterAI.analyze(pH_value, monitor.getDisplayData().tds, monitor.getDisplayData().temps[0]);

        if (WiFi.status() == WL_CONNECTED) {
            String payload = "{\"pH\":" + String(pH_value) + 
                             ", \"tds\":" + String(monitor.getDisplayData().tds) + 
                             ", \"ai_risk\":\"" + currentAIResult.risk + "\"}";
            mqtt.publish(mqtt.topic("telemetry"), payload); 
        }
    }

    // Ghi log vào PC
    if (millis() - lastLogTime >= LOG_INTERVAL) {
        lastLogTime = millis();
        sendLogToPC(); 
    }

    delay(10); 
}

// Lưu dữ liệu vào máy tính
void sendLogToPC() {
    if (strlen(AI_SERVER_URL) == 0) return;   // Chưa cấu hình máy chủ AI
    if(WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        
        http.begin(String(AI_SERVER_URL) + "/api/save_log"); 
        http.setTimeout(3000); 
        http.addHeader("Content-Type", "application/json");

        #if ARDUINOJSON_VERSION_MAJOR >= 7
            JsonDocument doc;
        #else            
            DynamicJsonDocument doc(512);
        #endif  

        doc["quality"] = currentAIResult.quality;
        doc["message"] = currentAIResult.message;
        doc["prediction"] = currentAIResult.prediction;
        doc["risk"] = currentAIResult.risk;
        doc["eta"] = currentAIResult.eta;

        String payload;
        serializeJson(doc, payload);
        
        int httpCode = http.POST(payload);
        if(httpCode == 200 || httpCode == 201) {
            Serial.println("📝 Đã gửi Nhật ký AI lên máy tính lưu trữ!");
        } else {
            Serial.printf("⚠️ Lỗi gửi Nhật ký AI. Máy tính không phản hồi (Mã lỗi: %d)\n", httpCode);
        }
        http.end();
    }
}