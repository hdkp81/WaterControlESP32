#ifndef WEBSERVERMANAGER_H
#define WEBSERVERMANAGER_H

#include <WebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <HTTPClient.h> // THÊM THƯ VIỆN NÀY ĐỂ GỌI API PYTHON
#include "SharedConfig.h" 
#include "WiFiPortal.h"   
#include "WaterQualityMonitorLib.h"
#include "ConfigManager.h" 

// Khai báo biến ngoại vi
extern float pH_value;
extern bool system_mode;
extern float pH_threshold;
extern WaterQualityMonitor monitor; 
extern ConfigManager config; 
extern SensorManager sensorManager;
extern String admin_user;
extern String admin_pass;

// Địa chỉ máy chủ AI (FastAPI) lấy từ AI_SERVER_URL trong secrets.h (rỗng = tắt tính năng AI)

// KẾT NỐI VỚI BIẾN KẾT QUẢ AI TOÀN CỤC (Được tính toán ngầm ở file .ino)
extern AIResult currentAIResult;

// Lưu trữ toàn bộ mã HTML vào bộ nhớ Flash (PROGMEM) để tránh phân mảnh RAM
const char MAIN_DASHBOARD_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="vi">
<head>
    <title>HDKP AI SMART AQUA</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <meta charset="UTF-8">
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.4.0/css/all.min.css">
    <script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.1/dist/chart.umd.js" crossorigin="anonymous"></script>
    <style>
        :root {
            --primary: #6c5ce7; --success: #00d285; --danger: #ff4757; --warning: #ffa502;
            --dark-bg: #1e2124; --card-bg: rgba(255, 255, 255, 0.9);
        }
        body { 
            font-family: 'Inter', 'Segoe UI', system-ui, sans-serif; 
            margin: 0; padding: 10px; 
            background: linear-gradient(135deg, #f5f7fa 0%, #c3cfe2 100%); 
            min-height: 100vh; color: #2d3436;
        }
        .container { max-width: 1200px; margin: 0 auto; }
        
        .header { 
            background: rgba(255, 255, 255, 0.85); backdrop-filter: blur(15px);
            padding: 12px 25px; border-radius: 20px; margin-bottom: 20px;
            display: flex; justify-content: space-between; align-items: center;
            position: sticky; top: 10px; z-index: 1000;
            box-shadow: 0 10px 30px rgba(31, 38, 135, 0.15); border: 1px solid rgba(255, 255, 255, 0.4);
        }

        .dropdown { position: relative; }
        .dropbtn { cursor: pointer; padding: 8px 15px; border-radius: 12px; transition: 0.3s; background: rgba(255,255,255,0.5); font-weight: bold; }
        .dropdown-content {
            display: none; position: absolute; right: 0; top: 120%; background-color: #fff;
            min-width: 240px; box-shadow: 0 15px 35px rgba(0,0,0,0.2);
            border-radius: 15px; z-index: 9999; padding: 10px; border: 1px solid #eee;
        }
        .dropdown-content a, .menu-info, .menu-lang {
            color: #333; padding: 10px; text-decoration: none; display: flex; align-items: center; gap: 10px;
            font-size: 0.9rem; border-radius: 8px; transition: 0.2s;
        }
        .dropdown-content a:hover { background-color: #f8f9fa; }
        .show { display: block !important; animation: fadeIn 0.3s ease; }

        .status-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(280px, 1fr)); gap: 15px; margin-bottom: 20px; }
        .card { 
            background: var(--card-bg); padding: 20px; border-radius: 20px; 
            box-shadow: 0 10px 25px rgba(0,0,0,0.05); border: 1px solid #fff;
            transition: all 0.3s ease; position: relative;
        }
        .card:hover { transform: translateY(-3px); box-shadow: 0 15px 35px rgba(0,0,0,0.1); }
        .val-display { font-size: 3rem; font-weight: 800; margin: 5px 0; letter-spacing: -1px; }
        .ph-color { color: var(--success); }
        .temp-color { color: var(--warning); }
        .tds-color { color: #00d2d3; }

        .gauge-bar { width: 100%; height: 10px; background: #eaedf0; border-radius: 10px; margin: 12px 0; overflow: hidden; }
        .gauge-fill { height: 100%; border-radius: 10px; transition: width 1s cubic-bezier(0.175, 0.885, 0.32, 1.275); }

        .btn { 
            padding: 10px 18px; border: none; border-radius: 10px; cursor: pointer; 
            font-weight: 600; transition: 0.3s; display: inline-flex; align-items: center; justify-content: center; gap: 8px;
        }
        .btn:active { transform: scale(0.96); }
        .btn-primary { background: var(--primary); color: white; }
        .btn-danger { background: var(--danger); color: white; }
        .btn-success { background: var(--success); color: white; }
        .btn-secondary { background: #6c757d; color: white; }

        .chart-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(280px, 1fr)); gap: 15px; margin-bottom: 20px; }
        .chart-wrapper { height: 180px; width: 100%; }

        /* KCS FIX: CSS tối ưu cho Bảng cài đặt đáp ứng mọi màn hình */
        .form-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(220px, 1fr)); gap: 15px; }
        .table-responsive { width: 100%; overflow-x: auto; margin-top: 10px; }
        .temp-table { width: 100%; border-collapse: collapse; font-size: 0.85rem; }
        .temp-table th, .temp-table td { border: 1px solid #edf2f7; padding: 8px 4px; text-align: center; }
        .temp-table input { width: 100%; padding: 8px 2px; text-align: center; border-radius: 6px; border: 1px solid #edf2f7; box-sizing: border-box; font-family: inherit; }
        
        .input { width: 100%; padding: 10px; border-radius: 8px; border: 2px solid #edf2f7; box-sizing: border-box; transition: 0.3s; font-family: inherit; }
        input:focus { border-color: var(--primary); outline: none; }
        
        .no-copy { user-select: none; -webkit-user-select: none; }
        .secure-blur { filter: blur(4px); opacity: 0.7; transition: all 0.3s ease; letter-spacing: 2px; font-family: monospace; }
        .secure-blur.show-text { filter: blur(0); opacity: 1; letter-spacing: normal; }
        .secure-blur::selection { background: transparent; color: transparent; }
        .secure-blur::-moz-selection { background: transparent; color: transparent; }

        .relay-ctrl { display: flex; align-items: center; justify-content: space-between; padding: 12px; background: #f8f9fa; border-radius: 15px; margin-bottom: 10px; }
        .dot { width: 10px; height: 10px; border-radius: 50%; display: inline-block; background: #dfe6e9; margin-right: 5px; }
        .dot-on { background: var(--success); box-shadow: 0 0 10px var(--success); animation: pulse 2s infinite; }

        .disabled-area { opacity: 0.4; pointer-events: none; filter: grayscale(100%); transition: all 0.3s ease; }

        /* Tối ưu hóa Modal cho thiết bị di động và máy tính */
        .modal { display: none; position: fixed; z-index: 5000; left: 0; top: 0; width: 100%; height: 100%; background: rgba(0,0,0,0.5); backdrop-filter: blur(4px); }
        .modal-content { 
            background: white; margin: 5vh auto; padding: 20px; border-radius: 20px; 
            width: 92%; max-width: 600px; /* Đủ rộng cho PC, co lại 92% cho phone */
            max-height: 90vh; overflow-y: auto; 
            animation: slideDown 0.3s; position: relative; box-sizing: border-box;
        }
        
        .close-btn { position: absolute; top: 15px; right: 20px; color: #aaa; font-size: 28px; font-weight: bold; cursor: pointer; line-height: 1; transition: 0.2s; z-index: 10; }
        .close-btn:hover { color: #ff4757; transform: scale(1.1); }

        @keyframes fadeIn { from { opacity: 0; } to { opacity: 1; } }
        @keyframes slideDown { from { transform: translateY(-30px); opacity: 0; } to { transform: translateY(0); opacity: 1; } }
        @keyframes pulse { 0% { opacity: 1; } 50% { opacity: 0.4; } 100% { opacity: 1; } }
        
        @media (max-width: 600px) { 
            .chart-grid { grid-template-columns: 1fr; } 
            .status-grid { grid-template-columns: 1fr; } 
        }
        
        /* =================== CHATBOX CSS =================== */
        #chat-launcher {
            position: fixed; bottom: 30px; right: 30px; width: 60px; height: 60px;
            background: var(--primary); color: white; border-radius: 50%;
            display: flex; align-items: center; justify-content: center;
            font-size: 24px; cursor: pointer; box-shadow: 0 10px 25px rgba(108, 92, 231, 0.4);
            z-index: 9999; transition: 0.3s;
        }
        #chat-launcher:hover { transform: scale(1.1) rotate(15deg); }

        #chat-window {
            position: fixed; bottom: 100px; right: 30px; width: 350px; max-width: 90vw;
            background: white; border-radius: 20px; box-shadow: 0 15px 50px rgba(0,0,0,0.2);
            display: none; flex-direction: column; overflow: hidden; z-index: 9998;
            border: 1px solid rgba(108, 92, 231, 0.2); animation: slideUp 0.4s ease;
        }
        .chat-header { background: var(--primary); color: white; padding: 15px; font-weight: bold; display: flex; justify-content: space-between; }
        #messages { height: 300px; overflow-y: auto; padding: 15px; background: #f8f9fa; display: flex; flex-direction: column; gap: 10px; }
        .msg { max-width: 80%; padding: 10px 15px; border-radius: 15px; font-size: 0.9rem; line-height: 1.4; word-wrap: break-word; }
        .msg.user { align-self: flex-end; background: var(--primary); color: white; border-bottom-right-radius: 2px; }
        .msg.ai { align-self: flex-start; background: white; color: #333; border-bottom-left-radius: 2px; box-shadow: 0 2px 5px rgba(0,0,0,0.05); }
        .chat-input-area { padding: 10px; background: white; display: flex; gap: 8px; border-top: 1px solid #eee; }
        #chat-input { flex: 1; border: 1px solid #eee; padding: 10px; border-radius: 10px; outline: none; }
        
        @keyframes slideUp { from { transform: translateY(20px); opacity: 0; } to { transform: translateY(0); opacity: 1; } }
        
        .btn-auto { background: var(--success) !important; color: white !important; box-shadow: 0 4px 15px rgba(0, 210, 133, 0.4); border: 1px solid var(--success); }
        .btn-manual { background: var(--warning) !important; color: white !important; box-shadow: 0 4px 15px rgba(255, 165, 2, 0.4); border: 1px solid var(--warning); }
        .btn-off { background: #f1f5f9 !important; color: #94a3b8 !important; border: 1px solid #cbd5e1; box-shadow: none; }
        
        /* ================= HIỆU ỨNG RELAY ================= */
        .relay-ctrl {
            display: flex; align-items: center; justify-content: space-between;
            padding: 15px; background: #fff; border-radius: 16px; margin-bottom: 12px;
            box-shadow: 0 4px 15px rgba(0,0,0,0.03); transition: all 0.4s cubic-bezier(0.175, 0.885, 0.32, 1.275);
            border: 2px solid transparent;
        }
        .relay-ctrl:hover { transform: translateY(-2px); box-shadow: 0 8px 25px rgba(0,0,0,0.08); }
        .relay-ctrl.active { border-color: var(--success); background: #f0fdf4; box-shadow: 0 5px 20px rgba(0, 210, 133, 0.2); }
        
        .relay-info { display: flex; align-items: center; gap: 15px; font-weight: 600; color: #2d3436; font-size: 1rem;}
        
        .relay-icon {
            width: 42px; height: 42px; border-radius: 12px; background: #f1f5f9;
            display: flex; align-items: center; justify-content: center;
            font-size: 1.2rem; transition: all 0.4s ease;
        }
        
        .c-pump { color: #0984e3; }
        .c-light { color: #f39c12; }
        .c-fan { color: #00b894; }
        
        .relay-ctrl.active .relay-icon { 
            background: var(--success);
            animation: pulseRelay 2s infinite; 
        }
        .relay-ctrl.active .relay-icon i { color: white !important; }

        .switch { position: relative; display: inline-block; width: 52px; height: 30px; margin: 0; }
        .switch input { opacity: 0; width: 0; height: 0; }
        .slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #cbd5e1; transition: .4s; border-radius: 34px; }
        .slider:before { position: absolute; content: ""; height: 22px; width: 22px; left: 4px; bottom: 4px; background-color: white; transition: .4s cubic-bezier(0.175, 0.885, 0.32, 1.275); border-radius: 50%; box-shadow: 0 2px 5px rgba(0,0,0,0.2); }
        
        input:checked + .slider { background-color: var(--success); }
        input:checked + .slider:before { transform: translateX(22px); }

        @keyframes pulseRelay { 
            0% { box-shadow: 0 0 0 0 rgba(0, 210, 133, 0.5); } 
            70% { box-shadow: 0 0 0 12px rgba(0, 210, 133, 0); } 
            100% { box-shadow: 0 0 0 0 rgba(0, 210, 133, 0); } 
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1 id="txt-title" style="margin:0; font-size:1.2rem;"> Giám sát nước HDKP AI 💧🐟</h1>
            <div class="dropdown">
                <div class="dropbtn" onclick="toggleMenu()">
                    👤 <span id="txt-hello">Xin chào</span>, <strong id="currentAdminUser">...</strong> ▼
                </div>
                <div id="myDropdown" class="dropdown-content">
                    <a href="javascript:void(0)" onclick="openAccountModal()"><span>🔒</span> <span id="txt-menu-account">Đổi tài khoản</span></a>
                    <a href="javascript:void(0)" onclick="openSystemModal()"><span>⚙️</span> <span id="txt-menu-settings">Cài đặt hệ thống</span></a>
                    
                    <div class="menu-info" onclick="openAboutModal()" style="cursor: pointer;"><span>🛡️</span> <span id="txt-dev-status">Nhà phát triển xác thực</span></div>
                    <div style="padding: 10px; border-top: 1px solid #eee;">
                        <span id="txt-lang-label" style="font-size:0.8rem; color:#666">Tiếng Việt</span>
                        <div style="margin-top:5px; display:flex; gap:5px;">
                            <button class="btn btn-sm" id="btn-vi" onclick="changeLang('vi')" style="padding:4px 8px; font-size:0.7rem">VN</button>
                            <button class="btn btn-sm" id="btn-en" onclick="changeLang('en')" style="padding:4px 8px; font-size:0.7rem">EN</button>
                        </div>
                    </div>
                    <a href="javascript:void(0)" onclick="performLogout()" style="color: var(--danger); border-top: 1px solid #eee;"><span>🚪</span> <span id="txt-menu-logout">Đăng xuất</span></a>
                </div>
            </div>
        </div>

        <div class="status-grid">
            <div class="card">
                <label class="txt-ph"><i class="fa-solid fa-vial" style="color: #e056fd;"></i> CHỈ SỐ PH</label>
                <div class="val-display ph-color" id="phValue">--</div>
                <div id="phErrorAlert" style="display: none; background-color: #ff4757; color: white; text-align: center; padding: 5px; border-radius: 5px; font-weight: bold; margin-bottom: 10px; font-size: 0.85em; animation: blinker 1s linear infinite;">
                    <i class="fa-solid fa-triangle-exclamation"></i> LỖI: MẤT KẾT NỐI ĐẦU DÒ
                </div>
                <div class="gauge-bar"><div class="gauge-fill" id="phGauge" style="background:var(--success); width:0%"></div></div>
                <div style="display:flex; gap:8px; margin-top:10px;">
                    <input type="number" id="newPhThresh" step="0.1" placeholder="Ngưỡng pH" class="input">
                    <button class="btn btn-primary" id="btn-set-ph" onclick="updatePh()">Sửa</button>
                </div>
                <p><small><span id="txt-sys-thresh">Hệ thống:</span> <span id="phThreshDisp">--</span></small></p>
            </div>
            <div class="card">
                <label class="txt-temp"><i class="fa-solid fa-temperature-high" style="color: #ff7675;"></i> NHIỆT ĐỘ NƯỚC</label>
                <div class="val-display temp-color" id="tempValue">--</div>
                <div class="gauge-bar"><div class="gauge-fill" id="tempGauge" style="background:var(--warning); width:0%"></div></div>
                <div id="tempAlertS1" style="display:none; font-size: 0.85rem; font-weight: bold; margin-top: 8px; animation: pulse 1.5s infinite;">
                    <i class="fa-solid fa-triangle-exclamation"></i> <span id="txt-temp-msg-s1">Cảnh báo</span>
                </div>
                <p id="txt-s1" style="margin-top:8px; font-size:0.7rem; color:#95a5a6;">Bể cá cảnh</p>
            </div>
            <div class="card">
                <label class="txt-tds"><i class="fa-solid fa-water" style="color: #00cec9;"></i> CHỈ SỐ TDS</label>
                <div class="val-display tds-color">
                    <span id="tdsValue">--</span> 
                    <span style="font-size: 0.5em; font-weight: normal; margin-left: 2px;">ppm</span>
                </div>
                <div class="gauge-bar"><div class="gauge-fill" id="tdsGauge" style="background:#00d2d3; width:0%"></div></div>
                <strong id="tdsLevel" style="font-size:0.85rem">Đang tải...</strong>
                <p id="txt-tds1" style="margin:0; white-space: nowrap; font-size:0.7rem; color:#95a5a6;"> 
                    TDS &lt; 170: Tốt; 170 &lt; TDS &lt; 600: Trung bình; TDS &gt; 800: Xấu
                </p>
            </div>
        </div>

        <div class="chart-grid">
            <div class="card">
                <h3 class="txt-chart-ph" style="margin-top:0; font-size:1rem; color:#636e72;">🧪 Biểu đồ pH</h3>
                <div class="chart-wrapper"><canvas id="phChart"></canvas></div>
            </div>
            <div class="card">
                <h3 class="txt-chart-temp" style="margin-top:0; font-size:1rem; color:#636e72;">🌡️ Biểu đồ Nhiệt độ</h3>
                <div class="chart-wrapper"><canvas id="tempChart"></canvas></div>
            </div>
            <div class="card">
                <h3 class="txt-chart-tds" style="margin-top:0; font-size:1rem; color:#636e72;">💧 Biểu đồ TDS</h3>
                <div class="chart-wrapper"><canvas id="tdsChart"></canvas></div>
            </div>
        </div>

        <div class="status-grid" style="grid-template-columns: 1fr;">
            <div class="card" style="border-left: 6px solid var(--primary);">
                <label id="txt-ai-title"><i class="fa-solid fa-brain" style="color: #fd79a8;"></i> PHÂN TÍCH AI</label>
                <div style="display:grid; grid-template-columns: 1fr 1fr 1fr; text-align:center; margin-top:15px;">
                    <div><small class="txt-trend">XU HƯỚNG</small><br><b id="aiTrend" style="color:var(--primary)">--</b></div>
                    <div><small class="txt-risk">RỦI RO</small><br><b id="aiRisk">--</b></div>
                    <div><small id="txt-eta">DỰ KIẾN</small><br><b id="aiEta">--</b></div>
                </div>
                <p style="margin-top:15px; background:rgba(108, 92, 231, 0.08); padding:12px; border-radius:12px; font-size:0.85rem;">
                    <i class="fa-solid fa-robot" style="color: #00b894;"></i> AI: <span id="aiMsg">Đang phân tích...</span>
                </p>
                
                <div style="display: grid; grid-template-columns: repeat(auto-fit, minmax(210px, 1fr)); gap: 10px;">
                        
                    <div style="display: flex; justify-content: space-between; align-items: center; background: rgba(9, 132, 227, 0.05); padding: 10px 12px; border-radius: 12px; border: 1px solid rgba(9, 132, 227, 0.1);">
                        <div style="font-size: 0.8rem; color: #636e72; line-height: 1.4;">
                            <i class="fa-solid fa-water" style="color: #0984e3;"></i> <span id="txt-last-water-lbl">Thay nước gần nhất</span><br> 
                            <strong id="txt-last-water-change" style="color: #0984e3;">Chưa có dữ liệu</strong>
                        </div>
                        <button class="btn btn-sm" style="background: #0984e3; color: white; font-size: 0.75rem; padding: 8px 12px; white-space: nowrap; box-shadow: 0 4px 10px rgba(9, 132, 227, 0.2);" onclick="logMaintenance('water')">
                            <i class="fa-solid fa-bucket"></i> <span id="txt-btn-log-water">Ghi nhận</span> 
                        </button>
                    </div>

                    <div style="display: flex; justify-content: space-between; align-items: center; background: rgba(0, 184, 148, 0.05); padding: 10px 12px; border-radius: 12px; border: 1px solid rgba(0, 184, 148, 0.1);">
                        <div style="font-size: 0.8rem; color: #636e72; line-height: 1.4;">
                            <i class="fa-solid fa-layer-group" style="color: #00b894;"></i> <span id="txt-last-filter-lbl">Thay lọc gần nhất</span><br> 
                            <strong id="txt-last-filter-change" style="color: #00b894;">Chưa có dữ liệu</strong>
                        </div>
                        <button class="btn btn-sm" style="background: #00b894; color: white; font-size: 0.75rem; padding: 8px 12px; white-space: nowrap; box-shadow: 0 4px 10px rgba(0, 184, 148, 0.2);" onclick="logMaintenance('filter')">
                            <i class="fa-solid fa-filter"></i> <span id="txt-btn-log-filter">Ghi nhận</span> 
                        </button>
                    </div>

                    <div style="display: flex; justify-content: space-between; align-items: center; background: rgba(108, 92, 231, 0.05); padding: 10px 12px; border-radius: 12px; border: 1px solid rgba(108, 92, 231, 0.1);">
                        <div style="font-size: 0.8rem; color: #636e72; line-height: 1.4;">
                            <i class="fa-solid fa-cubes" style="color: var(--primary);"></i> <span id="txt-last-salt-lbl">Bổ sung muối</span><br> 
                            <strong id="txt-last-salt-change" style="color: var(--primary);">Chưa có dữ liệu</strong>
                        </div>
                        <button class="btn btn-sm" style="background: var(--primary); color: white; font-size: 0.75rem; padding: 8px 12px; white-space: nowrap; box-shadow: 0 4px 10px rgba(108, 92, 231, 0.2);" onclick="logMaintenance('salt')">
                            <i class="fa-solid fa-plus"></i> <span id="txt-btn-log-salt">Ghi nhận</span> 
                        </button>
                    </div>

                </div>
            </div>
        </div>

        <div class="status-grid" style="grid-template-columns: 1fr;">
            <div class="card">
                <label id="txt-sys-mode-title"><i class="fa-solid fa-microchip" style="color: #0984e3;"></i> CHẾ ĐỘ HỆ THỐNG</label>
                <div style="display:flex; align-items:center; justify-content:space-between; margin: 15px 0 20px 0;">
                    <h2 id="txt-mode-display" style="margin: 0; font-size: 1.4rem;">--</h2>
                    <div style="display:flex; gap:8px;">
                        <button class="btn btn-sm btn-auto" id="mAuto" onclick="setSysMode('auto')" style="padding: 6px 12px; font-weight: bold; border-radius: 8px;">AUTO</button>
                        <button class="btn btn-sm btn-off" id="mManual" onclick="setSysMode('manual')" style="padding: 6px 12px; font-weight: bold; border-radius: 8px;">MANUAL</button>
                    </div>
                </div>

                <hr style="border: 0; border-top: 1px dashed #dfe6e9; margin: 15px 0;">

                <label id="txt-relay-title"><i class="fa-solid fa-gamepad" style="color: #e17055;"></i> ĐIỀU KHIỂN MANUAL</label>
                <div id="manualControlArea" style="margin-top: 15px;">
                    <div class="relay-ctrl" id="card-relay-0">
                        <div class="relay-info">
                            <div class="relay-icon"><i class="fa-solid fa-water c-pump"></i></div>
                            <span id="txt-pump-label">Bơm chính</span>
                        </div>
                        <label class="switch">
                            <input type="checkbox" id="btn-relay-0" onchange="setRelay(0, this.checked)">
                            <span class="slider"></span>
                        </label>
                    </div>

                    <div class="relay-ctrl" id="card-relay-1">
                        <div class="relay-info">
                            <div class="relay-icon"><i class="fa-solid fa-lightbulb c-light"></i></div>
                            <span id="txt-light-label">Đèn chiếu sáng</span> 
                        </div>
                        <label class="switch">
                            <input type="checkbox" id="btn-relay-1" onchange="setRelay(1, this.checked)">
                            <span class="slider"></span>
                        </label>
                    </div>

                    <div class="relay-ctrl" id="card-relay-2">
                        <div class="relay-info">
                            <div class="relay-icon"><i class="fa-solid fa-fan c-fan"></i></div>
                            <span id="txt-oxy-label">Máy sục Oxy</span> 
                        </div>
                        <label class="switch">
                            <input type="checkbox" id="btn-relay-2" onchange="setRelay(2, this.checked)">
                            <span class="slider"></span>
                        </label>
                    </div>
                </div>
                <p id="autoWarningText" style="display:none; color:var(--danger); font-size:0.8rem; text-align:center; margin-top:10px;">
                    Hệ thống đang chạy TỰ ĐỘNG. Nút điều khiển bị khóa.
                </p>
                <hr>
                <div id="txt-sys-info" style="font-weight:bold; margin-bottom:10px">🔧 THÔNG TIN HỆ THỐNG</div>
                <div style="font-size:0.8rem; line-height:1.8">
                    <div><strong class="txt-device">Thiết bị:</strong> <span id="deviceId">--</span></div>
                    <div><strong id="txt-ip">IP Local:</strong> <span id="localIp">--</span></div>                    
                    <div><strong class="txt-heap">RAM Trống:</strong> <span id="freeHeap">--</span></div>
                </div>
            </div>
        </div>

        <div style="text-align:center; padding:30px; color:#95a5a6; font-size:0.7rem; letter-spacing:1px;">
            HDKP AI Smart Aqua Monitor v1.8.6 | POWERED BY DINH VUI VE © 2026
        </div>
    </div>

    <!-- Modal Đổi tài khoản -->
    <div id="accountModal" class="modal">
        <div class="modal-content" style="max-width: 400px;">
            <span class="close-btn" onclick="closeAccountModal()">&times;</span>
            <h3 id="txt-pw-title" style="color: #dc3545; margin-top: 0; margin-bottom: 15px;">🔒 ĐỔI TÀI KHOẢN</h3>
            
            <div style="display: flex; flex-direction: column; gap: 10px;">
                <label id="txt-old-pass" style="font-weight: bold; color: #d35400;">Pass cũ (Bắt buộc):</label>
                <div style="position: relative; width: 100%;">
                    <input type="password" id="sysOldPass" class="input" placeholder="Nhập mật khẩu hiện tại..." style="width: 100%; padding-right: 35px; box-sizing: border-box;">
                    <i class="fa-solid fa-eye" onclick="togglePasswordVisibility('sysOldPass', this)" style="position: absolute; right: 10px; top: 50%; transform: translateY(-50%); cursor: pointer; color: #6c757d;"></i>
                </div>
                
                <hr style="width: 100%; border: 0; border-top: 1px dashed #ccc; margin: 5px 0;">
                
                <label id="txt-new-user">User mới:</label>
                <input type="text" id="sysAdminUser" class="input" placeholder="Tên đăng nhập mới">
                
                <label id="txt-new-pass">Pass mới:</label>
                <div style="position: relative; width: 100%;">
                    <input type="password" id="sysAdminPass" class="input" placeholder="Mật khẩu mới" style="width: 100%; padding-right: 35px; box-sizing: border-box;">
                    <i class="fa-solid fa-eye" onclick="togglePasswordVisibility('sysAdminPass', this)" style="position: absolute; right: 10px; top: 50%; transform: translateY(-50%); cursor: pointer; color: #6c757d;"></i>
                </div>
                
                <button class="btn btn-danger" id="btn-update-pw" style="width:100%; margin-top:10px; margin-bottom: 10px;" onclick="changeCredentials()">CẬP NHẬT TÀI KHOẢN</button>
            </div>
        </div>
    </div>

    <!-- Modal Cài đặt hệ thống -->
    <div id="systemModal" class="modal">
        <div class="modal-content">
            <span class="close-btn" onclick="closeSystemModal()">&times;</span>
            <h3 id="txt-settings-title" style="color: var(--primary); margin-top: 0; margin-bottom: 15px;"><i class="fa-solid fa-sliders"></i> CÀI ĐẶT HỆ THỐNG</h3>
            
            <div class="form-grid" style="margin-top:10px">
                <div class="form-group">
                    <label id="txt-cfg-tds">Hệ số TDS (< 10)</label>
                    <input type="number" id="cfgTds" step="0.01" class="input" style="margin-top: 5px;">
                </div>
                <div class="form-group">
                    <label id="txt-cfg-num">Số cảm biến (1-3)</label>
                    <input type="number" id="cfgNum" min="1" max="3" class="input" style="margin-top: 5px;">
                </div>
            </div>

            <div style="margin-top:15px">
                <label id="txt-caltemp-label" style="font-weight: 600; margin-bottom: 5px; display: block;">Ngưỡng Nhiệt độ (CALTEMP)</label>
                <div class="table-responsive">
                    <table class="temp-table">
                        <tr><th id="txt-tbl-sensor" style="width:20%">Cảm Biến</th><th class="txt-min" style="width:40%">MIN (°C)</th><th class="txt-max" style="width:40%">MAX (°C)</th></tr>
                        <tr><td>S1</td><td><input type="number" id="s0min" step="0.1"></td><td><input type="number" id="s0max" step="0.1"></td></tr>
                        <tr><td>S2</td><td><input type="number" id="s1min" step="0.1"></td><td><input type="number" id="s1max" step="0.1"></td></tr>
                        <tr><td>S3</td><td><input type="number" id="s2min" step="0.1"></td><td><input type="number" id="s2max" step="0.1"></td></tr>
                    </table>
                </div>
            </div>

            <hr style="border: 0; border-top: 1px dashed #ccc; margin: 20px 0;">

            <div class="form-grid">
                <div class="form-group">
                    <label id="txt-label-ts">ThingSpeak Write Key</label>
                    <div style="position: relative; width: 100%; margin-top: 5px;">
                        <input type="password" id="sysTsKey" class="input no-copy secure-blur" placeholder="API Key..." 
                            autocomplete="off" spellcheck="false" ondragstart="return false" ondrop="return false"
                            oncopy="return false" oncut="return false" oncontextmenu="return false" 
                            onkeydown="if(event.ctrlKey && (event.key==='c' || event.key==='C' || event.key==='x' || event.key==='X')) { event.preventDefault(); return false; }"
                            style="width: 100%; padding-right: 40px; box-sizing: border-box;">
                        <i class="fa-solid fa-eye" onclick="toggleVis(this, 'sysTsKey')" style="position: absolute; right: 15px; top: 50%; transform: translateY(-50%); cursor: pointer; color: #a4b0be; transition: 0.3s; z-index: 10;"></i>
                    </div>
                </div>

                <div class="form-group">
                    <label id="txt-label-mcp">XiaoZhi MCP Token</label>
                    <div style="position: relative; width: 100%; margin-top: 5px;">
                        <input type="password" id="sysMcpToken" class="input no-copy secure-blur" placeholder="wss://token..." 
                            autocomplete="off" spellcheck="false" ondragstart="return false" ondrop="return false"
                            oncopy="return false" oncut="return false" oncontextmenu="return false" 
                            onkeydown="if(event.ctrlKey && (event.key==='c' || event.key==='C' || event.key==='x' || event.key==='X')) { event.preventDefault(); return false; }"
                            style="width: 100%; padding-right: 40px; box-sizing: border-box;">
                        <i class="fa-solid fa-eye" onclick="toggleVis(this, 'sysMcpToken')" style="position: absolute; right: 15px; top: 50%; transform: translateY(-50%); cursor: pointer; color: #a4b0be; transition: 0.3s; z-index: 10;"></i>
                    </div>
                </div>
            </div>
            
            <div style="margin-top:25px; display:flex; flex-direction:column; gap:10px;">
                <button class="btn btn-success" id="btn-save" onclick="saveCfg()">💾 LƯU CẤU HÌNH</button>
                <button class="btn btn-secondary" id="btn-reset" onclick="resetToDefaults()">🔄 KHÔI PHỤC MẶC ĐỊNH</button>
            </div>
        </div>
    </div>
    
    <div id="aboutModal" class="modal">
        <div class="modal-content" style="text-align:center">
            <span class="close-btn" onclick="closeAboutModal()">&times;</span>
            <h2 id="txt-about-title" style="color:var(--primary); margin-top: 0;">ℹ️ THÔNG TIN</h2>
            <div id="txt-about-content" style="text-align:left; font-size:0.9rem; margin-bottom:20px"></div>
            <button class="btn btn-primary" id="btn-close-modal" style="width:100%" onclick="closeAboutModal()">Đóng</button>
        </div>
    </div>

    <!-- Khu vực thông báo cập nhật (ẩn mặc định) -->
    <div id="update-banner" style="display: none; background: #fff3cd; color: #856404; padding: 12px 15px; margin-bottom: 20px; border-radius: 6px; border: 1px solid #ffeeba; display: flex; align-items: center; justify-content: space-between; font-family: sans-serif;">
        <div>
            <span id="update-text" style="font-weight: 500;">Đang kiểm tra phiên bản mới...</span>
        </div>
        <div>
            <button id="update-btn" style="padding: 6px 12px; background: #28a745; color: white; border: none; border-radius: 4px; cursor: pointer; display: none; font-weight: bold;">Xem bản mới</button>
        </div>
    </div>
    
    <div id="chat-launcher" onclick="toggleChat()"><i class="fa-solid fa-comment-dots"></i></div>
    
    <div id="chat-window">
        <div class="chat-header">
            <span id="txt-chat-title"><i class="fa-solid fa-robot" style="color: #00b894;"></i> Trợ lý HDKP AI</span>
            <i class="fa-solid fa-chevron-down" style="cursor:pointer" onclick="toggleChat()"></i>
        </div>
        <div id="messages">
            <div class="msg ai" id="txt-chat-welcome">Chào bạn! Mình là trợ lý HDKP AI. Bạn cần hỏi gì về nguồn nước không ạ?</div>
        </div>
        <div class="chat-input-area">
            <input type="text" id="chat-input" placeholder="Hỏi về pH, TDS..." onkeypress="handleKey(event)">
            <button class="btn btn-primary" style="padding: 10px;" onclick="sendChat()"><i class="fa-solid fa-paper-plane"></i></button>
        </div>
    </div>
    
    <script>
        let currentLang = 'vi';
        const translations = {
            vi: {
                title: "Giám Sát Thủy Sinh Thông Minh HDKP AI 💧🐟", hello: "Xin chào", ph: "🧪 CHỈ SỐ pH", temp: "🌡️ NHIỆT ĐỘ NƯỚC", tds: "💧 CHỈ SỐ TDS", 
                pump_mode: "CHẾ ĐỘ HỆ THỐNG", thresh_lbl: "Ngưỡng hệ thống:", set_btn: "Cập nhật", ai_title: "PHÂN TÍCH TRÍ TUỆ NHÂN TẠO", trend: "XU HƯỚNG", risk: "RỦI RO",
                relay_title: "ĐIỀU KHIỂN MANUAL", chart_ph: "Biểu đồ pH", chart_temp: "Biểu đồ Nhiệt độ (°C)", chart_tds: "Biểu đồ TDS (ppm)",
                menu_account: "Đổi tài khoản", menu_set: "Cài đặt hệ thống", menu_out: "Đăng xuất", pw_title: "ĐỔI TÀI KHOẢN", update_btn: "LƯU THAY ĐỔI",
                settings: "CÀI ĐẶT HỆ THỐNG", cfg_tds: "Hệ số TDS (< 10)", cfg_num: "Số lượng (1-3)", caltemp_label: "Ngưỡng Nhiệt độ (CALTEMP)",
                label_ts: "ThingSpeak Write Key", label_mcp: "XiaoZhi MCP Token",
                save_btn: "LƯU CẤU HÌNH", 
                sys_info: "THÔNG TIN HỆ THỐNG", device: "Thiết bị:", heap: "RAM Trống:", pump_label: "Bơm chính", about_title: "THÔNG TIN PHẦN MỀM",
                about_msg: "<strong>Giám Sát Thủy Sinh Thông Minh HDKP AI</strong><br>Phát triển bởi <b>HDKP81</b><br>Phiên bản: <b>v1.8.6  - Professional AI</b>",
                dev_status: "Nhà phát triển", lang_label: "Ngôn ngữ hiển thị", new_user: "Tài khoản mới:", new_pass: "Mật khẩu mới:",
                btn_reset: "KHÔI PHỤC MẶC ĐỊNH",
                
                last_water: "Thay nước gần nhất", last_filter: "Thay lọc gần nhất", last_salt: "Bổ sung muối gần nhất", btn_log: "Ghi nhận", no_data: "Chưa có dữ liệu", 
                light_label: "Đèn chiếu sáng", oxy_label: "Máy sục Oxy", sys_thresh: "Hệ thống:", tank_s1: "Bể cá cảnh",
                tds_guide: "TDS < 170: Tốt; 170 < TDS < 600: Trung bình; TDS > 800: Xấu", eta: "DỰ KIẾN", ip_local: "IP Local:", btn_close: "Đóng",
                chat_title: "Trợ lý HDKP AI", chat_welcome: "Chào bạn! Mình là trợ lý HDKP AI. Bạn cần hỏi gì về nguồn nước không ạ?", chat_ph: "Hỏi về pH, TDS...", tbl_sensor: "Cảm Biến",
                
                loading: "Đang tải...", analyzing: "Đang phân tích...",
                msg_ph_updated: "Đã cập nhật ngưỡng pH!", msg_fill_pw: "⚠️ Vui lòng điền đủ User và Pass!", msg_confirm_pw: "Xác nhận đổi thông tin đăng nhập?",
                msg_logout: "✅ Đã cập nhật! Hệ thống sẽ đăng xuất.", msg_confirm_reset: "❗ Khôi phục cài đặt gốc?", msg_reset_ok: "✅ Đã khôi phục!",
                msg_save_ok: "✅ Đã lưu cấu hình!", msg_confirm_log: "Xác nhận báo cáo cho AI: Anh Định vừa thực hiện", msg_log_ok: "✅ Đã nạp dữ liệu",
                msg_log_err: "❌ Lỗi mạng! AI chưa kịp ghi sổ, nhưng đã lưu trên màn hình điện thoại.", msg_ai_thinking: "AI đang suy nghĩ...",
                msg_ai_writing: "AI đang ghi vào sổ tay...", msg_ai_err: "Mất kết nối đến ESP32!", plc_ph: "Ngưỡng pH",
                temp_low: "CẢNH BÁO: Nước quá LẠNH!", temp_high: "CẢNH BÁO: Nước quá NÓNG!"
            },
            en: {
                title: "HDKP AI Smart Aqua Monitor 💧🐟", hello: "Welcome", ph: "🧪 pH VALUE", temp: "🌡️ TEMPERATURE WATER", tds: "💧 TDS INDEX", 
                pump_mode: "SYSTEM MODE", thresh_lbl: "Threshold:", set_btn: "Update", ai_title: "AI INTELLIGENCE INSIGHTS", trend: "TREND", risk: "RISK",
                relay_title: "MANUAL CONTROL", chart_ph: "pH Chart", chart_temp: "Temperature Chart (°C)", chart_tds: "TDS Chart (ppm)",
                menu_account: "Change Account", menu_set: "System Settings", menu_out: "Logout", pw_title: "CHANGE CREDENTIALS", update_btn: "APPLY CHANGES",
                settings: "SYSTEM CONFIGURATION", cfg_tds: "TDS Factor (< 10)", cfg_num: "Sensors (1-3)", caltemp_label: "Temp Thresholds (CALTEMP)",
                label_ts: "ThingSpeak Write Key", label_mcp: "XiaoZhi MCP Token",
                save_btn: "SAVE CONFIG", 
                sys_info: "SYSTEM INFORMATION", device: "Device ID:", heap: "Free RAM:", pump_label: "Main Pump", about_title: "SOFTWARE INFO",
                about_msg: "<strong>HDKP AI Smart Aqua Monitor.</strong><br>Developed by <b>HDKP</b><br>Version: <b>v1.8.6  - Professional AI</b>",
                dev_status: "Developer", lang_label: "Display Language", new_user: "New Username:", new_pass: "New Password:",
                btn_reset: "FACTORY RESET",
                
                last_water: "Last Water Change", last_filter: "Last Filter Change", last_salt: "Added Salt", btn_log: "Log Event", no_data: "No data yet", 
                light_label: "Lighting", oxy_label: "Oxygen Pump", sys_thresh: "System:", tank_s1: "Main Tank",
                tds_guide: "TDS < 170: Good; 170-600: Avg; > 800: Bad", eta: "ETA", ip_local: "Local IP:", btn_close: "Close",
                chat_title: "HDKP AI Assistant", chat_welcome: "Hello! I am HDKP AI. What do you want to know about the water?", chat_ph: "Ask about pH, TDS...", tbl_sensor: "Sensor",
                
                loading: "Loading...", analyzing: "Analyzing...",
                msg_ph_updated: "pH threshold updated!", msg_fill_pw: "⚠️ Please fill both Username and Password!", msg_confirm_pw: "Confirm changing credentials?",
                msg_logout: "✅ Updated! System will logout.", msg_confirm_reset: "❗ Factory reset?", msg_reset_ok: "✅ Reset successful!",
                msg_save_ok: "✅ Configuration saved!", msg_confirm_log: "Confirm reporting to AI: User just performed", msg_log_ok: "✅ Data logged successfully",
                msg_log_err: "❌ Network error! AI didn't log, but saved locally.", msg_ai_thinking: "AI is thinking...",
                msg_ai_writing: "AI is writing to memory...", msg_ai_err: "Connection lost to ESP32!", plc_ph: "pH Threshold",
                temp_low: "WARNING: Water too COLD!", temp_high: "WARNING: Water too HOT!"
            }
        };

        let labels=[], phH=[], tdsH=[], tempH=[];
        let phChartInst, tempChartInst, tdsChartInst;

        function performLogout() {
            var xhr = new XMLHttpRequest();
            xhr.open("GET", "/logout", true);
            // Gửi thông tin đăng nhập rác ép trình duyệt hủy phiên cũ
            xhr.setRequestHeader("Authorization", "Basic " + btoa("logout:logout"));
            xhr.onreadystatechange = function() {
                if (xhr.readyState === 4) {
                    window.location.href = "/logout_done";
                }
            };
            xhr.send();
        }

        //=========================
        function toggleMenu() { document.getElementById("myDropdown").classList.toggle("show"); }
        function openAboutModal() { document.getElementById("aboutModal").style.display = "block"; document.getElementById("txt-about-content").innerHTML = translations[currentLang].about_msg; if(document.getElementById("myDropdown").classList.contains('show')) toggleMenu(); }
        function closeAboutModal() { document.getElementById("aboutModal").style.display = "none"; }
        
        function openAccountModal() { 
            document.getElementById('accountModal').style.display = 'block'; 
            if(document.getElementById("myDropdown").classList.contains('show')) toggleMenu(); 
        }
        function closeAccountModal() { document.getElementById('accountModal').style.display = 'none'; }

        function openSystemModal() { 
            document.getElementById('systemModal').style.display = 'block'; 
            if(document.getElementById("myDropdown").classList.contains('show')) toggleMenu(); 
        }
        function closeSystemModal() { document.getElementById('systemModal').style.display = 'none'; }

        function toggleVis(icon, id) {
            const el = document.getElementById(id);
            if (el.type === "password") {
                el.type = "text";
                el.classList.add('show-text'); 
                icon.classList.remove('fa-eye');
                icon.classList.add('fa-eye-slash');
                icon.style.color = 'var(--primary)';
            } else {
                el.type = "password";
                el.classList.remove('show-text'); 
                icon.classList.remove('fa-eye-slash');
                icon.classList.add('fa-eye');
                icon.style.color = '#a4b0be';
            }
        }

        function changeLang(lang) {
            currentLang = lang; const t = translations[lang];
            
            document.getElementById('txt-title').innerText = t.title;
            document.getElementById('txt-hello').innerText = t.hello;
            
            document.querySelector('.txt-ph').innerHTML = t.ph;
            document.querySelector('.txt-temp').innerHTML = t.temp;
            document.querySelector('.txt-tds').innerHTML = t.tds;
            
            document.getElementById('txt-sys-mode-title').innerHTML = `<i class="fa-solid fa-microchip" style="color:#0984e3;"></i> ` + t.pump_mode;
            document.getElementById('txt-ai-title').innerHTML = `<i class="fa-solid fa-brain" style="color:#fd79a8;"></i> ` + t.ai_title;
            document.getElementById('txt-relay-title').innerHTML = `<i class="fa-solid fa-gamepad" style="color:#e17055;"></i> ` + t.relay_title;
            
            document.querySelector('.txt-chart-ph').innerHTML = `🧪 ` + t.chart_ph;
            document.querySelector('.txt-chart-temp').innerHTML = `🌡️ ` + t.chart_temp;
            document.querySelector('.txt-chart-tds').innerHTML = `💧 ` + t.chart_tds;
            
            document.getElementById('txt-settings-title').innerHTML = `<i class="fa-solid fa-sliders"></i> ` + t.settings;
            document.getElementById('btn-save').innerHTML = `💾 ` + t.save_btn;
            document.getElementById('btn-reset').innerHTML = `🔄 ` + t.btn_reset;
            document.getElementById('txt-sys-info').innerHTML = `🔧 ` + t.sys_info;
            document.getElementById('txt-pw-title').innerHTML = `🔒 ` + t.pw_title;
            document.getElementById('txt-about-title').innerHTML = `ℹ️ ` + t.about_title;
            document.getElementById('txt-chat-title').innerHTML = `<i class="fa-solid fa-robot" style="color:#00b894;"></i> ` + t.chat_title;

            document.querySelector('.txt-trend').innerText = t.trend;
            document.querySelector('.txt-risk').innerText = t.risk;
            document.getElementById('txt-eta').innerText = t.eta;
            document.getElementById('txt-s1').innerText = t.tank_s1;
            document.getElementById('txt-tds1').innerText = t.tds_guide;
            document.getElementById('txt-sys-thresh').innerText = t.sys_thresh;
            document.getElementById('txt-ip').innerText = t.ip_local;
            document.getElementById('btn-close-modal').innerText = t.btn_close;
            document.getElementById('txt-chat-welcome').innerText = t.chat_welcome;
            document.getElementById('txt-tbl-sensor').innerText = t.tbl_sensor;

            document.getElementById('txt-cfg-tds').innerText = t.cfg_tds;
            document.getElementById('txt-cfg-num').innerText = t.cfg_num;
            document.getElementById('txt-label-ts').innerText = t.label_ts;
            document.getElementById('txt-label-mcp').innerText = t.label_mcp;
            document.getElementById('txt-caltemp-label').innerText = t.caltemp_label;
            document.querySelector('.txt-device').innerText = t.device;
            document.querySelector('.txt-heap').innerText = t.heap;
            
            document.getElementById('txt-menu-account').innerText = t.menu_account;
            document.getElementById('txt-menu-settings').innerText = t.menu_set;
            
            document.getElementById('txt-menu-logout').innerText = t.menu_out;
            document.getElementById('txt-dev-status').innerText = t.dev_status;
            document.getElementById('txt-lang-label').innerText = t.lang_label;
            document.getElementById('btn-set-ph').innerText = t.set_btn;
            document.getElementById('txt-new-user').innerText = t.new_user;
            document.getElementById('txt-new-pass').innerText = t.new_pass;
            document.getElementById('btn-update-pw').innerText = t.update_btn;
            
            document.getElementById('txt-pump-label').innerText = t.pump_label;
            document.getElementById('txt-light-label').innerText = t.light_label;
            document.getElementById('txt-oxy-label').innerText = t.oxy_label;

            document.getElementById('txt-last-water-lbl').innerText = t.last_water;
            document.getElementById('txt-last-filter-lbl').innerText = t.last_filter;
            document.getElementById('txt-btn-log-salt').innerText = t.btn_log;
            document.getElementById('txt-last-salt-lbl').innerText = t.last_salt;
            document.getElementById('txt-btn-log-water').innerText = t.btn_log;
            document.getElementById('txt-btn-log-filter').innerText = t.btn_log;
            
            document.getElementById('chat-input').placeholder = t.chat_ph;
            document.getElementById('newPhThresh').placeholder = t.plc_ph;
            
            const saltEl = document.getElementById('txt-last-salt-change');
            if (saltEl.innerText === "Chưa có dữ liệu" || saltEl.innerText === "No data yet") saltEl.innerText = t.no_data;
            
            const aiMsgEl = document.getElementById('aiMsg');
            if (aiMsgEl && (aiMsgEl.innerText === "Đang phân tích..." || aiMsgEl.innerText === "Analyzing...")) {
                aiMsgEl.innerText = t.analyzing;
            }

            const waterEl = document.getElementById('txt-last-water-change');
            if (waterEl.innerText === "Chưa có dữ liệu" || waterEl.innerText === "No data yet") waterEl.innerText = t.no_data;
            const filterEl = document.getElementById('txt-last-filter-change');
            if (filterEl.innerText === "Chưa có dữ liệu" || filterEl.innerText === "No data yet") filterEl.innerText = t.no_data;

            const tdsL = document.getElementById('tdsLevel');
            if (tdsL) {
                const val = parseFloat(document.getElementById('tdsValue').innerText);
                if (!isNaN(val)) {
                    if(val < 170) tdsL.innerText = lang === 'vi' ? "TỐT" : "GOOD";
                    else if(val < 600) tdsL.innerText = lang === 'vi' ? "TRUNG BÌNH" : "AVERAGE";
                    else tdsL.innerText = lang === 'vi' ? "Ô NHIỄM!" : "POLLUTED!";
                } else {
                    tdsL.innerText = t.loading;
                }
            }

            localStorage.setItem('prefLang', lang);
            
            document.getElementById('autoWarningText').innerText = lang === 'vi' ? "Hệ thống đang chạy TỰ ĐỘNG. Nút điều khiển bị khóa." : "System is in AUTO mode. Controls are locked.";
            document.getElementById('btn-vi').className = (lang === 'vi') ? "btn btn-sm btn-primary" : "btn btn-sm";
            document.getElementById('btn-en').className = (lang === 'en') ? "btn btn-sm btn-primary" : "btn btn-sm";                                                                
        }

        // Tách làm 3 biểu đồ độc lập và ẩn chú thích
        function initCharts() {
            const opt = { 
                responsive: true, maintainAspectRatio: false, animation: false, 
                plugins: { legend: { display: false } }, // Ẩn chú thích để gọn không gian
                scales: { x: { display: true, ticks: { color: '#94a3b8', font: { size: 9 }, maxTicksLimit: 10 }, grid: { display: false } } } 
            };
            phChartInst = new Chart(document.getElementById('phChart'), { type: 'line', data: { labels, datasets: [{label: 'pH', borderColor: '#00d285', data: phH, fill: true, backgroundColor: 'rgba(0, 210, 133, 0.1)', tension: 0.4, pointRadius: 2}] }, options: opt });
            tempChartInst = new Chart(document.getElementById('tempChart'), { type: 'line', data: { labels, datasets: [{label: 'Temp', borderColor: '#ffa502', data: tempH, fill: true, backgroundColor: 'rgba(255, 165, 2, 0.1)', tension: 0.4, pointRadius: 2}] }, options: opt });  
            tdsChartInst = new Chart(document.getElementById('tdsChart'), { type: 'line', data: { labels, datasets: [{label: 'TDS', borderColor: '#00d2d3', data: tdsH, fill: true, backgroundColor: 'rgba(0, 210, 211, 0.1)', tension: 0.4, pointRadius: 2}] }, options: opt });
        }

        async function update() {
            try {
                const r = await fetch('/api/data'); if(!r.ok) return;
                const d = await r.json();

                let phDisplay = document.getElementById('phValue');
                let phGauge = document.getElementById('phGauge');
                let phErrorAlert = document.getElementById('phErrorAlert'); 

                if (d.pH_value == -1) {
                    phDisplay.innerText = "ERR";               
                    phDisplay.style.color = "#ff4757";         
                    if(phErrorAlert) phErrorAlert.style.display = "block"; 
                    phGauge.style.width = "0%";                
                } else {
                    phDisplay.innerText = d.pH_value.toFixed(2);
                    phDisplay.style.color = "";                
                    if(phErrorAlert) phErrorAlert.style.display = "none";  
                    phGauge.style.width = (d.pH_value / 14 * 100) + '%';   
                }

                document.getElementById('tempValue').innerText = d.temp_value.toFixed(1) + "°C";
                document.getElementById('tdsValue').innerText = d.tds_value.toFixed(0);
                document.getElementById('phThreshDisp').innerText = d.pH_threshold.toFixed(1);
                
                document.getElementById('tempGauge').style.width = Math.min((d.temp_value / 50) * 100, 100) + '%';
                document.getElementById('tdsGauge').style.width = Math.min(d.tds_value / 10, 100) + '%';
                
                const s1Min = parseFloat(document.getElementById('s0min').value);
                const s1Max = parseFloat(document.getElementById('s0max').value);
                const tempAlertEl = document.getElementById('tempAlertS1');
                const tempMsgEl = document.getElementById('txt-temp-msg-s1');
                const tempValEl = document.getElementById('tempValue');
                const tempGaugeEl = document.getElementById('tempGauge');

                if (!isNaN(s1Min) && !isNaN(s1Max)) {
                    if (d.temp_value < s1Min) {
                        tempAlertEl.style.display = 'block';
                        tempAlertEl.style.color = '#0984e3'; 
                        tempMsgEl.innerText = translations[currentLang].temp_low;
                        tempValEl.style.color = '#0984e3';
                        tempGaugeEl.style.background = '#0984e3';
                    } else if (d.temp_value > s1Max) {
                        tempAlertEl.style.display = 'block';
                        tempAlertEl.style.color = 'var(--danger)'; 
                        tempMsgEl.innerText = translations[currentLang].temp_high;
                        tempValEl.style.color = 'var(--danger)';
                        tempGaugeEl.style.background = 'var(--danger)';
                    } else {
                        tempAlertEl.style.display = 'none'; 
                        tempValEl.style.color = 'var(--warning)';
                        tempGaugeEl.style.background = 'var(--warning)';
                    }
                } else {
                    tempAlertEl.style.display = 'none';
                }
                
                const tdsL = document.getElementById('tdsLevel');
                if(d.tds_value < 170) { tdsL.innerText = currentLang === 'vi' ? "TỐT" : "GOOD"; tdsL.style.color="var(--success)"; }
                else if(d.tds_value < 600) { tdsL.innerText = currentLang === 'vi' ? "TRUNG BÌNH" : "AVERAGE"; tdsL.style.color="var(--warning)"; }
                else { tdsL.innerText = currentLang === 'vi' ? "Ô NHIỄM!" : "POLLUTED!"; tdsL.style.color="var(--danger)"; }
                
                document.getElementById('aiTrend').innerText = (d.prediction || "--").toUpperCase();
                document.getElementById('aiRisk').innerText = (d.risk || "THẤP").toUpperCase();
                document.getElementById('aiRisk').style.color = d.risk === "low" ? "var(--success)" : "var(--danger)";
                
                let etaVal = d.eta;
                let etaEl = document.getElementById('aiEta');
                
                if (etaVal === undefined || etaVal === 0 || etaVal === 999) {
                    etaEl.innerText = currentLang === 'vi' ? "Ổn định" : "Stable";
                    etaEl.style.color = "var(--success)"; 
                } else {
                    etaEl.innerText = etaVal + (currentLang === 'vi' ? " Phút" : " Mins");
                    etaEl.style.color = "var(--danger)"; 
                }
                
                document.getElementById('aiMsg').innerText = d.ai_message || "Stable";
                
                const isMan = d.system_mode === 'manual';
                document.getElementById('txt-mode-display').innerText = isMan ? "MANUAL" : "AUTO";
                document.getElementById('txt-mode-display').style.color = isMan ? "var(--warning)" : "var(--primary)";
                
                document.getElementById('mManual').className = isMan ? "btn btn-sm btn-manual" : "btn btn-sm btn-off";
                document.getElementById('mAuto').className = isMan ? "btn btn-sm btn-off" : "btn btn-sm btn-auto";
                
                const manualArea = document.getElementById('manualControlArea');
                const autoWarn = document.getElementById('autoWarningText');
                if(!isMan) { manualArea.classList.add('disabled-area'); autoWarn.style.display = 'block'; } 
                else { manualArea.classList.remove('disabled-area'); autoWarn.style.display = 'none'; }
                
                document.getElementById('deviceId').innerText = d.device_id || "ESP32-HDKP";
                document.getElementById('localIp').innerText = d.local_ip || "0.0.0.0";
                document.getElementById('freeHeap').innerText = d.free_heap + " bytes";
                document.getElementById('currentAdminUser').innerText = d.admin_user || "Admin";
                
                let stPump = (d.pump !== undefined) ? d.pump : false; 
                document.getElementById('btn-relay-0').checked = stPump; 
                document.getElementById('card-relay-0').classList.toggle('active', stPump);
                
                let stLight = (d.relay_0 !== undefined) ? d.relay_0 : false;
                document.getElementById('btn-relay-1').checked = stLight; 
                document.getElementById('card-relay-1').classList.toggle('active', stLight);
                
                let stOxy = (d.relay_1 !== undefined) ? d.relay_1 : false;
                document.getElementById('btn-relay-2').checked = stOxy; 
                document.getElementById('card-relay-2').classList.toggle('active', stOxy);
                
                const now = new Date();
                const timeStr = now.getHours().toString().padStart(2, '0') + ':' + now.getMinutes().toString().padStart(2, '0') + ':' + now.getSeconds().toString().padStart(2, '0');
                const dateStr = now.getDate().toString().padStart(2, '0') + '/' + (now.getMonth() + 1).toString().padStart(2, '0');
                labels.push([timeStr, dateStr]); tdsH.push(d.tds_value); tempH.push(d.temp_value); phH.push(d.pH_value);
                if(labels.length > 20) { labels.shift(); tdsH.shift(); tempH.shift(); phH.shift(); }
                
                // Cập nhật cả 3 biểu đồ
                if(phChartInst) phChartInst.update();
                if(tempChartInst) tempChartInst.update();
                if(tdsChartInst) tdsChartInst.update();
            } catch(e) {}
        }

        function post(u, d) { return fetch(u, {method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify(d)}).then(r => { update(); if (!r.ok) { alert(currentLang === 'vi' ? '❌ Giá trị không hợp lệ hoặc không được phép' : '❌ Invalid value or not allowed'); return Promise.reject(new Error('HTTP ' + r.status)); } return r; }); }
        function setRelay(i, s) { post('/api/control/relay', {relay:i, status:s}); }
        function setSysMode(m) { post('/api/control/mode', {mode:m}); }
        function updatePh() { const v = document.getElementById('newPhThresh').value; if(v) post('/api/settings/ph_threshold', {threshold: parseFloat(v)}).then(()=>alert(translations[currentLang].msg_ph_updated)); }

        function changeCredentials() {
            const oldP = document.getElementById('sysOldPass').value.trim();
            const u = document.getElementById('sysAdminUser').value.trim();
            const p = document.getElementById('sysAdminPass').value.trim();
            
            if(!oldP || !u || !p) { 
                alert(translations[currentLang].msg_fill_pw); 
                return; 
            }
            
            if(confirm(translations[currentLang].msg_confirm_pw)) {
                fetch('/api/settings/credentials', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify({oldPass: oldP, user: u, pass: p})
                })
                .then(async (res) => {
                    const data = await res.json();
                    if(res.ok && data.status === 'ok') {
                        alert("✅ Đổi tài khoản thành công! Trình duyệt sẽ yêu cầu đăng nhập lại.");
                        // Xóa cookie/xác thực cũ bằng cách reload ép buộc với auth giả
                        window.location.href = "http://logout:logout@" + window.location.host;
                        setTimeout(() => { window.location.href = "/"; }, 500);
                    } else {
                        alert("❌ BÁO LỖI: " + (data.msg || "Không thể đổi tài khoản!"));
                    }
                })
                .catch(err => {
                    alert("❌ Lỗi kết nối đến ESP32!");
                });
            }
        }

        function togglePasswordVisibility(inputId, iconElement) {
            const inputField = document.getElementById(inputId);
            
            if (inputField.type === "password") {
                inputField.type = "text";
                iconElement.classList.remove("fa-eye");
                iconElement.classList.add("fa-eye-slash"); 
                iconElement.style.color = "#007bff";       
            } else {
                inputField.type = "password";
                iconElement.classList.remove("fa-eye-slash");
                iconElement.classList.add("fa-eye");       
                iconElement.style.color = "#6c757d";       
            }
        }

        function saveCfg() {
            const tds = document.getElementById('cfgTds');
            const num = document.getElementById('cfgNum');
            if(parseFloat(tds.value) >= 10) { alert(currentLang === 'vi' ? "❌ Lỗi: Hệ số TDS phải nhỏ hơn 10!" : "❌ Error: TDS Factor must be less than 10!"); return; }
            if(parseInt(num.value) > 3 || parseInt(num.value) < 1) { alert(currentLang === 'vi' ? "❌ Lỗi: Số cảm biến từ 1-3!" : "❌ Error: 1-3 sensors only!"); return; }
            const data = {
                tds_factor: parseFloat(tds.value), num_sensors: parseInt(num.value),
                ts_key: document.getElementById('sysTsKey').value, mcp_token: document.getElementById('sysMcpToken').value,
                temp_min: [parseFloat(document.getElementById('s0min').value), parseFloat(document.getElementById('s1min').value), parseFloat(document.getElementById('s2min').value)],
                temp_max: [parseFloat(document.getElementById('s0max').value), parseFloat(document.getElementById('s1max').value), parseFloat(document.getElementById('s2max').value)]
            };
            post('/api/sysconfig', data).then(()=>alert(translations[currentLang].msg_save_ok));
        }

        function resetToDefaults() { if(confirm(translations[currentLang].msg_confirm_reset)) { fetch('/api/sysconfig/reset', {method:'POST'}).then(() => { alert(translations[currentLang].msg_reset_ok); location.reload(); }); } }

        function toggleChat() { const win = document.getElementById('chat-window'); win.style.display = (win.style.display === 'flex') ? 'none' : 'flex'; }
        function handleKey(e) { if(e.key === 'Enter') sendChat(); }

        async function sendChat() {
            const input = document.getElementById("chat-input");
            const text = input.value.trim();
            if(!text) return;
            addMessage(text, 'user');
            input.value = "";
            const loadingId = "loading-" + Date.now();
            addMessage(`<span style="color:#94a3b8"><i class="fa-solid fa-comment-dots fa-fade"></i> ${translations[currentLang].msg_ai_thinking}</span>`, 'ai', loadingId, true);
            
            try {
                const res = await fetch("/api/ai/chat", { method: "POST", headers: {"Content-Type": "application/json"}, body: JSON.stringify({ message: text }) });
                const data = await res.json();
                const loadingEl = document.getElementById(loadingId);
                if (loadingEl) loadingEl.remove();
                addMessage(data.reply || data.msg || data.status || "...", 'ai');
            } catch(e) {
                const loadingEl = document.getElementById(loadingId);
                if (loadingEl) loadingEl.remove();
                addMessage(`<span style='color:red'><i class='fa-solid fa-triangle-exclamation'></i> ${translations[currentLang].msg_ai_err}</span>`, 'ai', "", true);
            }
        }

        // 🌟 THÊM HÀM NÀY VÀO ĐÂY ĐỂ FIX LỖI GIAO DIỆN CHAT 🌟
        function addMessage(msg, sender, id = "", trustedHtml = false) {
            const div = document.getElementById("messages");
            const m = document.createElement("div");
            m.className = `msg ${sender}`;
            if (trustedHtml) { m.innerHTML = msg; }          // chỉ cho markup do chính trang này tạo
            else { m.textContent = String(msg); m.style.whiteSpace = "pre-wrap"; }  // nội dung người dùng / AI: luôn là text
            if(id) m.id = id;   
            div.appendChild(m);
            div.scrollTop = div.scrollHeight;
        }
        // ===================================================

        async function logMaintenance(actionType) {
            let displayAction = actionType;
            if (currentLang === 'en') {
                if (actionType === 'water') displayAction = 'Water Change';
                else if (actionType === 'filter') displayAction = 'Filter Change';
                else if (actionType === 'salt') displayAction = 'Added Salt'; 
            } else {
                if (actionType === 'water') displayAction = 'Thay nước';
                else if (actionType === 'filter') displayAction = 'Thay bông lọc';
                else if (actionType === 'salt') displayAction = 'Bổ sung muối'; 
            }

            if(!confirm(`${translations[currentLang].msg_confirm_log} [${displayAction}]?`)) return;

            const now = new Date();
            const dateStr = now.getDate().toString().padStart(2, '0') + '/' + (now.getMonth() + 1).toString().padStart(2, '0') + '/' + now.getFullYear() + ' ' + now.getHours().toString().padStart(2, '0') + ':' + now.getMinutes().toString().padStart(2, '0');

            // 1. Cập nhật ngay lên màn hình giao diện
            if (actionType === 'water') {
                document.getElementById('txt-last-water-change').innerText = dateStr;
            } else if (actionType === 'filter') {
                document.getElementById('txt-last-filter-change').innerText = dateStr;
            } else if (actionType === 'salt') { 
                document.getElementById('txt-last-salt-change').innerText = dateStr;
            }

            // 2. Bắn dữ liệu vào Két sắt ESP32
            fetch('/api/maintenance', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({ action: actionType, date: dateStr })
            }).catch(e => console.log("⚠️ Lỗi lưu bảo trì vào ESP32:", e));

            const msg = `[GHI NHẬN BẢO TRÌ THỦ CÔNG]: Tôi vừa mới ${displayAction} cho bể cá. Hãy lưu vào trí nhớ để phân tích chất lượng nước sau này.`;
            
            addMessage(`<i class="fa-solid fa-check-circle"></i> Đã gửi báo cáo: ${displayAction}`, 'user', "", true);
            const loadingId = "loading-log-" + Date.now();
            addMessage(`<span style="color:#94a3b8"><i class="fa-solid fa-pen fa-bounce"></i> ${translations[currentLang].msg_ai_writing}</span>`, 'ai', loadingId, true);

            try {
                const res = await fetch("/api/ai/chat", { method: "POST", headers: {"Content-Type": "application/json"}, body: JSON.stringify({ message: msg }) });
                const data = await res.json();
                const loadingEl = document.getElementById(loadingId);
                if (loadingEl) loadingEl.remove();
                addMessage(data.reply || data.msg || data.status || "...", 'ai');
                alert(`${translations[currentLang].msg_log_ok} [${displayAction}]!`);
            } catch(e) {
                const loadingEl = document.getElementById(loadingId);
                if (loadingEl) loadingEl.remove();
                alert(translations[currentLang].msg_log_err);
            }
        }
        // 🌟 THÊM HÀM NÀY VÀO ĐÂY ĐỂ TW ĐỘNG KIỂM TRA BẢN CẬP NHẬT 🌟
        // Hàm chuẩn để so sánh Semantic Versioning (ví dụ: 1.8.7 so với 1.8.6)
        function compareVersions(v1, v2) {
            const p1 = v1.split('.').map(Number);
            const p2 = v2.split('.').map(Number);
            for (let i = 0; i < Math.max(p1.length, p2.length); i++) {
                const n1 = p1[i] || 0;
                const n2 = p2[i] || 0;
                if (n1 > n2) return 1;
                if (n1 < n2) return -1;
            }
            return 0;
        }

        // Kích hoạt kiểm tra tự động ngay khi trang Dashboard được tải xong
        window.addEventListener('DOMContentLoaded', checkForUpdates);
        //=================================================================
    
        window.onload = () => {
            // RÚT LỊCH SỬ BẢO TRÌ TỪ ESP32
            fetch('/api/maintenance')
                .then(r => r.json())
                .then(d => {
                    document.getElementById('txt-last-water-change').innerText = d.water;
                    document.getElementById('txt-last-filter-change').innerText = d.filter;
                    document.getElementById('txt-last-salt-change').innerText = d.salt;
                })
                .catch(e => console.log("⚠️ Lỗi tải lịch sử bảo trì từ ESP32:", e));

            try { 
                initCharts(); 
            } catch(e) { 
                console.log("⚠️ Lỗi: Không tải được biểu đồ do mất mạng!", e); 
            }

            const savedLang = localStorage.getItem('prefLang') || 'vi'; 
            changeLang(savedLang); 
            
            update(); 
            setInterval(update, 5000); 

            // Tải thông số cấu hình hệ thống
            fetch('/api/sysconfig').then(r=>r.json()).then(d=>{
                document.getElementById('cfgTds').value=d.tds_factor; 
                document.getElementById('cfgNum').value=d.num_sensors;
                document.getElementById('sysTsKey').value=d.ts_key || ""; 
                document.getElementById('sysMcpToken').value=d.mcp_token || "";
                for(let i=0; i<3; i++) { 
                    document.getElementById('s'+i+'min').value = d.temp_min[i]; 
                    document.getElementById('s'+i+'max').value = d.temp_max[i]; 
                }
            }).catch(e => console.log("⚠️ Lỗi tải cấu hình", e));
        };

        window.onclick = function(e) { if (!e.target.matches('.dropbtn') && !e.target.closest('.dropdown')) { var d = document.getElementById("myDropdown"); if (d && d.classList.contains('show')) d.classList.remove('show'); } }
    </script>
</body>
</html>
)rawliteral";

class WebServerManager {
private:
    WebServer* mainServer;
    WiFiPortal* wifiPortal;      
    SensorManager* sensorManagerPtr;

    // Hàm phụ trợ dùng chung để kiểm tra xác thực
    bool checkAuth() {
        if (!mainServer->authenticate(admin_user.c_str(), admin_pass.c_str())) {
            mainServer->send(401, "application/json", "{\"status\":\"unauthorized\"}");
            return false;
        }
        return true;
    }

public:
    WebServerManager(WebServer* mainServerPtr, WiFiPortal* wifiMgr, SensorManager* sensorMgr) {
        mainServer = mainServerPtr;
        wifiPortal = wifiMgr;
        sensorManagerPtr = sensorMgr;
    }

    void setupMainServer() {
        Preferences prefs; 
        prefs.begin("sys_config", false);
        admin_user = prefs.getString("admin_user", "admin");
        if (!prefs.isKey("admin_pass")) {
            // Lần chạy đầu: sinh mật khẩu ngẫu nhiên (KHÔNG dùng mật khẩu mặc định chung)
            admin_pass = randomToken(10);
            prefs.putString("admin_user", admin_user);
            prefs.putString("admin_pass", admin_pass);
            Serial.println(F("=================================================="));
            Serial.printf("🔐 MẬT KHẨU WEB LẦN ĐẦU -> user: %s | pass: %s\n", admin_user.c_str(), admin_pass.c_str());
            Serial.println(F("   Hãy đổi mật khẩu ngay trong giao diện web."));
            Serial.println(F("=================================================="));
        } else {
            admin_pass = prefs.getString("admin_pass", "");
            if (admin_pass == "123456") Serial.println(F("⚠️ Bạn đang dùng mật khẩu yếu '123456'. Hãy đổi trong giao diện web!"));
        }
        prefs.end();
        mainServer->on("/", HTTP_GET, [this]() {
            if (!mainServer->authenticate(admin_user.c_str(), admin_pass.c_str())) return mainServer->requestAuthentication();
            mainServer->sendHeader("X-Frame-Options", "DENY");
            mainServer->sendHeader("X-Content-Type-Options", "nosniff");
            mainServer->sendHeader("Referrer-Policy", "no-referrer");
            // Gửi dữ liệu HTML trực tiếp từ PROGMEM
            mainServer->send_P(200, "text/html", MAIN_DASHBOARD_HTML);
        });

        mainServer->on("/api/data", HTTP_GET, [this]() { handleGetData(); });
        mainServer->on("/api/control/relay", HTTP_POST, [this]() { handleRelayControl(); }); 
        mainServer->on("/api/control/mode", HTTP_POST, [this]() { handleModeControl(); });
        mainServer->on("/api/settings/ph_threshold", HTTP_POST, [this]() { handlePHThreshold(); });
        mainServer->on("/api/sysconfig", HTTP_GET, [this]() { handleGetSysConfig(); });
        mainServer->on("/api/sysconfig", HTTP_POST, [this]() { handlePostSysConfig(); });
        mainServer->on("/api/sysconfig/reset", HTTP_POST, [this]() { handleResetConfig(); });
        mainServer->on("/api/settings/credentials", HTTP_POST, [this]() { handleCredentials(); });
        mainServer->on("/api/ai/chat", HTTP_POST, [this]() { handleAIChat(); });
        mainServer->on("/api/maintenance", HTTP_GET, [this]() { handleGetMaintenance(); });
        mainServer->on("/api/maintenance", HTTP_POST, [this]() { handlePostMaintenance(); });

        // 1. Nhận lệnh hủy phiên
        mainServer->on("/logout", HTTP_GET, [this]() {
            mainServer->sendHeader("Connection", "close");
            mainServer->send(401, "application/json", "{\"status\":\"logged_out\"}");
        });

        // 2. Trang thông báo đăng xuất hoàn tất
        mainServer->on("/logout_done", HTTP_GET, [this]() {
            String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
            html += "<title>Đã đăng xuất</title><style>body{font-family:sans-serif;text-align:center;padding:50px;background:#f5f7fa;}</style></head><body>";
            html += "<h2 style='color:#2ed573;'>✅ ĐÃ ĐĂNG XUẤT THÀNH CÔNG</h2>";
            html += "<p>Phiên làm việc đã kết thúc an toàn.</p>";
            html += "<a href='/' style='display:inline-block;padding:10px 20px;background:#6c5ce7;color:white;text-decoration:none;border-radius:8px;font-weight:bold;'>🔑 Đăng nhập lại</a>";
            html += "</body></html>";
            mainServer->send(200, "text/html", html);
        });

        mainServer->begin();
    }

    void handleClients() { mainServer->handleClient(); }    

private:
    void handleGetData() {
        if (!checkAuth()) return;
        
        #if ARDUINOJSON_VERSION_MAJOR >= 7
            JsonDocument doc;
        #else
            DynamicJsonDocument doc(1024);
        #endif
         
        DisplayData wqData = monitor.getDisplayData();
        float curTemp = (wqData.temps[0] > -50) ? wqData.temps[0] : 0;
        
        doc["pH_value"] = pH_value;
        doc["system_mode"] = system_mode ? "manual" : "auto";
        doc["pH_threshold"] = pH_threshold;
        doc["tds_value"] = wqData.tds;
        doc["temp_value"] = curTemp; 
        // Đóng gói dữ liệu Rơ-le gửi lên Web:
        doc["pump"] = sensorManager.getPumpStatus(); // Trạng thái Bơm
        doc["relay_0"] = monitor.getRelayState(0);   // Trạng thái Đèn
        doc["relay_1"] = monitor.getRelayState(1);   // Trạng thái Oxy
        doc["wifi_ssid"] = WiFi.SSID();
        doc["local_ip"] = WiFi.localIP().toString();
        doc["free_heap"] = ESP.getFreeHeap();
        doc["admin_user"] = admin_user;
        doc["device_id"] = "HDKP AI Smart Aqua Monitor";

        // XUẤT TRỰC TIẾP TỪ BIẾN TOÀN CỤC currentAIResult
        doc["quality"] = currentAIResult.quality;
        doc["ai_message"] = currentAIResult.message;
        doc["prediction"] = currentAIResult.prediction;
        doc["risk"] = currentAIResult.risk;
        doc["eta"] = currentAIResult.eta;

        String json; serializeJson(doc, json);
        mainServer->send(200, "application/json", json);
    }

    void handleGetSysConfig() {
        if (!checkAuth()) return;

        #if ARDUINOJSON_VERSION_MAJOR >= 7
            JsonDocument doc;
        #else
            DynamicJsonDocument doc(1024);
        #endif
        
        doc["tds_factor"] = config.getTdsFactor();
        doc["num_sensors"] = config.getNumTempSensors();
        doc["ts_key"] = config.getThingSpeakAPIKey();
        doc["mcp_token"] = config.getMcpToken();
        JsonArray tMin = doc.createNestedArray("temp_min");
        JsonArray tMax = doc.createNestedArray("temp_max");
        for(int i=0; i<3; i++) {
            tMin.add(config.getMinTempThreshold(i));
            tMax.add(config.getMaxTempThreshold(i));
        }
        
        String json; 
        serializeJson(doc, json); 
        mainServer->send(200, "application/json", json);
    }

    void handlePostSysConfig() {
        if (!checkAuth()) return;
        
        if (!mainServer->hasArg("plain")) {
            mainServer->send(400, "application/json", "{\"status\":\"error\",\"message\":\"No data\"}");
            return;
        }

        #if ARDUINOJSON_VERSION_MAJOR >= 7
            JsonDocument doc;
        #else
            DynamicJsonDocument doc(1536);
        #endif
        
        DeserializationError error = deserializeJson(doc, mainServer->arg("plain"));
        if (error) {
            mainServer->send(400, "application/json", "{\"status\":\"error\", \"message\":\"Invalid JSON\"}");
            return;
        }

        bool ok = true;
        if (doc.containsKey("tds_factor") && !config.setTdsFactor(doc["tds_factor"])) ok = false;
        if (doc.containsKey("num_sensors") && !config.setNumTempSensors(doc["num_sensors"])) ok = false;
        if (doc.containsKey("ts_key") && !config.setThingSpeakAPIKey(doc["ts_key"].as<String>())) ok = false;
        if (doc.containsKey("mcp_token") && !config.setMcpToken(doc["mcp_token"].as<String>())) ok = false;
        if (doc.containsKey("temp_min") && doc.containsKey("temp_max")) {
            for (int i = 0; i < 3; i++) {
                // Ghi đồng thời MIN/MAX, yêu cầu MIN < MAX
                if (!config.setTempRange(i, doc["temp_min"][i] | NAN, doc["temp_max"][i] | NAN)) ok = false;
            }
        }

        if (ok) mainServer->send(200, "application/json", "{\"status\":\"success\"}");
        else    mainServer->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Giá trị không hợp lệ\"}");
    }

    void handleResetConfig() {
        if (!checkAuth()) return;

        config.setTdsFactor(0.5); 
        config.setNumTempSensors(1);
        config.setThingSpeakAPIKey("");
        config.setMcpToken("");
        for(int i=0; i<3; i++) {
            config.setTempThreshold(i, true, 0.0);
            config.setTempThreshold(i, false, 50.0);
        }
        mainServer->send(200, "application/json", "{\"status\":\"success\"}");
    }

    void handleRelayControl() { 
        if (!checkAuth()) return;

        #if ARDUINOJSON_VERSION_MAJOR >= 7
            JsonDocument doc;
        #else
            DynamicJsonDocument doc(128);
        #endif
        
        DeserializationError error = deserializeJson(doc, mainServer->arg("plain"));
        if (error) {
            mainServer->send(400, "application/json", "{\"status\":\"error\", \"message\":\"Invalid JSON\"}");
            return;
        }

        // Bắt buộc có đủ trường hợp lệ, tránh mặc định relay=0 (bơm) khi thiếu dữ liệu
        if (!doc["relay"].is<int>() || !doc["status"].is<bool>()) {
            mainServer->send(400, "application/json", "{\"status\":\"error\", \"message\":\"Missing relay/status\"}");
            return;
        }
        int relayIndex = doc["relay"].as<int>();
        bool state = doc["status"].as<bool>();
        if (relayIndex < 0 || relayIndex > 2) {
            mainServer->send(400, "application/json", "{\"status\":\"error\", \"message\":\"Invalid relay\"}");
            return;
        }

        // Web ra lệnh -> Chuyển mạch sang MANUAL
        system_mode = true; 

        if (relayIndex == 0) {
            sensorManager.controlPump(state); 
        } 
        else if (relayIndex == 1) {
            monitor.setRelayManual(0, state); 
        } 
        else if (relayIndex == 2) {
            monitor.setRelayManual(1, state); 
        }

        mainServer->send(200, "application/json", "{\"status\":\"success\"}"); 
    }
    
    void handleModeControl() { 
        if (!checkAuth()) return;

        #if ARDUINOJSON_VERSION_MAJOR >= 7
            JsonDocument doc;
        #else
            DynamicJsonDocument doc(128);
        #endif
        
        DeserializationError error = deserializeJson(doc, mainServer->arg("plain"));
        if (error) {
            mainServer->send(400, "application/json", "{\"status\":\"error\", \"message\":\"Invalid JSON\"}");
            return;
        }

        system_mode = (doc["mode"] == "manual"); 
        if (!system_mode) { 
            // Chuyển các rơ-le thư viện sang Auto
            for(int i=0; i<3; i++) monitor.setRelayAuto(i);
            // Ép bơm tính toán lại trạng thái ngay lập tức theo ngưỡng pH
            sensorManager.autoControl(pH_threshold); 
        } 
        mainServer->send(200, "application/json", "{\"status\":\"success\"}"); 
    }

    void handlePHThreshold() { 
        if (!checkAuth()) return;

        #if ARDUINOJSON_VERSION_MAJOR >= 7
            JsonDocument doc;
        #else
            DynamicJsonDocument doc(128);
        #endif
        
        DeserializationError error = deserializeJson(doc, mainServer->arg("plain"));
        if (error) {
            mainServer->send(400, "application/json", "{\"status\":\"error\", \"message\":\"Invalid JSON\"}");
            return;
        }

        if (!doc["threshold"].is<float>()) {
            mainServer->send(400, "application/json", "{\"status\":\"error\", \"message\":\"Missing threshold\"}");
            return;
        }
        float newThreshold = doc["threshold"].as<float>();
        if (!isfinite(newThreshold) || newThreshold < 0.5f || newThreshold > 13.5f) {
            mainServer->send(400, "application/json", "{\"status\":\"error\", \"message\":\"Threshold must be 0.5-13.5\"}");
            return;
        }
        pH_threshold = newThreshold;
        Preferences prefs; 
        prefs.begin("sys_config", false); 
        prefs.putFloat("ph_thresh", pH_threshold); 
        prefs.end(); 
        mainServer->send(200, "application/json", "{\"status\":\"success\"}"); 
    }

    void handleCredentials() {
        if (!checkAuth()) return;

        if (!mainServer->hasArg("plain")) {
            mainServer->send(400, "application/json", "{\"status\":\"error\",\"msg\":\"No data\"}");
            return;
        }

        #if ARDUINOJSON_VERSION_MAJOR >= 7
            JsonDocument doc;
        #else
            DynamicJsonDocument doc(512);
        #endif
        
        DeserializationError error = deserializeJson(doc, mainServer->arg("plain"));

        if (error) {
            mainServer->send(400, "application/json", "{\"status\":\"error\",\"msg\":\"Lỗi đọc JSON\"}");
            return;
        }

        String incomingOldPass = doc["oldPass"] | ""; 
        String incomingNewUser = doc["user"] | "";
        String incomingNewPass = doc["pass"] | "";

        if (incomingOldPass != admin_pass) {
            mainServer->send(401, "application/json", "{\"status\":\"error\", \"msg\":\"Sai mật khẩu hiện tại!\"}");
            return;
        }

        if (incomingNewUser.length() > 32 || incomingNewPass.length() > 64) {
            mainServer->send(400, "application/json", "{\"status\":\"error\",\"msg\":\"User tối đa 32, Pass tối đa 64 ký tự\"}");
            return;
        }
        if (incomingNewPass.length() < 8) {
            mainServer->send(400, "application/json", "{\"status\":\"error\",\"msg\":\"Mật khẩu mới phải từ 8 ký tự trở lên\"}");
            return;
        }

        if (incomingNewUser.length() > 0 && incomingNewPass.length() > 0) {
            admin_user = incomingNewUser;
            admin_pass = incomingNewPass;

            // Lưu chặt chẽ vào Flash NVS
            Preferences prefs;
            prefs.begin("sys_config", false);
            prefs.putString("admin_user", admin_user);
            prefs.putString("admin_pass", admin_pass);
            prefs.end();

            Serial.printf("✅ Đã đổi tài khoản web sang user: %s\n", admin_user.c_str());
            mainServer->send(200, "application/json", "{\"status\":\"ok\"}");
        } else {
            mainServer->send(400, "application/json", "{\"status\":\"error\",\"msg\":\"User/Pass không được để trống\"}");
        }
    }

    // XỬ LÝ TIN NHẮN CHATBOT + LỆNH ĐIỀU KHIỂN
    void handleAIChat() {
        if (!checkAuth()) return;   // BẮT BUỘC đăng nhập: endpoint này điều khiển được bơm/relay

        if (!mainServer->hasArg("plain")) {
            mainServer->send(400, "application/json", "{\"reply\":\"Thiếu dữ liệu\"}");
            return;
        }
        {
            #if ARDUINOJSON_VERSION_MAJOR >= 7
                JsonDocument inputDoc;
            #else
                DynamicJsonDocument inputDoc(768);
            #endif
            
            if (deserializeJson(inputDoc, mainServer->arg("plain")) || !inputDoc["message"].is<const char*>()) {
                mainServer->send(400, "application/json", "{\"reply\":\"JSON không hợp lệ\"}");
                return;
            }
            String userMsg = inputDoc["message"].as<String>();
            if (userMsg.length() > 300) userMsg = userMsg.substring(0, 300);
            
            String userMsgLower = userMsg;                                                                                                               
            userMsgLower.toLowerCase();

            String aiReply = "";
            bool isHardwareCommand = false;                                        

            // --- NHÓM ĐIỀU KHIỂN THIẾT BỊ ---
            if (userMsgLower.indexOf("bật bơm") >= 0 || userMsgLower.indexOf("mở bơm") >= 0 || userMsgLower.indexOf("mo bom") >= 0 || userMsgLower.indexOf("bat bom") >= 0) {
                system_mode = true;
                sensorManager.controlPump(true); 
                
                aiReply = "✅ Tuân lệnh! Đã BẬT bơm và chuyển sang chế độ THỦ CÔNG.";
                isHardwareCommand = true;                                        
            } 
            else if (userMsgLower.indexOf("tắt bơm") >= 0 || userMsgLower.indexOf("dừng bơm") >= 0 || userMsgLower.indexOf("dung bom") >= 0 || userMsgLower.indexOf("tat bom") >= 0) {
                system_mode = true;
                sensorManager.controlPump(false); 
                
                aiReply = "🛑 Đã rõ! Em đã TẮT bơm an toàn.";
                isHardwareCommand = true; 
            }
            else if (userMsgLower.indexOf("bật đèn") >= 0 || userMsgLower.indexOf("mở đèn") >= 0 || userMsgLower.indexOf("mo den") >= 0 || userMsgLower.indexOf("bat den") >= 0) {
                system_mode = true;
                monitor.setRelayManual(0, true); 
                
                aiReply = "💡 Đã bật đèn. Hệ thống đang lung linh luôn anh nhé!";
                isHardwareCommand = true; 
            }
            else if (userMsgLower.indexOf("tắt đèn") >= 0 || userMsgLower.indexOf("tat den") >= 0) {
                system_mode = true;
                monitor.setRelayManual(0, false); 
                
                aiReply = "🌑 Đã tắt đèn để tiết kiệm điện ạ.";
                isHardwareCommand = true; 
            }
            else if (userMsgLower.indexOf("bật sủi oxy") >= 0 || userMsgLower.indexOf("bat sui oxy") >= 0 || userMsgLower.indexOf("bật sủi") >= 0 || userMsgLower.indexOf("bat sui") >= 0 || userMsgLower.indexOf("bat suc oxy") >= 0 || userMsgLower.indexOf("bật sục oxy") >= 0 || userMsgLower.indexOf("bật máy sủi") >= 0) {
                system_mode = true;
                monitor.setRelayManual(1, true); 
                
                aiReply = "🫧 Tuân lệnh! Em đã BẬT sục Oxy/Lọc cho cá.";
                isHardwareCommand = true;
            }
            else if (userMsgLower.indexOf("tắt oxy") >= 0 || userMsgLower.indexOf("tắt lọc") >= 0 || userMsgLower.indexOf("tắt sủi") >= 0 || userMsgLower.indexOf("tat oxy") >= 0 || userMsgLower.indexOf("tat sui") >= 0) {
                system_mode = true;
                monitor.setRelayManual(1, false); 
                
                aiReply = "🛑 Đã rõ! Em đã TẮT sục Oxy/Lọc.";
                isHardwareCommand = true;
            }                                                                                                                                                                                            
            else if (userMsgLower.indexOf("tự động") >= 0 || userMsgLower.indexOf("auto") >= 0 || userMsgLower.indexOf("tu dong") >= 0) {
                system_mode = false;
                for(int i=0; i<3; i++) monitor.setRelayAuto(i);
                
                aiReply = "🤖 Hệ thống đã về chế độ TỰ ĐỘNG. Em sẽ tự tính toán thời điểm bật tắt thiết bị.";
                isHardwareCommand = true;                                        
            }
            else if (userMsgLower.indexOf("thủ công") >= 0 || userMsgLower.indexOf("thu cong") >= 0 || userMsgLower.indexOf("bằng tay") >= 0 || userMsgLower.indexOf("bang tay") >= 0 || userMsgLower.indexOf("manual") >= 0) {
                system_mode = true;
                
                aiReply = "🛠️ Đã chuyển hệ thống sang chế độ THỦ CÔNG. Em sẽ không tự ý bật/tắt thiết bị nữa, mọi quyền quyết định thuộc về bạn!";
                isHardwareCommand = true;
            }

            // --- NHÓM KIỂM TRA TRẠNG THÁI THIẾT BỊ HIỆN TẠI ---
            else if (userMsgLower.indexOf("trạng thái bơm") >= 0 || userMsgLower.indexOf("bơm đang") >= 0 || userMsgLower.indexOf("bom dang") >= 0) {
                bool state = sensorManager.getPumpStatus(); 
                aiReply = state ? "🌊 Máy bơm chính hiện đang BẬT anh nhé." : "🛑 Máy bơm chính hiện đang TẮT.";
                isHardwareCommand = true;
            }
            else if (userMsgLower.indexOf("trạng thái đèn") >= 0 || userMsgLower.indexOf("đèn đang") >= 0 || userMsgLower.indexOf("den dang") >= 0) {
                bool state = monitor.getRelayState(0);
                aiReply = state ? "💡 Đèn chiếu sáng hiện đang BẬT." : "🌑 Đèn chiếu sáng hiện đang TẮT.";
                isHardwareCommand = true;
            }
            else if (userMsgLower.indexOf("trạng thái oxy") >= 0 || userMsgLower.indexOf("sủi đang") >= 0 || userMsgLower.indexOf("oxy đang") >= 0 || userMsgLower.indexOf("sui dang") >= 0) {
                bool state = monitor.getRelayState(1);
                aiReply = state ? "🫧 Sục Oxy/Lọc hiện đang BẬT." : "🛑 Sục Oxy/Lọc hiện đang TẮT.";
                isHardwareCommand = true;
            }
            else if (userMsgLower.indexOf("trạng thái thiết bị") >= 0 || userMsgLower.indexOf("trạng thái rơ le") >= 0 || userMsgLower.indexOf("các thiết bị đang") >= 0) {
                aiReply = "🔌 Trạng thái thiết bị hiện tại:\n"
                          "- Bơm chính: " + String(sensorManager.getPumpStatus() ? "Đang BẬT 🟢" : "Đang TẮT 🔴") + "\n"
                          "- Đèn chiếu sáng: " + String(monitor.getRelayState(0) ? "Đang BẬT 🟢" : "Đang TẮT 🔴") + "\n"
                          "- Sủi Oxy: " + String(monitor.getRelayState(1) ? "Đang BẬT 🟢" : "Đang TẮT 🔴");
                isHardwareCommand = true;
            }

            // NẾU LÀ CÂU HỎI TƯ VẤN KHÁC -> GỬI LÊN MÁY CHỦ PYTHON
            if (!isHardwareCommand) {
                if (strlen(AI_SERVER_URL) == 0) {
                    aiReply = "ℹ️ Máy chủ AI chưa được cấu hình (AI_SERVER_URL trong secrets.h). Các lệnh điều khiển thiết bị vẫn dùng được.";
                } else if(WiFi.status() == WL_CONNECTED) {
                    WiFiClient client; 
                    HTTPClient http;
                    
                    String fullUrl = String(AI_SERVER_URL) + "/api/chat";
                    http.begin(client, fullUrl); 
                    http.setTimeout(8000); // Không chặn vòng lặp điều khiển quá lâu
                    http.addHeader("Content-Type", "application/json");
                    
                    #if ARDUINOJSON_VERSION_MAJOR >= 7
                        JsonDocument pyReq;
                    #else
                        DynamicJsonDocument pyReq(512);
                    #endif
                    
                    pyReq["user_id"] = AI_USER_ID;
                    pyReq["message"] = userMsg; 
                    
                    DisplayData wqDataChat = monitor.getDisplayData();
                    float curTempChat = (wqDataChat.temps[0] > -50) ? wqDataChat.temps[0] : 0;

                    pyReq["ph"] = pH_value;
                    pyReq["tds"] = wqDataChat.tds;
                    pyReq["temp"] = curTempChat;
                    
                    String pyPayload;
                    serializeJson(pyReq, pyPayload);
                    
                    int httpResponseCode = http.POST(pyPayload);
                    if (httpResponseCode == 200) {
                        String pyResponse = http.getString();
                        #if ARDUINOJSON_VERSION_MAJOR >= 7
                            JsonDocument pyResDoc;
                        #else
                            DynamicJsonDocument pyResDoc(2048);
                        #endif
                        deserializeJson(pyResDoc, pyResponse);
                        
                        aiReply = pyResDoc["reply"].as<String>();
                    } else {
                        aiReply = "⚠️ Lỗi: Không thể kết nối tới máy chủ AI (Mã lỗi: " + String(httpResponseCode) + "). Hãy kiểm tra AI_SERVER_URL trong secrets.h.";
                    }
                    http.end();
                } else {
                    aiReply = "⚠️ Lỗi: Trạm ESP32 đang mất kết nối WiFi!";
                }
            }
            
            // TRẢ KẾT QUẢ VỀ CHO GIAO DIỆN WEB                                                                                                                                 
            #if ARDUINOJSON_VERSION_MAJOR >= 7
                JsonDocument outputDoc;
            #else
                DynamicJsonDocument outputDoc(2048);
            #endif
            
            outputDoc["reply"] = aiReply;
            String json; 
            serializeJson(outputDoc, json);
            mainServer->send(200, "application/json", json);
        }
    }

    void handleGetMaintenance() {
        if (!checkAuth()) return;
        Preferences prefs;
        prefs.begin("maintenance", true); 
        
        #if ARDUINOJSON_VERSION_MAJOR >= 7
            JsonDocument doc;
        #else
            DynamicJsonDocument doc(256);
        #endif
        
        doc["water"] = prefs.getString("water", "Chưa có dữ liệu");
        doc["filter"] = prefs.getString("filter", "Chưa có dữ liệu");
        doc["salt"] = prefs.getString("salt", "Chưa có dữ liệu");
        prefs.end(); 

        String json;
        serializeJson(doc, json);
        mainServer->send(200, "application/json", json);
    }
    
    void handlePostMaintenance() {
        if (!checkAuth()) return;
        
        #if ARDUINOJSON_VERSION_MAJOR >= 7
            JsonDocument doc;
        #else
            DynamicJsonDocument doc(256);
        #endif
        
        DeserializationError error = deserializeJson(doc, mainServer->arg("plain"));
        
        if (error) {
            mainServer->send(400, "application/json", "{\"status\":\"error\"}");
            return;
        }

        String action = doc["action"].as<String>();
        String date = doc["date"].as<String>();

        Preferences prefs;
        prefs.begin("maintenance", false); 
        if (action == "water") prefs.putString("water", date);
        else if (action == "filter") prefs.putString("filter", date);
        else if (action == "salt") prefs.putString("salt", date);
        prefs.end(); 

        mainServer->send(200, "application/json", "{\"status\":\"ok\"}");
    }
    
};

#endif