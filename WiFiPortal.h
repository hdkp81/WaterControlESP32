#ifndef WIFIPORTAL_H
#define WIFIPORTAL_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <vector>
#include "SharedConfig.h" 
#include "LCD.h"

#define DNS_PORT 53

struct WiFiNetwork {
    String ssid;
    int32_t rssi;
    bool secure;
};

// Escape ký tự đặc biệt để chèn an toàn vào HTML (chống XSS)
static String htmlEscape(const String& in) {
    String out; out.reserve(in.length() + 8);
    for (size_t i = 0; i < in.length(); i++) {
        char c = in.charAt(i);
        switch (c) {
            case '&':  out += "&amp;";  break;
            case '<':  out += "&lt;";   break;
            case '>':  out += "&gt;";   break;
            case '"':  out += "&quot;"; break;
            case '\'': out += "&#39;";  break;
            default:   out += c;
        }
    }
    return out;
}

namespace WiFiPortalData {
    static std::vector<WiFiNetwork> wifiList;
}

class WiFiPortal {
private:
    String apSsid_;
    String apPass_;
    uint32_t connectTimeoutMs_ = 15000;
    bool isAPMode_ = false; 

    DNSServer dnsServer;
    WebServer server{80};
    Preferences prefs;

    IPAddress apIP = IPAddress(192, 168, 4, 1);

    inline bool apHasPassword() const { return apPass_.length() >= 8; }

    // Mật khẩu AP: ưu tiên AP_PASSWORD trong secrets.h, nếu không thì sinh ngẫu nhiên và lưu NVS
    String getOrCreateApPassword() {
#ifdef AP_PASSWORD
        if (strlen(AP_PASSWORD) >= 8) return String(AP_PASSWORD);
#endif
        Preferences p;
        p.begin("sys_config", false);
        String pw = p.getString("ap_pass", "");
        if (pw.length() < 8) {
            pw = randomToken(10);
            p.putString("ap_pass", pw);
        }
        p.end();
        return pw;
    }

    // 🌟 Đẩy HTML/CSS/JS vào bộ nhớ Flash (PROGMEM) để chống phân mảnh RAM
    const char* PAGE_META = R"rawliteral(<meta name='viewport' content='width=device-width, initial-scale=1'>)rawliteral";

    const char* PAGE_STYLE = R"rawliteral(
        <style>
        body{font-family:Arial;background:#f7f7f7;margin:0;padding:20px 10px;}
        h2{color:#333;text-align:center;margin-top:0;}
        .container{max-width:480px;margin:auto;padding:0;}
        .card{background:#fff;padding:15px;margin:10px 0;border-radius:10px;box-shadow:0 2px 5px rgba(0,0,0,0.1);}
        .button{width:100%;padding:12px;background:#007bff;color:white;border:none;border-radius:5px;cursor:pointer;margin-top:5px;font-size:1.1em;font-weight:bold;}
        .button:hover{background:#0056b3;}
        .button:disabled{background:#ccc;cursor:not-allowed;}
        .delete-btn{background:#dc3545;color:white;border:none;border-radius:5px;padding:6px 12px;cursor:pointer;}
        input[type='text'],input[type='password'],input[type='number']{width:100%;padding:10px;margin:5px 0 10px 0;border:1px solid #ccc;border-radius:5px;box-sizing:border-box;}
        .wifi-list div.wifi-item{padding:10px 12px;cursor:pointer;background-color:#f9f9f9;display:flex;justify-content:space-between;align-items:center;border-bottom:1px solid #eee;}
        .wifi-list div.wifi-item:hover{background-color:#e6e6e6;}
        .signal{color:#555;font-size:0.9em;}
        .ok{color:#28a745;font-weight:bold;}
        .warn{color:#ffc107;font-weight:bold;}
        .tab-container{overflow:hidden;border:1px solid #ccc;background-color:#f1f1f1;border-radius:10px 10px 0 0;}
        .tab-button{background-color:inherit;float:left;border:none;outline:none;cursor:pointer;padding:14px 16px;transition:0.3s;font-size:1.1em;font-weight:bold;}
        .tab-button:hover{background-color:#ddd;}
        .tab-button.active{background-color:#fff;border-bottom:none;}
        .tab-content{display:none;padding:15px;border:1px solid #ccc;border-top:none;background-color:#fff;border-radius:0 0 10px 10px;min-height:100px;}
        .checkbox-group{margin:15px 0;display:flex;align-items:center;}
        .checkbox-group label{margin-left:8px;font-weight:normal;}
        </style>
    )rawliteral";
    
    const char* PAGE_SCRIPT = R"rawliteral(
        <script>
        function selectSSID(ssid){
            document.querySelector('input[name="ssid"]').value = ssid;
            document.querySelector('input[name="password"]').focus();
        }
        function openTab(evt, tabName) {
            var i, tabcontent, tablinks;
            tabcontent = document.getElementsByClassName('tab-content');
            for (i = 0; i < tabcontent.length; i++) {tabcontent[i].style.display = 'none';}
            tablinks = document.getElementsByClassName('tab-button');
            for (i = 0; i < tablinks.length; i++) {tablinks[i].className = tablinks[i].className.replace(' active', '');}
            document.getElementById(tabName).style.display = 'block';
            evt.currentTarget.className += ' active';
        }
        </script>
    )rawliteral";

    bool tryConnectFromNVS(uint32_t timeoutMs) {
        prefs.begin("wifi", true);
        String ssid = prefs.getString("ssid", "");
        String pass = prefs.getString("password", "");
        prefs.end();

        if (ssid.isEmpty()) return false;

        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid.c_str(), pass.c_str());

        Serial.printf("🔄 Kết nối tới WiFi đã lưu: %s ...\n", ssid.c_str());
        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
            delay(500);
            Serial.print(".");
        }
        Serial.println();
        return WiFi.status() == WL_CONNECTED;
    }

    void startConfigAP() {
        Serial.println("🚀 Bắt đầu chế độ AP...");
        WiFi.disconnect(); 
        delay(100);
        WiFi.mode(WIFI_AP_STA); 
        WiFi.softAP(apSsid_.c_str(), apHasPassword() ? apPass_.c_str() : nullptr);
        WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
        delay(100);
        Serial.printf("📡 AP đang mở: %s | IP: %s\n", apSsid_.c_str(), WiFi.softAPIP().toString().c_str());
        Serial.printf("🔑 Mật khẩu AP: %s\n", apPass_.c_str());
        {   // Hiện mật khẩu AP trên LCD (5 phút) để người dùng kết nối cấu hình
            String l2 = "PW:" + apPass_;
            lcdShowMessage("WiFi AP Setup", l2.c_str(), 300000UL);
        }
        dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
        server.begin();
        isAPMode_ = true;
    }

    void handleRoot() {
        prefs.begin("wifi", true);
        String savedSSID = prefs.getString("ssid", "");
        prefs.end();

        Preferences sysPrefs;
        sysPrefs.begin("sys_config", true);
        String currentDeviceName = sysPrefs.getString("device_name", "Hệ thống kiểm soát chất lượng nước ESP32");
        float currentPhThresh = sysPrefs.getFloat("ph_thresh", 6.0);
        sysPrefs.end();

        // 🌟 TỐI ƯU: Gửi chunked response thay vì dồn vào 1 String khổng lồ gây tràn RAM
        server.setContentLength(CONTENT_LENGTH_UNKNOWN);
        server.send(200, "text/html", "");

        server.sendContent(F("<!DOCTYPE html><html><head><meta charset='utf-8'><title>Cấu hình WiFi & Hệ thống</title>"));
        server.sendContent(PAGE_META); 
        server.sendContent(PAGE_STYLE);
        server.sendContent(PAGE_SCRIPT); 
        server.sendContent(F("</head><body><div class='container'>"));
        server.sendContent(F("<h2>Cấu hình Hệ Thống ESP32</h2>"));

        String chunk = "<div class='card'><h3>Trạng thái kết nối</h3>";
        if (WiFi.status() == WL_CONNECTED) {
            chunk += "<p class='ok'>✅ Đã kết nối: <b>" + htmlEscape(WiFi.SSID()) + "</b></p>";
            chunk += "<p>IP: " + WiFi.localIP().toString() + "</p>";
        } else {
            chunk += "<p class='warn'>⚠️ Chưa kết nối mạng Wi-Fi.</p>";
        }

        if (savedSSID.length() > 0) {
            chunk += "<p>Mạng đã lưu: <b>" + htmlEscape(savedSSID) + "</b></p>";
            chunk += "<form action='/delete' method='POST' style='display:inline-block'>";
            chunk += "<button class='delete-btn' type='submit'>❌ Xóa mạng này</button>";
            chunk += "</form>";
        }
        chunk += "</div>";
        server.sendContent(chunk);

        chunk = "<div class='card'><div class='tab-container'>";
        chunk += "<button class='tab-button active' onclick=\"openTab(event, 'tabConnect')\">Cài đặt</button>";
        chunk += "</div>";
        
        chunk += "<div id='tabConnect' class='tab-content' style='display:block;'>";
        chunk += "<form action='/save' method='POST' id='wifiForm'>";
        
        chunk += "<h3>Thông số hệ thống</h3>";
        chunk += "<label>Tên thiết bị:</label><br><input name='deviceName' type='text' value='" + htmlEscape(currentDeviceName) + "' maxlength='32' required><br>";
        chunk += "<label>Ngưỡng pH (Tự động bật bơm khi pH cao hơn ngưỡng):</label><br><input name='phThreshold' type='number' step='0.1' min='0' max='14' value='" + String(currentPhThresh) + "' required><br>";
        chunk += "<hr>";
        
        chunk += "<h3>Kết nối WiFi</h3>";
        chunk += "<label>Tên WiFi (SSID):</label><br><input name='ssid' id='ssid_input' type='text' required><br>";
        chunk += "<label>Mật khẩu WiFi:</label><br><input type='password' name='password'><br>";
        
        chunk += "<div class='checkbox-group'>";
        chunk += "<input type='checkbox' id='save_nvs' name='save_nvs' value='true' checked>";
        chunk += "<label for='save_nvs'>Lưu thông tin vào bộ nhớ</label>";
        chunk += "</div>";
        chunk += "<button class='button' type='submit'>Lưu Cài Đặt & Khởi Động Lại</button>";
        chunk += "</form></div></div>";
        server.sendContent(chunk);

        chunk = "<div class='card'><h3>Danh sách WiFi xung quanh</h3>";
        chunk += "<button class='button' onclick=\"location.href='/rescan'\">🔄 Quét lại</button><br><br>";
        chunk += "<div class='wifi-list'>";
        server.sendContent(chunk);

        if (WiFiPortalData::wifiList.empty()) {
            server.sendContent(F("<p>Chưa quét được mạng WiFi nào.</p>"));
        } else {
            for (auto &net : WiFiPortalData::wifiList) {
                String lock = net.secure ? "🔒" : "";
                String item = "<div class='wifi-item' data-ssid='" + htmlEscape(net.ssid) + "' onclick='selectSSID(this.dataset.ssid)'>"; 
                item += "<span>" + htmlEscape(net.ssid) + " " + lock + "</span>";
                item += "<span class='signal'>" + String(net.rssi) + " dBm</span></div>";
                server.sendContent(item);
            }
        }

        server.sendContent(F("</div></div></div></body></html>"));
        server.sendContent(""); // Kết thúc chunk
    }

    void handleRescan() {
        WiFiPortalData::wifiList.clear();
        int n = WiFi.scanNetworks();
        for (int i = 0; i < n; ++i) {
            WiFiNetwork net;
            net.ssid = WiFi.SSID(i);
            net.rssi = WiFi.RSSI(i);
            net.secure = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
            WiFiPortalData::wifiList.push_back(net);
        }
        handleRoot();
    }

    void handleSave() {
        String ssid = server.arg("ssid");
        String password = server.arg("password");
        String deviceName = server.arg("deviceName");
        String phThreshold = server.arg("phThreshold");

        // --- Kiểm tra đầu vào ---
        float phVal = phThreshold.toFloat();
        bool phOk = phThreshold.length() > 0 && isfinite(phVal) && phVal >= 0.0f && phVal <= 14.0f;
        bool wifiOk = ssid.length() > 0 && ssid.length() <= 32 && password.length() <= 63;
        if (deviceName.length() > 32) deviceName = deviceName.substring(0, 32);

        if (!phOk || !wifiOk) {
            server.send(400, "text/html; charset=utf-8",
                "<!DOCTYPE html><html><head><meta charset='utf-8'></head><body>"
                "<h3>⚠️ Dữ liệu không hợp lệ</h3><p>pH phải trong khoảng 0-14, SSID 1-32 ký tự, mật khẩu WiFi tối đa 63 ký tự.</p>"
                "<a href='/'>Quay lại</a></body></html>");
            return;
        }
        
        Preferences sysPrefs;
        sysPrefs.begin("sys_config", false);
        if(deviceName.length() > 0) sysPrefs.putString("device_name", deviceName);
        sysPrefs.putFloat("ph_thresh", phVal);
        sysPrefs.end();

        if (server.hasArg("save_nvs")) {
            prefs.begin("wifi", false); 
            prefs.putString("ssid", ssid);
            prefs.putString("password", password);
            prefs.end();                
        }

        String page = "<!DOCTYPE html><html><head><meta charset='utf-8'><title>Đang Kết Nối</title>";
        page += PAGE_META; 
        page += PAGE_STYLE;
        page += "</head><body><div class='container card'>";
        page += "<h2>✅ Đã Lưu Cấu Hình</h2>";
        page += "<p>Tên thiết bị: <b>" + htmlEscape(deviceName) + "</b></p>";
        page += "<p>Ngưỡng pH: <b>" + String(phVal, 1) + "</b></p>";
        page += "<p>Hệ thống đang khởi động lại và kết nối tới mạng WiFi nhà bạn.</p>";
        page += "<p id='msg'>Vui lòng đóng trang này.</p>";
        page += "</div></body></html>";
        
        server.send(200, "text/html; charset=utf-8", page);
        
        delay(500); 
        WiFi.softAPdisconnect(true);
        isAPMode_ = false;
        Serial.println("✅ Đã lưu cấu hình. Đang khởi động lại mạch...");
        delay(2000); 
        ESP.restart(); 
    }

    void handleSwitch() {
        prefs.begin("wifi", false);
        prefs.remove("ssid");
        prefs.remove("password");
        prefs.end();
        WiFi.disconnect(true, false);
        delay(100);
        server.sendHeader("Location", "/");
        server.send(303);
    }

public:
    void begin(const char* apSsid = nullptr, const char* apPass = nullptr, uint32_t connectTimeoutMs = 30000) {
        apSsid_ = (apSsid && strlen(apSsid) > 0) ? apSsid : AP_SSID;
        apPass_ = (apPass && strlen(apPass) >= 8) ? String(apPass) : getOrCreateApPassword();
        connectTimeoutMs_ = connectTimeoutMs;

        // Bật tính năng tự động kết nối lại ngầm của ESP32
        WiFi.setAutoReconnect(true);

        server.on("/", [this]() { handleRoot(); });
        server.on("/save", HTTP_POST, [this]() { handleSave(); }); 
        server.on("/rescan", [this]() { handleRescan(); });
        server.on("/delete", HTTP_POST, [this]() { handleSwitch(); }); 

        server.onNotFound([this]() {
            server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
            server.send(302, "text/plain", "");
        });
        
        if (!tryConnectFromNVS(connectTimeoutMs)) {
            Serial.println("⚠ Không kết nối được WiFi đã lưu. Mở AP cấu hình...");
            startConfigAP();
        } else {
            Serial.println("✅ Đã kết nối WiFi thành công!");
            Serial.print("📶 IP: ");
            Serial.println(WiFi.localIP());
            WiFi.softAPdisconnect(true);
            WiFi.mode(WIFI_STA);
            isAPMode_ = false;
        }
    }

    void loop() {
        if (isAPMode_) {
            server.handleClient();
            dnsServer.processNextRequest();
        }
    }


    bool isInAPMode() { return isAPMode_; }
};

#endif // WIFIPORTAL_H