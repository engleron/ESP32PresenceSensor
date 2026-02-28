# ESP32Presence

An Arduino IDE sketch for ESP32-based presence detection and automatic light control.  
Supports multiple ESP32 board variants and both PIR and mmWave (LD2410) sensors.

---

## Features

- **Multi-board support** — works with ESP32 DevKit, ESP32-S2, ESP32-S3, ESP32-C3, and WEMOS LOLIN D32 out of the box; easily extended for other boards
- **PIR sensor** — fast, low-cost motion detection (always enabled)
- **LD2410 mmWave radar** *(optional)* — detects stationary presence that PIR misses
- **Automatic light control** — turns on when presence is detected, turns off after a configurable timeout
- **Serial debugging** — prints board name, pin assignments, and light state to the Serial Monitor

---

## Supported Boards

| Board | PIR GPIO | Light GPIO | Radar RX | Radar TX |
|---|---|---|---|---|
| ESP32 DevKit (generic) | 15 | 18 | 32 | 33 |
| ESP32-S2 | 10 | 6 | 9 | 8 |
| ESP32-S3 | 4 | 5 | 17 | 18 |
| ESP32-C3 | 2 | 3 | 4 | 5 |
| WEMOS LOLIN D32 / D32 Pro | 34 | 26 | 16 | 17 |

Pin assignments can be changed in the **BOARD-SPECIFIC PIN DEFINITIONS** section of the sketch.  
To add another board, insert a new `#elif` block in that section.

---

## Hardware

### Required
- Any supported ESP32 development board
- PIR motion sensor (e.g. HC-SR501)
  - VCC → 3.3 V or 5 V (check your sensor's datasheet)
  - GND → GND
  - OUT → `PIR_PIN`
- Relay module or LED for light output
  - Signal → `LIGHT_PIN`
  - Adjust `LIGHT_ON_LEVEL` / `LIGHT_OFF_LEVEL` for active-HIGH or active-LOW relays

### Optional (mmWave radar)
- HLK-LD2410 / LD2410B / LD2410C mmWave presence sensor
  - VCC → 3.3 V
  - GND → GND
  - TX  → `RADAR_RX_PIN` (ESP32 receives)
  - RX  → `RADAR_TX_PIN` (ESP32 transmits)

---

## Software Setup

1. Install the **Arduino IDE** (2.x recommended) with ESP32 board support:  
   `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
2. Open `ESP32Presence/ESP32Presence.ino` in the Arduino IDE.
3. Select your board from **Tools → Board**.
4. *(Optional)* To enable the LD2410 mmWave radar:
   - Install the **ld2410** library via **Sketch → Include Library → Manage Libraries** (search "ld2410" by ncmreynolds).
   - Uncomment `#define ENABLE_MMWAVE` near the top of the sketch.
5. Adjust `LIGHT_TIMEOUT_MS` (default: 5 minutes) and pin definitions as needed.
6. Upload to your board and open the Serial Monitor at **115200 baud**.

---

## Configuration

| Constant | Default | Description |
|---|---|---|
| `ENABLE_MMWAVE` | *disabled* | Uncomment to enable LD2410 mmWave radar |
| `LIGHT_TIMEOUT_MS` | `300000` (5 min) | Milliseconds of no presence before light turns off |
| `LIGHT_ON_LEVEL` | `HIGH` | Output level when light is ON |
| `LIGHT_OFF_LEVEL` | `LOW` | Output level when light is OFF |

---

## License

MIT
