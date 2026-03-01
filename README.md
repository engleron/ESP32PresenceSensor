# ESP32Presence

An Arduino IDE sketch for ESP32-based presence detection and automatic light control.  
Supports multiple ESP32 board variants and both PIR and mmWave (LD2410) sensors.

---

## Features

- **Multi-board support** — works with ESP32 DevKit, ESP32-S2, ESP32-S3, ESP32-C3, and WEMOS LOLIN D32 out of the box; easily extended for other boards
- **PIR sensor** *(optional)* — fast, low-cost motion detection; enable with `#define ENABLE_PIR`
- **LD2410 mmWave radar** *(optional)* — detects stationary presence that PIR misses; enable with `#define ENABLE_MMWAVE`
- **Automatic light control** — turns on when presence is detected, turns off after a configurable timeout
- **Direct relay/LED output** — controls `LIGHT_PIN` directly when no ISY controller is configured
- **WiFi captive portal** — on first boot (or after factory reset) the device opens a hotspot (`ESP32-Presence-Setup`) and serves a configuration page for WiFi, sensor timeout, and optional ISY/EISY settings
- **Factory reset** — hold the BOOT button for 3 seconds to clear all settings and re-enter setup mode
- **LED status** — built-in RGB LED shows presence state (green = moving, yellow = static, red = no presence, blue blink = config mode)
- **Serial debugging** — prints board name, pin assignments, active features, and light state to the Serial Monitor

---

## Supported Boards

| Board |
|---|
| ESP32 DevKit (generic) |
| ESP32-S2 |
| ESP32-S3 |
| ESP32-C3 |
| WEMOS LOLIN D32 |

Pin assignments are defined in the **BOARD-SPECIFIC PIN DEFINITIONS** section of the sketch.  
To add another board, insert a new `#elif` block in that section.

---

## Hardware

### Required
- Any supported ESP32 development board

### Optional — PIR motion sensor (e.g. HC-SR501)
- VCC → 3.3 V or 5 V (check your sensor's datasheet)
- GND → GND
- OUT → `PIR_PIN`

### Optional — Relay module or LED for direct light output
- Signal → `LIGHT_PIN`
- Adjust `LIGHT_ON_LEVEL` / `LIGHT_OFF_LEVEL` for active-HIGH or active-LOW relays

### Optional — mmWave radar (HLK-LD2410 / LD2410B / LD2410C)
- VCC → 3.3 V
- GND → GND
- TX  → `RADAR_RX_PIN` (ESP32 receives)
- RX  → `RADAR_TX_PIN` (ESP32 transmits)
- OUT → `RADAR_OUT_PIN` (digital presence detection — always used regardless of `ENABLE_MMWAVE`)

**Example wiring (generic ESP32 DevKit defaults):**
```
LD2410C TX  → GPIO18   (RADAR_RX_PIN)
LD2410C RX  → GPIO17   (RADAR_TX_PIN)
LD2410C OUT → GPIO4    (RADAR_OUT_PIN)
PIR OUT     → GPIO23   (PIR_PIN)
Relay IN    → GPIO26   (LIGHT_PIN)
```

---

## Software Setup

1. Install the **Arduino IDE** (2.x recommended) with ESP32 board support:  
   `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
2. Open `ESP32Presence/ESP32Presence.ino` in the Arduino IDE.
3. Select your board from **Tools → Board**.
4. *(Optional)* To enable the PIR sensor:
   - Uncomment `#define ENABLE_PIR` near the top of the sketch.
5. *(Optional)* To enable the LD2410 mmWave radar UART protocol:
   - Install the **ld2410** library via **Sketch → Include Library → Manage Libraries** (search "ld2410" by ncmreynolds).
   - Uncomment `#define ENABLE_MMWAVE` near the top of the sketch.
6. Adjust `LIGHT_TIMEOUT_MS` (default: 5 minutes) and pin definitions as needed.
7. Upload to your board and open the Serial Monitor at **115200 baud**.

---

## Configuration

### Compile-time constants

| Constant | Default | Description |
|---|---|---|
| `ENABLE_PIR` | *disabled* | Uncomment to enable PIR sensor (`PIR_PIN`) |
| `ENABLE_MMWAVE` | *disabled* | Uncomment to enable LD2410 mmWave UART protocol |
| `LIGHT_TIMEOUT_MS` | `300000` (5 min) | Milliseconds of no presence before light turns off |
| `LIGHT_ON_LEVEL` | `HIGH` | Output level on `LIGHT_PIN` when light is ON |
| `LIGHT_OFF_LEVEL` | `LOW` | Output level on `LIGHT_PIN` when light is OFF |

### Runtime configuration (WiFi portal)

On first boot the device creates an access point **`ESP32-Presence-Setup`** (no password).  
Connect to it with any phone or laptop — you will be automatically redirected to the setup page, or navigate to `http://192.168.4.1` manually.

The portal lets you configure:
- **WiFi network & password** — scans nearby networks and lets you pick one
- **Light-off delay** — 1 / 2 / 3 / 5 / 10 / 15 minutes
- **EISY / ISY / Polisy** controller IP, username, password, and Insteon device address *(optional)*

To re-enter config mode at any time, hold the BOOT button for **3 seconds**. The LED will blink blue while in config mode.

---

## License

MIT
