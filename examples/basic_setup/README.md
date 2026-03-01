# Basic Setup Example

This guide walks you through the simplest possible setup for the ESP32 Presence Detection System.

## What You Need

- 1x ESP32-DevKitC-32E or ESP32-S3 development board
- 1x LD2410C presence sensor
- 5x jumper wires (female-to-female or male-to-female depending on your board)
- USB cable for programming

## Quick Wiring (ESP32-DevKitC-32E)

```
LD2410C  →  ESP32
VCC      →  VIN (5V)
GND      →  GND
TX       →  GPIO18
RX       →  GPIO17
OUT      →  GPIO4
```

## Flash and Configure

1. Open `ESP32Presence/ESP32Presence.ino` in Arduino IDE
2. Select Tools → Board → ESP32 Dev Module
3. Click Upload
4. Connect to the `Presence_XXXX` WiFi network on your phone
5. Enter your WiFi password in the configuration page
6. Set an admin password
7. Click Save

That's it! The LED will:
- Blink blue while in setup mode
- Flash green 3 times when connected to WiFi
- Show green for movement, yellow for static presence, red for no presence

## Next Steps

- Read the full [README](../../README.md) for all features
- Set up EISY/ISY integration for automatic light control
- Access the web interface at `http://presence.local`
