#ifndef MCP_SERVICE_H
#define MCP_SERVICE_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WebSocketMCP.h>

#include "SharedConfig.h"           // Lấy các hằng số hệ thống
#include "WaterQualityMonitorLib.h" // Để truy cập DisplayData, AlertData
#include "ConfigManager.h"          // Ghi dữ liệu cài đặt EEPROM/Preferences

class MCPService {
private:
    WebSocketMCP _client;
    WaterQualityMonitor& _monitor;
    ConfigManager& _config;
    bool _started = false;

    // Callback xử lý sự kiện kết nối
    static inline void onConnection(bool connected) {
        Serial.println(connected ? "✅ [MCP] Connected" : "❌ [MCP] Disconnected");
    }

    // =================================================
    // ĐĂNG KÝ CÁC CÔNG CỤ (TOOLS) CHO AI - FIXED SCHEMA
    // =================================================
    void registerTools() {

        // =================================================
        // 1. SENSOR.GET_ALL
        // =================================================
        _client.registerTool(
            "sensor.get_all",
            "Lay nhiet do 3 cam bien, TDS va chi so pH",
            "{\"type\":\"object\",\"properties\":{}}",
            [this](const String&) {
                DisplayData d = _monitor.getDisplayData();

                // 🌟 TỐI ƯU: Sử dụng buffer tĩnh và snprintf để tránh phân mảnh Heap do String +=
                char resp[128];
                snprintf(resp, sizeof(resp), 
                         "{\"temperatures\":[%.1f,%.1f,%.1f],\"tds\":%.1f,\"pH\":%.2f}",
                         d.temps[0], d.temps[1], d.temps[2], d.tds, d.ph);

                return WebSocketMCP::ToolResponse(String(resp));
            }
        );

        // =================================================
        // 2. RELAY.GET_STATES
        // =================================================
        _client.registerTool(
            "relay.get_states",
            "Lay trang thai cac relay",
            "{\"type\":\"object\",\"properties\":{}}",
            [this](const String&) {
                char resp[128];
                // Do MAX_HARDWARE_RELAYS cố định bằng 3, fix cứng format để tốc độ O(1)
                snprintf(resp, sizeof(resp), 
                         "{\"relay0\":%s,\"relay1\":%s,\"relay2\":%s}",
                         _monitor.getRelayState(0) ? "true" : "false",
                         _monitor.getRelayState(1) ? "true" : "false",
                         _monitor.getRelayState(2) ? "true" : "false");

                return WebSocketMCP::ToolResponse(String(resp));
            }
        );

        // =================================================
        // 3. SENSOR.GET_ALERT_STATE
        // =================================================
        _client.registerTool(
            "sensor.get_alert_state",
            "Trang thai canh bao tong quat",
            "{\"type\":\"object\",\"properties\":{}}",
            [this](const String&) {
                bool active = _monitor.hasActiveAlert();
                uint8_t lvl = _monitor.getTDSWarningState();

                const char* severity = (lvl == 2) ? "critical" : (lvl == 1) ? "warning" : "normal";

                char resp[128];
                snprintf(resp, sizeof(resp), 
                         "{\"active\":%s,\"severity\":\"%s\"}",
                         active ? "true" : "false", severity);

                return WebSocketMCP::ToolResponse(String(resp));
            }
        );

        // =================================================
        // 4. SENSOR.GET_ALERT (FULL – AI ĐỌC)
        // =================================================
        _client.registerTool(
            "sensor.get_alert",
            "Noi dung canh bao chi tiet",
            "{\"type\":\"object\",\"properties\":{}}",
            [this](const String&) {
                if (!_monitor.hasActiveAlert()) {
                    return WebSocketMCP::ToolResponse("{\"active\":false}");
                }
                return WebSocketMCP::ToolResponse(_monitor.buildAlertPayload());
            }
        );

        // =================================================
        // 5. SETUP.NGUONGNHIETDO
        // =================================================
        _client.registerTool(
            "setup.nguong_nhietdo",
            "Thiet lap nguong nhiet do MIN/MAX cho cam bien (S0–S2)",
            R"({
                "type": "object",
                "properties": {
                    "sensor": {"type": "integer", "description": "ID cam bien 0-2"},
                    "type": {"type": "string", "enum": ["min", "max"]},
                    "value": {"type": "number", "description": "Gia tri nhiet do cai dat"}
                },
                "required": ["sensor", "type", "value"]
            })",
            [this](const String& payload) {
                // Tự động tương thích với cả ArduinoJson V6 và V7
                #if ARDUINOJSON_VERSION_MAJOR >= 7
                    JsonDocument doc;
                #else
                    StaticJsonDocument<256> doc;
                #endif

                if (deserializeJson(doc, payload)) {
                    return WebSocketMCP::ToolResponse("{\"ok\":false,\"error\":\"invalid_json\"}");
                }

                int sensor = doc["sensor"] | -1;
                const char* type = doc["type"] | "";
                float value = doc["value"] | NAN;

                if (sensor < 0 || sensor >= MAX_SENSORS || !isfinite(value)) {
                    return WebSocketMCP::ToolResponse("{\"ok\":false,\"error\":\"invalid_params\"}");
                }

                bool isMin;
                if (strcasecmp(type, "min") == 0) isMin = true;
                else if (strcasecmp(type, "max") == 0) isMin = false;
                else {
                    return WebSocketMCP::ToolResponse("{\"ok\":false,\"error\":\"type_must_be_min_or_max\"}");
                }

                bool ok = _config.setTempThreshold(sensor, isMin, value);
                if (!ok) {
                    return WebSocketMCP::ToolResponse("{\"ok\":false,\"error\":\"range_conflict\"}");
                }

                char resp[128];
                snprintf(resp, sizeof(resp), 
                         "{\"ok\":true,\"sensor\":%d,\"type\":\"%s\",\"value\":%.1f}", 
                         sensor, type, value);

                return WebSocketMCP::ToolResponse(String(resp));
            }
        );

        // =================================================
        // 6. SETUP.TDS_FACTOR
        // =================================================
        _client.registerTool(
            "setup.tds_factor",
            "Thiet lap he so TDS (tuong duong CALTDS)",
            R"({
                "type": "object",
                "properties": {
                    "value": {"type": "number", "description": "He so TDS factor"}
                },
                "required": ["value"]
            })",
            [this](const String& payload) {
                #if ARDUINOJSON_VERSION_MAJOR >= 7
                    JsonDocument doc;
                #else
                    StaticJsonDocument<128> doc;
                #endif

                if (deserializeJson(doc, payload)) {
                    return WebSocketMCP::ToolResponse("{\"ok\":false,\"error\":\"invalid_json\"}");
                }

                float value = doc["value"] | NAN;
                if (!isfinite(value) || value <= 0.0f) {
                    return WebSocketMCP::ToolResponse("{\"ok\":false,\"error\":\"invalid_value\"}");
                }

                bool ok = _config.setTdsFactor(value);
                if (!ok) {
                    return WebSocketMCP::ToolResponse("{\"ok\":false,\"error\":\"out_of_range\"}");
                }

                char resp[64];
                snprintf(resp, sizeof(resp), "{\"ok\":true,\"tds_factor\":%.3f}", value);

                return WebSocketMCP::ToolResponse(String(resp));
            }
        );

        Serial.println("⚙️ [MCP] Tools registered successfully");
    }

public:
    // Constructor
    explicit MCPService(WaterQualityMonitor& monitor, ConfigManager& config)
        : _monitor(monitor), _config(config) {}

    // Khởi động dịch vụ với URL endpoint
    void begin(const char* endpoint) {
        if (_started) return;

        _client.begin(endpoint, onConnection);                                 
        registerTools();
        _started = true;

        Serial.println("🚀 [MCP] Service Started");
    }

    // Hàm duy trì kết nối (gọi trong loop)
    void loop() {
        if (_started) {
            _client.loop();
        }
    }

    // Kiểm tra trạng thái
    bool isConnected() {
        return _client.isConnected();
    }
};

#endif // MCP_SERVICE_H