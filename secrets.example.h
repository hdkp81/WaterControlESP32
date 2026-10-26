
#pragma once

// =====================================================================
// secrets.example.h -> Copy thành secrets.h rồi chỉnh cấu hình.
// secrets.h phải nằm trong .gitignore và KHÔNG commit lên GitHub.
// Bỏ qua các cấu hình tùy chọn thì sử dụng giá trị mặc định an toàn.
// =====================================================================

// ================= MQTT CONFIGURATION ================================
// MQTT mặc định TẮT khi MQTT_HOST rỗng.
// Chỉ cấu hình khi broker hỗ trợ MQTT qua TLS và có CA certificate hợp lệ.

// Địa chỉ broker và cổng TLS.
// Thay bằng hostname thật của broker; không thêm mqtt:// hoặc https://.
// #define MQTT_HOST "mqtt.example.com"
// #define MQTT_PORT 8883

// Tài khoản riêng cho thiết bị ESP32.
// Nếu khai báo một giá trị thì phải khai báo cả hai.
// Dùng mật khẩu mạnh, không tái sử dụng mật khẩu quản trị.
// #define MQTT_USER "esp32_device"
// #define MQTT_PASS "CHANGE_TO_A_STRONG_PASSWORD"

// ID thiết bị tùy chọn.
// Đây chỉ là định danh, KHÔNG thay thế xác thực.
// Nếu không khai báo, SharedConfig.h sẽ tạo ID từ MAC.
// #define MQTT_DEVICE_ID "aquarium01"

// Điều khiển từ xa mặc định TẮT.
// Chỉ đổi thành 1 sau khi xác thực và ACL trên broker đã được
// cấu hình, kiểm thử và giới hạn quyền theo topic.
// #define MQTT_REMOTE_CONTROL 0

// CA certificate dùng xác minh TLS của broker.
// Phải thay nội dung ví dụ bằng CA certificate đúng của broker.
// Không dùng setInsecure() hoặc chứng chỉ placeholder.
// Giữ nguyên ký tự xuống dòng \n trong chuỗi.
//
// #define MQTT_CA_CERT \
// "-----BEGIN CERTIFICATE-----\n" \
// "REPLACE_WITH_REAL_CA_CERTIFICATE_CONTENT\n" \
// "-----END CERTIFICATE-----\n"


// ================= AI / FASTAPI BACKEND ===============================
// SharedConfig.h đã đặt mặc định:
// AI_SERVER_URL = ""
// AI_USER_ID = "web_user"
//
// Điền URL nếu ESP32 cần gọi backend FastAPI.
// Dùng địa chỉ IP hoặc hostname có thể truy cập từ ESP32.
// Ví dụ HTTP chỉ phù hợp mạng tin cậy trong quá trình thử nghiệm.
// Khi triển khai qua Internet, nên dùng HTTPS và xác thực phù hợp.
//
// #define AI_SERVER_URL "http://192.168.1.100:8000"
// #define AI_USER_ID "web_user"


// ================= OTA (CẬP NHẬT FIRMWARE QUA WIFI) ===================
// OTA mặc định TẮT. Chỉ bật khi đặt mật khẩu từ 8 ký tự trở lên.
// Dùng mật khẩu mạnh, khác mật khẩu web và mật khẩu WiFi.
// #define OTA_PASSWORD "CHANGE_TO_A_STRONG_PASSWORD"


// ================= WIFI CONFIG PORTAL (ACCESS POINT) ==================
// Mặc định: mật khẩu AP được sinh ngẫu nhiên ở lần chạy đầu,
// lưu trong NVS, hiển thị trên LCD và Serial.
// Chỉ khai báo nếu muốn đặt cố định (tối thiểu 8 ký tự).
// #define AP_PASSWORD "CHANGE_TO_A_STRONG_PASSWORD"


// ================= THINGSPEAK / MCP TOKEN =============================
// Khóa ThingSpeak và MCP token KHÔNG đặt trong file này.
// Nhập qua Serial Monitor: SETKEY <ThingSpeakKey> và SETMCP <MCP_Token>
// (được lưu trong NVS của thiết bị, không nằm trong mã nguồn).


// ================= IMPORTANT SECURITY NOTES ===========================
// 1. Không commit secrets.h lên GitHub.
// 2. Không đưa API key, token hoặc mật khẩu vào topic MQTT.
// 3. Cấu hình ACL trên broker để giới hạn quyền publish/subscribe.
// 4. MQTT_REMOTE_CONTROL = 1 phải dùng tài khoản được xác thực.
// 5. Chỉ bật MQTT khi MQTT_CA_CERT là CA certificate thực tế hợp lệ.
// 6. Không dùng ID thiết bị hoặc tên topic làm cơ chế xác thực.
// 7. Không đưa mật khẩu OTA/AP, khóa ThingSpeak hay MCP token vào mã nguồn công khai.
// =====================================================================
