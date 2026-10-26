# Changelog

## 1.8.6 — Bản vá bảo mật & độ tin cậy

> ✅ Đã nạp và chạy thử trên mạch ESP32 thật (biên dịch với ESP32 Arduino core 3.3.3).
> ⚠️ Vẫn nên thử với tải giả (đèn LED thay bơm) trước khi nối bơm thật.
> Đấu nối đúng chân quy định trong `SharedConfig.h` (lưu ý chân 5 và chân 2 là strapping pin).

### Bảo mật
- **MQTT không còn gửi `ThingSpeak key` / `MCP token`** lên broker (trước đây gửi dạng retained lên broker công khai).
- MQTT: hỗ trợ user/pass, xác thực chứng chỉ TLS (`MQTT_CA_CERT`); mã thiết bị mặc định lấy từ MAC thay vì chuỗi cố định.
- MQTT: **điều khiển bơm từ xa mặc định TẮT** (`MQTT_REMOTE_CONTROL`), lệnh được giới hạn độ dài.
- `/api/ai/chat` **bắt buộc đăng nhập** (trước đây ai cùng mạng cũng bật/tắt được bơm), giới hạn độ dài tin nhắn, timeout 8 giây (trước 40 giây chặn vòng điều khiển).
- OTA **chỉ bật khi có `OTA_PASSWORD`** (≥ 8 ký tự).
- Bỏ mật khẩu mặc định: web (`admin/123456`) và AP (`12345678`) nay được **sinh ngẫu nhiên** ở lần chạy đầu; mật khẩu mới tối thiểu 8 ký tự; không in mật khẩu ra Serial khi đổi (chỉ hiện một lần ở lần chạy đầu để người dùng ghi lại).
- Trang cấu hình WiFi: chống XSS (escape SSID, tên thiết bị), kiểm tra đầu vào.
- Giao diện web: tin nhắn chat hiển thị dạng text (không `innerHTML`); thêm header `X-Frame-Options`, `X-Content-Type-Options`, `Referrer-Policy`.
- Serial không còn in ThingSpeak key.
- `OTA_PASSWORD` có giá trị mặc định rỗng trong `SharedConfig.h` (OTA tắt) — sửa lỗi không biên dịch được khi không có `secrets.h`.
- `secrets.example.h` bổ sung `OTA_PASSWORD`, `AP_PASSWORD` và hướng dẫn nhập ThingSpeak key / MCP token qua Serial (`SETKEY`, `SETMCP`).
- Giao diện web: ghim `chart.js@4.4.1` thay vì tải bản mới nhất.
- Địa chỉ máy chủ AI, user id, thông tin cá nhân trong chuỗi chat đưa ra ngoài code (`secrets.h`).

### An toàn bơm / thiết bị
- Auto: **cảm biến pH lỗi → tắt bơm** (trước đây giữ nguyên trạng thái, có thể chạy mãi).
- Auto: giới hạn chạy liên tục 30 phút, nghỉ 10 phút (`PUMP_MAX_RUN_MS`, `PUMP_COOLDOWN_MS` trong `SharedConfig.h`).
- `/api/control/relay`: thiếu `relay`/`status` hoặc sai chỉ số → trả 400 (trước đây mặc định điều khiển bơm).
- `/api/settings/ph_threshold`, trang cấu hình WiFi: kiểm tra ngưỡng pH hợp lệ (trước đây thiếu trường → ngưỡng 0 → bơm chạy liên tục).

### Lỗi logic
- Cảnh báo pH kiềm hiển thị đúng "KIỀM" (trước đây hiển thị "AXIT").
- Lưu/khôi phục trạng thái relay hoạt động (trước đây chỉ đọc, không bao giờ ghi); khôi phục thực hiện trong `setup()` thay vì constructor toàn cục.
- Bộ đếm spike pH không còn dùng chung giữa 2 bộ AI; MQTT dùng chung kết quả AI với web.
- Cập nhật ngưỡng nhiệt MIN/MAX cùng lúc (`setTempRange`) — trước đây cập nhật lần lượt có thể tự đảo sai giá trị.
- API cấu hình hệ thống báo lỗi khi giá trị không hợp lệ (trước đây luôn báo thành công); giao diện hiện cảnh báo khi server từ chối.
- ThingSpeak: không gửi khi chưa có API key; không gửi pH lỗi.
- Nhãn trang WiFi: "bật bơm khi pH **cao hơn** ngưỡng" (khớp với logic thực tế).
- MQTT: thay lambda có `[this]` bằng hàm callback tĩnh (`callbackThunk`) tương thích PubSubClient; chỉ hỗ trợ một đối tượng `MQTTManager`.
- Dọn code chết, thống nhất số phiên bản.

## 1.8.3
Bản gốc.
