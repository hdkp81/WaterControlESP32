
#ifndef MQTTMANAGER_H
#define MQTTMANAGER_H

#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <cstring>

#include "SharedConfig.h"
#include "WaterQualityMonitorLib.h"
#include "ConfigManager.h"

extern ConfigManager config;
extern SensorManager sensorManager;
extern bool system_mode;

// PubSubClient sử dụng callback dạng con trỏ hàm,
// không hỗ trợ lambda có capture [this].
class MQTTManager;
static MQTTManager* mqttManagerCallbackInstance = nullptr;

class MQTTManager {
private:
    WiFiClientSecure secureClient;
    PubSubClient client;

    WaterQualityMonitor* monitor;

    String deviceId;
    int relayPin;
    bool enabled = false;

    unsigned long lastReconnect = 0;
    unsigned long lastSend = 0;

    const unsigned long reconnectInterval = 5000;
    const unsigned long sendInterval = 5000;

    // Xử lý lệnh điều khiển từ broker.
    void callback(char* topicName, byte* payload, unsigned int length) {
#if MQTT_REMOTE_CONTROL
        if (topicName == nullptr || payload == nullptr) return;

        // Chỉ chấp nhận ON hoặc OFF.
        if (length == 0 || length > 3) return;

        char msg[4];
        memcpy(msg, payload, length);
        msg[length] = '\0';

        // Chỉ nhận lệnh trên topic điều khiển của thiết bị.
        if (String(topicName) != topic("control/pump")) return;

        if (strcmp(msg, "ON") == 0) {
            system_mode = true;
            sensorManager.controlPump(true);
            Serial.println(F("[MQTT] Remote command: pump ON"));
        } else if (strcmp(msg, "OFF") == 0) {
            system_mode = true;
            sensorManager.controlPump(false);
            Serial.println(F("[MQTT] Remote command: pump OFF"));
        } else {
            Serial.println(F("[MQTT] Ignored: invalid pump command"));
        }
#else
        (void)topicName;
        (void)payload;
        (void)length;
#endif
    }

    // Cầu nối callback tĩnh của PubSubClient tới đối tượng MQTTManager.
    static void callbackThunk(
        char* topicName,
        byte* payload,
        unsigned int length
    ) {
        if (mqttManagerCallbackInstance != nullptr) {
            mqttManagerCallbackInstance->callback(
                topicName,
                payload,
                length
            );
        }
    }

    void reconnect() {
        if (!enabled || client.connected()) return;

        if (millis() - lastReconnect < reconnectInterval) return;
        lastReconnect = millis();

        String clientId = "ESP32-" + deviceId;
        String statusTopic = topic("status");

        bool ok = false;

        const bool hasUser = strlen(MQTT_USER) > 0;
        const bool hasPass = strlen(MQTT_PASS) > 0;

        // Không chấp nhận cấu hình tài khoản thiếu một nửa.
        if (hasUser != hasPass) {
            Serial.println(
                F("[MQTT] ERROR: incomplete broker credentials")
            );
            return;
        }

#if MQTT_REMOTE_CONTROL
        // Điều khiển từ xa bắt buộc xác thực broker.
        if (!hasUser || !hasPass) {
            Serial.println(
                F("[MQTT] ERROR: remote control requires credentials")
            );
            return;
        }
#endif

        if (hasUser && hasPass) {
            ok = client.connect(
                clientId.c_str(),
                MQTT_USER,
                MQTT_PASS,
                statusTopic.c_str(),
                1,
                true,
                "offline"
            );
        } else {
#if !MQTT_REMOTE_CONTROL
            // Chỉ cho phép anonymous khi điều khiển từ xa tắt.
            ok = client.connect(
                clientId.c_str(),
                statusTopic.c_str(),
                1,
                true,
                "offline"
            );
#endif
        }

        if (ok) {
            client.publish(statusTopic.c_str(), "online", true);

#if MQTT_REMOTE_CONTROL
            String controlTopic = topic("control/pump");

            if (!client.subscribe(controlTopic.c_str(), 1)) {
                Serial.println(
                    F("[MQTT] WARNING: control subscription failed")
                );
            }
#endif

            publishConfig();

            Serial.println(F("[MQTT] Connected securely"));
        } else {
            Serial.printf(
                "[MQTT] Connection failed, state=%d\n",
                client.state()
            );
        }
    }

public:
    MQTTManager(String id, int relay, WaterQualityMonitor* mon)
        : client(secureClient),
          monitor(mon),
          deviceId(id),
          relayPin(relay) {
        // Bản này thiết kế cho một MQTTManager duy nhất.
        mqttManagerCallbackInstance = this;
    }

    // watermonitor/<deviceId>/<suffix>
    String topic(const char* suffix) const {
        return "watermonitor/" + deviceId + "/" + suffix;
    }

    void begin() {
        enabled = false;

        // 1. Không cấu hình broker thì MQTT tắt.
        if (strlen(MQTT_HOST) == 0) {
            Serial.println(
                F("[MQTT] Disabled: MQTT_HOST is empty")
            );
            return;
        }

        // 2. Username và password phải được cấu hình đồng bộ.
        const bool hasUser = strlen(MQTT_USER) > 0;
        const bool hasPass = strlen(MQTT_PASS) > 0;

        if (hasUser != hasPass) {
            Serial.println(
                F("[MQTT] ERROR: MQTT_USER and MQTT_PASS must both be configured")
            );
            return;
        }

        // 3. Điều khiển từ xa bắt buộc xác thực.
#if MQTT_REMOTE_CONTROL
        if (!hasUser || !hasPass) {
            Serial.println(
                F("[MQTT] ERROR: remote control requires credentials")
            );
            return;
        }
#endif

        // 4. Bắt buộc xác minh chứng chỉ TLS.
#ifndef MQTT_CA_CERT
        Serial.println(
            F("[MQTT] ERROR: MQTT_CA_CERT is not configured")
        );
        return;
#else
        if (strlen(MQTT_CA_CERT) == 0) {
            Serial.println(
                F("[MQTT] ERROR: MQTT_CA_CERT is empty")
            );
            return;
        }

        secureClient.setCACert(MQTT_CA_CERT);
#endif

        // 5. Cấu hình client sau khi kiểm tra bảo mật.
        client.setBufferSize(1024);
        client.setServer(MQTT_HOST, MQTT_PORT);
        client.setCallback(&MQTTManager::callbackThunk);

        enabled = true;

        Serial.println(
            F("[MQTT] Security checks passed; MQTT enabled")
        );
    }

    void loop() {
        if (!enabled || WiFi.status() != WL_CONNECTED) return;

        if (!client.connected()) {
            reconnect();
        }

        client.loop();
    }

    // Gửi dữ liệu cảm biến và kết quả AI.
    void publishSensor(float ph, const AIResult& aiResult) {
        if (!enabled || !client.connected() || monitor == nullptr) return;
        if (millis() - lastSend < sendInterval) return;

        DisplayData data = monitor->getDisplayData();

#if ARDUINOJSON_VERSION_MAJOR >= 7
        JsonDocument doc;
#else
        DynamicJsonDocument doc(1024);
#endif

        doc["pH_value"] = ph;
        doc["tds_value"] = data.tds;
        doc["temp_value"] =
            (data.temps[0] > -50) ? data.temps[0] : 0;

        doc["pump_status"] = sensorManager.getPumpStatus();
        doc["system_mode"] = system_mode ? "manual" : "auto";
        doc["free_heap"] = ESP.getFreeHeap();

        doc["quality"] = aiResult.quality;
        doc["ai_message"] = aiResult.message;
        doc["prediction"] = aiResult.prediction;
        doc["risk"] = aiResult.risk;
        doc["eta"] = aiResult.eta;

        String buffer;
        serializeJson(doc, buffer);

        client.publish(topic("data").c_str(), buffer.c_str());

        if (aiResult.quality.length() > 0 &&
            aiResult.quality != "TỐT") {
            client.publish(
                topic("alert").c_str(),
                aiResult.message.c_str()
            );
        }

        lastSend = millis();
    }

    // Gửi bản tin tùy ý.
    bool publish(const String& topicStr, const String& payload) {
        if (!enabled || !client.connected()) return false;

        return client.publish(
            topicStr.c_str(),
            payload.c_str()
        );
    }

    // Chỉ gửi thông số không nhạy cảm.
    void publishConfig() {
        if (!enabled || !client.connected()) return;

#if ARDUINOJSON_VERSION_MAJOR >= 7
        JsonDocument doc;
#else
        StaticJsonDocument<128> doc;
#endif

        doc["tds_factor"] = config.getTdsFactor();
        doc["num_sensors"] = config.getNumTempSensors();

        String buffer;
        serializeJson(doc, buffer);

        client.publish(
            topic("config/state").c_str(),
            buffer.c_str(),
            true
        );
    }
};

#endif // MQTTMANAGER_H
