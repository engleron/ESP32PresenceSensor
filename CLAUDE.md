# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

An ESP32 Arduino firmware project for occupancy detection using the LD2410C mmWave radar sensor. Supports multiple home automation integrations (EISY/ISY/Polisy, Insteon Hub 2, Home Assistant, and optionally native HomeKit) for automatic light control. The firmware is modularized into `.h/.cpp` units under `ESP32Presence/`.

## Build & Flash

This is an **Arduino IDE project** — there is no CLI build system (no `make`, `cmake`, or PlatformIO). All building and flashing is done via Arduino IDE.

**Board selection (required before compile):**
- ESP32-DevKitC-32E → `Tools → Board → ESP32 Dev Module`
- ESP32-S3 → `Tools → Board → ESP32S3 Dev Module`

**Required libraries (install via Library Manager):**
- Adafruit NeoPixel
- (Optional) arduino-homekit-esp32 by Mixiaoxiao — only needed when `#define ENABLE_HOMEKIT` is uncommented

**Required board package:** esp32 by Espressif Systems (add URL to Boards Manager):
```
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```

**Upload settings:** Speed 921600, Flash 4MB, Partition: Default 4MB with spiffs

**Compile verification:** A `#warning` message confirms board detection — `"Compiling for ESP32"` or `"Compiling for ESP32-S3"`.

**Serial Monitor:** 115200 baud for debug output.

## Architecture

Firmware modules live in `ESP32Presence/`:

- `ESP32Presence.ino` — thin Arduino entrypoint that calls `appSetup()` / `appLoop()`
- `PresenceConfig.h` — compile-time defaults (firmware version, board pin defaults, timing constants, default ports)
- `PresenceState.h/.cpp` — global runtime state + hardware objects (`strip`, `server`, `preferences`, etc.)
- `PresenceCore.h/.cpp` — utility helpers, session/auth logic, config load/save/clear/export
- `PresenceIntegrations.h/.cpp` — EISY/ISY, Insteon Hub 2, Home Assistant control paths
- `PresenceWeb.h/.cpp` — setup portal, authenticated web pages, JSON API routes
- `PresenceRuntime.h/.cpp` — LED state logic, sensor polling, reset handling, WiFi mode control, runtime loop

Runtime settings remain in NVS via `Preferences`; compile-time defaults are intentionally centralized in `PresenceConfig.h`.

### Board-Specific Pin Assignments

| Pin | ESP32-DevKitC-32E | ESP32-S3 |
|-----|-------------------|----------|
| Sensor OUT | GPIO4 | GPIO4 |
| Sensor TX (UART2 RX) | GPIO18 | GPIO17 |
| Sensor RX (UART2 TX) | GPIO17 | GPIO16 |
| RGB LED (WS2812) | GPIO16 | GPIO48 |
| Factory reset button | GPIO0 (BOOT) | GPIO0 (BOOT) |

### Key Behaviors

- **Setup mode** — triggered when no WiFi SSID is saved; device acts as AP (`Presence_XXXX`) with captive portal DNS on port 53
- **Integration selection** — dropdown in setup/settings UI selects active integration (`none` | `isy` | `insteon_hub` | `ha` | `homekit`); only one active at a time; stored as NVS key `int_mode`
- **Migration** — existing devices with ISY configured auto-migrate to `integrationMode = "isy"` on first load (no factory reset needed)
- **Session management** — single-session model, 15-minute timeout, SHA-256 hashed password stored via `Preferences` (ESP32 NVS)
- **Factory reset** — hold BOOT (GPIO0) for 5 seconds; LED gives staged visual feedback; clears all `Preferences` keys
- **OTA updates** — ArduinoOTA enabled after WiFi connect
- **mDNS** — hostname `presence.local`
- **Watchdog** — `esp_task_wdt` with 8-second timeout
- **HomeKit** — compile-time only; uncomment `#define ENABLE_HOMEKIT` and install arduino-homekit-esp32 library

### Storage

All persistent settings stored via `Preferences` (ESP32 NVS flash). No filesystem (SPIFFS/LittleFS) is used.

## Code Style

- Indentation: 2 spaces
- Variable names: camelCase
- Constants: UPPER_SNAKE_CASE
- Functions: camelCase
- Max line length: 100 characters
- Use `F()` macro for string literals in `Serial.print`
