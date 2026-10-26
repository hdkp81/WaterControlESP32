# HDKP AI Smart Aqua Monitor (ESP32)

**ESP32-Based Aquarium Water Quality Monitoring and Control System**

[English](README.md) | [Tiếng Việt](README.vi.md)

<p align="center">
  <img src="banner.png" alt="HDKP AI Smart Aqua Monitor (ESP32) Overview" width="100%">
</p>

**Version:** 1.8.6  
**Project:** HDKP AI Smart Aqua Monitor (ESP32)  
**License:** MIT  
**Copyright:** © 2026 HDKP

HDKP AI Smart Aqua Monitor is an ESP32-based firmware project for monitoring and controlling aquarium and aquatic-system water quality. It supports pH, TDS, and temperature monitoring; automatic pump and relay control; an LCD and web dashboard; MQTT and ThingSpeak telemetry; and optional AI chatbot and MCP integration with XiaoZhi.

Version 1.8.6 is described as a security and reliability patch release. See [CHANGELOG.md](CHANGELOG.md) for version history.

> **Safety warning:** This project controls electrical equipment and pumps. Use properly rated power supplies, relays, fuses, and electrical protection. Test with a dummy load before connecting real equipment. The software is provided "AS IS" without warranty. See [LICENSE](LICENSE).

---

## Table of Contents

- [Features](#features)
- [Hardware and Pin Mapping](#hardware-and-pin-mapping)
- [Required Libraries](#required-libraries)
- [Quick Start](#quick-start)
- [Configuration: secrets.h](#configuration-secretsh)
- [Pump and Relay Control](#pump-and-relay-control)
- [Serial Commands](#serial-commands)
- [Web API](#web-api)
- [MQTT Integration](#mqtt-integration)
- [Security Considerations](#security-considerations)
- [Known Limitations](#known-limitations)
- [Project Structure](#project-structure)
- [Troubleshooting](#troubleshooting)
- [Contributing](#contributing)
- [License](#license)

---

## Features

### 1. Water Quality Monitoring

- pH measurement with median and low-pass filtering.
- TDS measurement with temperature compensation support.
- Temperature monitoring using up to three DS18B20 sensors.
- Lightweight on-device analysis of pH/TDS trends, risk levels, and estimated time to reach configured thresholds.
- LCD 16x2 display for sensor readings and alerts.
- LED indicators for system status and water-quality warnings.

### 2. Automatic Control

- Automatic pump control based on pH thresholds.
- Temperature-based relay control according to the configured sensor mapping.
- Auto and Manual operating modes.
- Configurable thresholds and hysteresis to reduce frequent relay switching.
- Software safeguards for pump operation in Auto mode, subject to the firmware implementation.

**Important:** Software safeguards do not replace independent electrical protection, emergency shutoff mechanisms, or appropriate equipment supervision.

### 3. Web Dashboard

- Browser-based dashboard for sensor readings and device status.
- Password-protected access.
- Pump and relay control.
- Auto/Manual mode selection.
- Configuration of pH, TDS, and temperature thresholds.
- Maintenance log management.
- Vietnamese and English interface support.
- Chatbot supporting Vietnamese commands and optional AI-backed question answering.

### 4. Connectivity and Integration

- Wi-Fi connectivity and captive portal for network setup.
- MQTT telemetry and alerts.
- ThingSpeak data reporting.
- OTA firmware updates, subject to configuration.
- MCP tools for XiaoZhi integration.
- Optional FastAPI backend for chatbot and maintenance-log services.

---

## Hardware and Pin Mapping

The following table reflects the project configuration described in the documentation. Verify all pin assignments against the actual firmware and hardware before wiring.

| Function | ESP32 GPIO | Notes |
|---|---|---|
| DS18B20 OneWire bus | GPIO 4 | Typically requires a 4.7 kΩ pull-up resistor |
| TDS sensor | GPIO 36 (ADC1) | Verify the sensor module's output voltage |
| pH sensor | GPIO 34 (ADC1) | Firmware may apply a voltage multiplier of ×1.5; verify against the actual circuit |
| Pump relay | GPIO 5 | Active-LOW; GPIO 5 is a strapping pin on classic ESP32 variants |
| Relay 0 / 1 / 2 | GPIO 32 / 33 / 25 | Verify activation levels in firmware |
| TDS / temperature warning LEDs | GPIO 2 / 26 | GPIO 2 may affect boot behavior on some boards |
| Other declared LEDs | GPIO 27 / 14 | Verify their actual usage in firmware |
| LCD 16x2 I²C | SDA 21 / SCL 22 | Common I²C address: `0x27` |

### Hardware Notes

- ESP32 GPIO operates at 3.3 V logic. Do not exceed the voltage limits of GPIO or ADC inputs.
- Check the output voltage of the pH and TDS modules before connecting them to the ESP32.
- Calibrate the pH sensor with appropriate reference solutions.
- Calibrate the TDS sensor against a suitable reference solution and the actual probe.
- Confirm whether each relay is Active-LOW or Active-HIGH.
- GPIO 5 and GPIO 2 can affect boot configuration on some ESP32 variants. Test the complete circuit during startup.
- Use appropriately rated power supplies, relays, fuses, wiring, and electrical enclosures.

### Relay Mapping Warning

The firmware may automatically control temperature-related relays according to their corresponding temperature sensors, while the web dashboard or chatbot may label some relays as "Light" or "Oxygen."

If a light or air pump is connected to a relay that is automatically controlled by temperature, the device may switch on or off unexpectedly.

Verify the relay mapping in the firmware, dashboard, and actual wiring before enabling automatic control.

---

## Required Libraries

Install the dependencies through the Arduino IDE Library Manager or their official project sources.

- **ESP32 Arduino Core:** Wi-Fi, WebServer, Preferences, HTTPClient, ArduinoOTA, and related components.
- `OneWire`
- `DallasTemperature`
- `ArduinoJson` — use a version compatible with the firmware; the project is intended to support versions in the 6.x and 7.x series.
- `PubSubClient`
- `LiquidCrystal_I2C`
- `WebSocketMCP` — MCP integration used for XiaoZhi.

Library versions can affect compilation and runtime behavior. Record the exact versions tested for each official release.

Third-party libraries may have their own licenses. See [License](#license).

---

## Quick Start

### Step 1: Prepare the Development Environment

1. Install Arduino IDE.
2. Install an ESP32 Arduino Core version compatible with the project.
3. Download or clone this repository.
4. Install the required libraries.

### Step 2: Open the Firmware

Open:

`WaterControlESP32v1.8.6.ino`

For the Arduino IDE, the sketch directory should normally have the same name as the `.ino` file.

### Step 3: Create a Private Configuration File

1. Copy `secrets.example.h` to `secrets.h`.
2. Configure the options you need.
3. Do not commit `secrets.h` containing real credentials to GitHub.
4. Verify that `.gitignore` excludes the private configuration file.

### Step 4: Select the Board and Upload

1. Select the correct ESP32 board.
2. Select the serial port.
3. Verify the GPIO assignments and power supply.
4. Compile and upload the firmware.
5. Open Serial Monitor at 115200 baud.

### Step 5: Configure Wi-Fi

If no Wi-Fi configuration is available at startup, the firmware may start an access point for initial network setup.

According to the documented configuration:

- Access point SSID: `ESP32-HD7-Ver1.1`.
- The AP password may be generated automatically.
- Configuration may be stored in NVS.
- After Wi-Fi connects, the ESP32 IP address can be used to access the dashboard.

Use the actual SSID, password, and IP address shown by the device or your router. These values may differ between installations.

### Step 6: Log In and Change the Password

If the firmware generates a temporary web password, retrieve it through the documented provisioning channel. Change temporary or default credentials immediately after the first login.

Do not expose the ESP32's HTTP port directly to the public Internet.

### Step 7: Configure Optional Services

- **ThingSpeak:** Configure the Write API Key if required.
- **MQTT:** Configure the broker and authentication details.
- **MCP:** Configure the token required by the MCP server.
- **AI chatbot:** Configure the FastAPI server URL if required.

---

## Configuration: `secrets.h`

The following settings reflect the documented project configuration. Verify macro names and actual defaults in `secrets.example.h` for the firmware version being used.

| Macro | Documented Default | Purpose |
|---|---|---|
| `MQTT_HOST` | `broker.emqx.io` | MQTT broker hostname |
| `MQTT_PORT` | `8883` | Common MQTT-over-TLS port |
| `MQTT_USER` / `MQTT_PASS` | Empty | Broker authentication credentials |
| `MQTT_DEVICE_ID` | `aqua_<MAC>` | Device identifier used in MQTT topics |
| `MQTT_CA_CERT` | Not configured | CA certificate for TLS server verification |
| `MQTT_REMOTE_CONTROL` | `0` | Set to `1` to enable remote pump control if supported by the firmware |
| `OTA_PASSWORD` | Empty | OTA password; documented configuration requires at least 8 characters |
| `AI_SERVER_URL` | Empty | FastAPI server URL, e.g. `http://192.168.1.90:8000` |
| `AI_USER_ID` | `web_user` | User ID sent with chatbot requests |
| `AP_PASSWORD` | Automatically generated | Optional fixed AP password if supported by the firmware |

### Configuration Security

- Do not use a public MQTT broker for controlling real equipment without appropriate authentication and access restrictions.
- Never publish Wi-Fi passwords, ThingSpeak keys, MCP tokens, or OTA passwords.
- Do not commit real credentials to source control or include them in screenshots and release logs.
- Revoke and replace any credentials that have been exposed.
- A secret stored in `secrets.h` is compiled into firmware and should not be considered permanently protected.

---

## Pump and Relay Control

### pH-Based Pump Control

According to the current project documentation:

- In Auto mode, the pump turns on when pH exceeds the configured threshold.
- The pump turns off when pH falls below the threshold minus a hysteresis value of 0.5.
- A sensor fault is documented to trigger pump shutdown.
- Maximum Auto-mode pump runtime: 30 minutes.
- Rest period after the runtime limit: 10 minutes.
- Manual mode may not apply the same runtime limit as Auto mode.

Verify these behaviors in the firmware and test them on the actual hardware before deployment. Do not rely on pH-based control alone to protect aquatic life.

### Temperature-Based Relay Control

Relays may be controlled according to their corresponding temperature sensors. Switching thresholds, control direction, and hysteresis depend on the firmware and configuration.

Before connecting heaters, lights, air pumps, or other electrical loads, confirm the relay logic and switching conditions.

---

## Serial Commands

The documented Serial Monitor baud rate is 115200.

| Command | Purpose |
|---|---|
| `HELP` | Display command help |
| `CALTDS` | Calibrate TDS |
| `CALTEMP S0 MIN <value>` | Configure the minimum threshold for sensor S0, if supported |
| `CALTEMP S0 MAX <value>` | Configure the maximum threshold for sensor S0, if supported |
| `SETKEY <ThingSpeakKey>` | Configure the ThingSpeak key |
| `SETMCP <token>` | Configure the MCP token |
| `SETNUMTEMP <1..3>` | Configure the number of temperature sensors |
| `SAVE` | Save pending configuration changes |
| `CANCEL` | Cancel pending changes, if supported |
| `RESET` | Reset configuration according to the firmware implementation |

Verify the exact syntax and supported command variants against the Serial command handler in the firmware.

**Warning:** `RESET` may erase configuration and credentials. Use it only after reviewing the firmware behavior and preparing to configure the device again.

---

## Web API

The following endpoints are documented for the project. Verify the HTTP methods, request schemas, and access controls against the actual implementation before integrating external clients.

| Method | Endpoint | Purpose |
|---|---|---|
| `GET` | `/` | Dashboard |
| `GET` | `/api/data` | Current sensor readings and status |
| `POST` | `/api/control/relay` | Control a relay |
| `POST` | `/api/control/mode` | Switch between Auto and Manual |
| `POST` | `/api/settings/ph_threshold` | Configure the pH threshold |
| `GET` / `POST` | `/api/sysconfig` | Read or update system configuration |
| `POST` | `/api/sysconfig/reset` | Reset configuration |
| `POST` | `/api/settings/credentials` | Change login credentials |
| `POST` | `/api/ai/chat` | Chatbot and Vietnamese commands |
| `GET` / `POST` | `/api/maintenance` | Read or update maintenance logs |

Example relay-control payload based on the documented API:

```json
{
  "relay": 0,
  "status": true
}

---
## Contributing

Contributions that improve the project's stability, security, usability, and documentation are welcome.

Before submitting a Pull Request:

1. Clearly describe the problem and how to reproduce it.
2. Explain the changes made.
3. Verify compilation and test on suitable hardware.
4. Do not include passwords, API keys, MCP tokens, or personal information in commits.
5. Acknowledge relevant third-party libraries and components.
6. Update the README or CHANGELOG if changes affect users.

---

## License

Copyright (c) 2026 HDKP 

This project is licensed under the **MIT License**. You may use, copy, modify, distribute, and integrate the source code into commercial products subject to the terms in the [LICENSE](LICENSE) file.

The MIT License requires the copyright notice and permission notice to be included in all copies or substantial portions of the software, as specified by its terms.

Third-party libraries, resources, and components remain subject to their respective licenses. Licensing this project does not automatically change the licenses of those components.

---

**HDKP AI Smart Aqua Monitor (ESP32) — v1.8.6**

Developed by **HDKP**  
Contact: `dinhbui3381@gmail.com`