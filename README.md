# ESP32 Presence Detection System

An ESP32-based occupancy detection system using the LD2410C mmWave radar sensor that integrates with Universal Devices EISY/ISY/Polisy controllers to automatically control Insteon lighting based on presence detection.

---

## Table of Contents

- [Features](#features)
- [Quick Start (TL;DR)](#quick-start-tldr)
- [Hardware Requirements](#hardware-requirements)
- [Software Requirements](#software-requirements)
- [Wiring](#wiring)
- [Installation](#installation)
- [Project Structure](#project-structure)
- [Compile-Time Configuration](#compile-time-configuration)
- [First-Time Setup](#first-time-setup)
- [LED Status Reference](#led-status-reference)
- [Web Interface](#web-interface)
- [Factory Reset](#factory-reset)
- [EISY / ISY / Polisy Integration](#eisy--isy--polisy-integration)
- [API Reference](#api-reference)
- [FAQ](#faq)
- [Troubleshooting](#troubleshooting)
- [Contributing](#contributing)
- [Changelog](#changelog)
- [License](#license)

---

## Features

- [x] **Automatic presence detection** using LD2410C mmWave radar (detects motion AND static presence)
- [x] **Multi-board support** — ESP32-DevKitC-32E and ESP32-S3 with compile-time auto-detection
- [x] **Unique WiFi AP name** — `Presence_XXXX` (last 4 characters of MAC address)
- [x] **Captive portal** — Opens configuration page automatically on iOS, Android, and Windows
- [x] **Comprehensive help text** — Every setting explained in the setup UI
- [x] **Admin password protection** — Web interface secured with SHA-256 hashed password
- [x] **Session management** — 15-minute timeout with automatic logout
- [x] **Status dashboard** — Live sensor status, uptime, WiFi signal, memory, and more
- [x] **Settings management** — Change all settings via web browser, no re-flashing needed
- [x] **Factory reset** — 5-second BOOT button hold with visual countdown feedback
- [x] **mDNS support** — Access via `http://presence.local`
- [x] **OTA firmware updates** — Update firmware over WiFi
- [x] **Export/import configuration** — JSON format for backup and restore
- [x] **EISY/ISY/Polisy integration** — Automatic light control via REST API (HTTP and HTTPS)
- [x] **Rate limiting** — Brute-force protection on login page
- [x] **Watchdog timer** — Auto-recovery from hangs
- [x] **RGB LED status** — Color-coded feedback for all device states

---

## Quick Start (TL;DR)

1. Flash the firmware to your ESP32 board
2. Connect to the `Presence_XXXX` WiFi network from your phone
3. The configuration page opens automatically — enter your WiFi credentials and settings
4. Set an admin password (required)
5. Done! The device connects to your WiFi and starts detecting presence

---

## Hardware Requirements

### Supported ESP32 Boards

| Board | Status | Notes |
|-------|--------|-------|
| **ESP32-DevKitC-32E** | Tested | Built-in WS2812 LED on GPIO16 |
| **ESP32-S3 Dev Board** | Tested | Built-in WS2812 LED on GPIO48 |

> **Note:** The board is auto-detected at compile time. No manual configuration required.

### LD2410C Presence Sensor

| Specification | Value |
|---------------|-------|
| Supply voltage | 5V |
| Detection range | Up to 5 meters |
| Detection type | Moving + Static presence |
| Communication | UART (256000 baud) |
| Output pin | Digital HIGH/LOW |

**Where to buy:** Search for "LD2410C" on Amazon, AliExpress, or Mouser. Typical price: $5–15 USD.

### Optional: EISY / ISY / Polisy Controller

Required only if you want automatic Insteon light control. Supported models:
- Universal Devices EISY
- Universal Devices Polisy
- Universal Devices ISY994i
- Universal Devices ISY994 (all versions)

---

## Software Requirements

### Arduino IDE

- Arduino IDE **1.8.19** or **2.x** (2.x recommended)
- Download: [https://www.arduino.cc/en/software](https://www.arduino.cc/en/software)

### Required Libraries

Install via Arduino IDE → Sketch → Include Library → Manage Libraries:

| Library | Version | Purpose |
|---------|---------|---------|
| **Adafruit NeoPixel** | 1.11.0+ | RGB LED control |

### ESP32 Board Package

1. Open Arduino IDE → File → Preferences
2. Add this URL to "Additional Boards Manager URLs":
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. Go to Tools → Board → Boards Manager
4. Search for "esp32" and install **esp32 by Espressif Systems** (version 3.3.7 or later recommended)

---

## Wiring

See [docs/WIRING.md](docs/WIRING.md) for detailed wiring instructions.

### ESP32-DevKitC-32E Quick Reference

| LD2410C Pin | ESP32 Pin | Notes |
|-------------|-----------|-------|
| VCC | 5V (VIN) | Use 5V power supply |
| GND | GND | Common ground |
| TX (sensor out) | GPIO18 | ESP32 UART2 RX |
| RX (sensor in) | GPIO17 | ESP32 UART2 TX |
| OUT | GPIO4 | Digital presence signal |

### ESP32-S3 Quick Reference

| LD2410C Pin | ESP32-S3 Pin | Notes |
|-------------|--------------|-------|
| VCC | 5V (VIN) | Use 5V power supply |
| GND | GND | Common ground |
| TX (sensor out) | GPIO17 | ESP32-S3 UART2 RX |
| RX (sensor in) | GPIO16 | ESP32-S3 UART2 TX |
| OUT | GPIO4 | Digital presence signal |

---

## Installation

### 1. Download the Code

Clone or download this repository:
```bash
git clone https://github.com/yourusername/PresenceSensor.git
```

### 2. Open in Arduino IDE

Open `ESP32Presence/ESP32Presence.ino` in Arduino IDE.

### 3. Select Your Board

Go to **Tools → Board** and select:
- For ESP32-DevKitC-32E: `ESP32 Dev Module`
- For ESP32-S3: `ESP32S3 Dev Module`

### 4. Configure Upload Settings

| Setting | Value |
|---------|-------|
| Upload Speed | 921600 |
| Flash Size | 4MB (32Mb) |
| Partition Scheme | Default 4MB with spiffs |
| Core Debug Level | None |

### 5. Flash the Firmware

1. Connect your ESP32 to your computer via USB
2. Select the correct COM port under **Tools → Port**
3. Click **Upload** (or press Ctrl+U)
4. Open Serial Monitor (115200 baud) to see startup messages

> **Note:** During compilation, you will see a warning like `"Compiling for ESP32"` or `"Compiling for ESP32-S3"` — this confirms the board was detected correctly.

---

## Project Structure

Firmware is now organized into focused modules under `ESP32Presence/`:

- `ESP32Presence.ino` — placeholder sketch file kept for Arduino IDE project structure
- `PresenceConfig.h` — central compile-time defaults (pins, timers, ports, firmware version)
- `PresenceState.h/.cpp` — global runtime state and hardware objects
- `PresenceCore.h/.cpp` — utilities, security/session, configuration persistence
- `PresenceIntegrations.h/.cpp` — EISY/ISY, Insteon Hub, and Home Assistant control
- `PresenceWeb.h/.cpp` — setup portal, authenticated UI, and API endpoints
- `PresenceRuntime.h/.cpp` — LED/sensor/reset logic, WiFi setup, runtime loop

`setup()` and `loop()` are implemented in `PresenceRuntime.cpp` to avoid Arduino IDE prototype-generation edge cases.

---

## Compile-Time Configuration

Use [`ESP32Presence/PresenceConfig.h`](ESP32Presence/PresenceConfig.h) as the single source of truth for:

- firmware version
- board default pins
- timing constants (heartbeat, watchdog, session timeout, etc.)
- default ports and feature flags

This file is intended for developer-level defaults. User-level settings (WiFi credentials, integration data, brightness, custom pins) are still changed in the web UI and stored in NVS (`Preferences`).

---

## First-Time Setup

### Step 1: Connect to the Setup Network

After flashing, the device creates a WiFi access point:
- **SSID:** `Presence_XXXX` (where XXXX = last 4 characters of MAC address, e.g., `Presence_A3F2`)
- **Password:** None (open network)
- **LED:** Blinking blue (1 second on, 1 second off)

Connect to this network from your phone, tablet, or laptop.

### Step 2: Configure Your Device

The configuration page opens automatically (captive portal). If it doesn't open automatically, navigate to `http://192.168.4.1` in your browser.

Fill in the following:

**WiFi Settings (Required)**
- Select your home WiFi network from the dropdown
- Enter your WiFi password
- Note: ESP32 only supports **2.4 GHz** WiFi networks

**Detection Settings**
- **Light Off Delay:** How long to wait after no presence before turning lights off (default: 5 minutes)

**Admin Password (Required)**
- Set a secure password to protect the configuration page
- Write it down — you'll need it to change settings later
- If forgotten, perform a factory reset

**EISY / ISY / Polisy Settings (Optional)**
- Only fill this out if you have a Universal Devices controller
- Leave blank to use the sensor without light control

### Step 3: Save and Connect

Click **Save Configuration**. The device will:
1. Save your settings
2. Restart automatically
3. Connect to your WiFi network
4. Flash the LED green 3 times when connected successfully

### Step 4: Access the Web Interface

Find the device's IP address in your Serial Monitor output, or navigate to `http://presence.local` if mDNS is working on your network.

---

## LED Status Reference

| LED Color | Pattern | Meaning |
|-----------|---------|---------|
| Blue | Blinking (1s on/1s off) | Setup mode — waiting for configuration |
| Green | Solid | Movement detected |
| Yellow | Solid | Static presence detected (person sitting still) |
| Red | Solid | No presence detected |
| Red/Blue | Alternating fast blink | Error condition |
| Purple | 3 quick flashes | Factory reset confirmed |

---

## Web Interface

After initial setup, access the web interface at:
- **IP Address:** `http://192.168.1.XXX` (shown in Serial Monitor)
- **mDNS:** `http://presence.local`

### Pages

| Page | URL | Description |
|------|-----|-------------|
| Login | `/login` | Enter admin password |
| Status Dashboard | `/` | Live sensor status, uptime, WiFi info |
| Settings | `/config` | Change all configuration |
| Factory Reset | `/reset` | Reset device to factory defaults |
| Logout | `/logout` | End your session |

### API Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/status` | GET | Current status as JSON (no auth) |
| `/api/config` | POST | Update configuration (auth required) |
| `/api/login` | POST | Authenticate, returns session cookie |
| `/api/logout` | POST | Invalidate session |

See [docs/CONFIGURATION.md](docs/CONFIGURATION.md) for detailed web interface usage.

---

## Factory Reset

To reset all settings and return to setup mode:

1. Locate the **BOOT** button on your ESP32 board
2. Press and **hold** the BOOT button for **5 seconds**
3. Watch the LED feedback:
   - **0–2 seconds:** LED turns solid red
   - **2–4 seconds:** LED blinks red/blue slowly
   - **4–5 seconds:** LED blinks red/blue rapidly
4. Release the button after 5 seconds
5. LED flashes **purple 3 times** to confirm reset
6. Device reboots into setup mode (blue blinking LED)

> **Warning:** Factory reset erases ALL settings including WiFi credentials, admin password, and EISY/ISY configuration.

---

## EISY / ISY / Polisy Integration

The device can automatically control an Insteon light through your Universal Devices controller.

### Setup

1. In the configuration page, enter your controller's IP address
2. Enter your admin username and password
3. Enter the Insteon device address of the light to control

### Finding Your Insteon Device Address

1. Open the EISY/ISY Admin Console
2. Right-click on the light device in the device tree
3. Select **Copy → Address**
4. The address format is: `1A.2B.3C`

### HTTP vs HTTPS

| Controller | Protocol |
|------------|----------|
| EISY | HTTPS (check "Use HTTPS") |
| Polisy | HTTP |
| ISY994i | HTTP |
| ISY994 | HTTP |

### How It Works

- Light turns **ON** when presence is detected
- Light turns **OFF** after no presence for the configured timeout period
- Uses REST API: `/rest/nodes/{address}/cmd/DON` and `/rest/nodes/{address}/cmd/DOF`

---

## FAQ

**Q: Why doesn't the captive portal open automatically?**
A: Try navigating to `http://192.168.4.1` manually. Some browsers or Android versions may block automatic redirects.

**Q: Can I use 5 GHz WiFi?**
A: No. The ESP32 only supports 2.4 GHz WiFi networks.

**Q: The sensor detects presence even when the room is empty. What's wrong?**
A: The LD2410C is very sensitive. Try adjusting the sensitivity settings using the LD2410C configuration tool, or move the sensor to a better location.

**Q: I forgot my admin password. What do I do?**
A: Perform a factory reset (hold BOOT button for 5 seconds). This erases all settings and you'll need to reconfigure the device.

**Q: Can I use this without an EISY/ISY controller?**
A: Yes! Leave the EISY/ISY settings blank. The device still detects presence and shows status on the LED and web interface.

**Q: How do I update the firmware?**
A: Use the OTA update feature in the Settings page, or re-flash via USB cable.

**Q: Why does my board show as "ESP32-DevKitC-32E" but it's actually an S3?**
A: Make sure you selected the correct board in Arduino IDE under Tools → Board before compiling.

---

## Troubleshooting

See [docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md) for detailed troubleshooting guides.

**Common issues:**

- **LED not working** → Check GPIO pin matches your board type
- **Can't connect to setup WiFi** → Look for `Presence_XXXX` network, ensure 2.4 GHz band
- **Can't access web interface** → Try IP address directly, check firewall settings
- **EISY not responding** → Check IP address, credentials, and HTTP vs HTTPS setting
- **Compile error** → Ensure you have the correct board selected and libraries installed

---

## Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/my-feature`
3. Commit your changes with clear messages
4. Push to your fork and open a Pull Request

### Reporting Issues

Please include:
- Board type (ESP32-DevKitC-32E or ESP32-S3)
- Firmware version
- Arduino IDE version
- Serial Monitor output
- Description of the problem and steps to reproduce

### Code Style

- Indentation: 2 spaces
- Variable names: camelCase
- Constants: UPPER_SNAKE_CASE
- Functions: camelCase
- Max line length: 100 characters
- Use `F()` macro for string literals in Serial.print

---

## Changelog

See [CHANGELOG.md](CHANGELOG.md) for full version history.

**Current version: v2.2.0** — Modular `.h/.cpp` architecture, centralized compile-time configuration, and documentation refresh.

---

## License

MIT License — see [LICENSE](LICENSE) for details.

---

## Credits

- **LD2410C sensor** — Hi-Link Technology
- **Universal Devices** — EISY/ISY/Polisy controllers
- **Adafruit NeoPixel** library
- **ESP32 Arduino core** — Espressif Systems
