# Wiring Guide

This document provides detailed wiring instructions for connecting the LD2410C presence sensor to supported ESP32 boards.

---

## Table of Contents

- [Overview](#overview)
- [LD2410C Sensor Pinout](#ld2410c-sensor-pinout)
- [ESP32-DevKitC-32E Wiring](#esp32-devkitc-32e-wiring)
- [ESP32-S3 Wiring](#esp32-s3-wiring)
- [Power Requirements](#power-requirements)
- [Custom Pin Configuration](#custom-pin-configuration)
- [Wiring Troubleshooting](#wiring-troubleshooting)

---

## Overview

The LD2410C communicates with the ESP32 using:
- **UART serial** (TX/RX) for configuration and data
- **OUT pin** for digital presence/no-presence signal
- **5V power** supply

The ESP32 controls a **built-in WS2812 RGB LED** for status indication (no external LED required).

---

## LD2410C Sensor Pinout

```
LD2410C Sensor (viewed from front, pins on bottom)
┌─────────────────────────────────────┐
│             LD2410C                 │
│                                     │
│  [Antenna area - do not obstruct]   │
│                                     │
└──┬──┬──┬──┬──┬──┬──────────────────┘
   │  │  │  │  │  │
  VCC TX RX OUT GND (some have additional pins)
```

| Pin Label | Direction | Description |
|-----------|-----------|-------------|
| VCC | Input | 5V power supply |
| GND | Input | Ground |
| TX | Output | UART data from sensor to ESP32 |
| RX | Input | UART data from ESP32 to sensor |
| OUT | Output | Digital presence signal (HIGH = presence) |

> **Note:** Some LD2410C modules have additional pins (IO1, IO2). Leave these unconnected unless you have a specific reason to use them.

---

## ESP32-DevKitC-32E Wiring

### Pin Assignments

| LD2410C | ESP32 Pin | GPIO | Notes |
|---------|-----------|------|-------|
| VCC | VIN | — | 5V from USB power |
| GND | GND | — | Any GND pin |
| TX | RX2 | GPIO18 | ESP32 receives sensor data |
| RX | TX2 | GPIO17 | ESP32 sends commands to sensor |
| OUT | D4 | GPIO4 | Digital presence detection |

The built-in RGB LED is on **GPIO16** — no external LED needed.

### Wiring Diagram (ASCII)

```
ESP32-DevKitC-32E          LD2410C Sensor
┌──────────────────┐       ┌─────────────────┐
│                  │       │                 │
│  VIN ────────────┼───────┼── VCC (5V)      │
│  GND ────────────┼───────┼── GND           │
│  GPIO18 (RX2) ───┼───────┼── TX            │
│  GPIO17 (TX2) ───┼───────┼── RX            │
│  GPIO4  ─────────┼───────┼── OUT           │
│                  │       │                 │
│  GPIO16 → [Built-in RGB LED]               │
│  GPIO0  → [BOOT button - Service/Reset]    │
│                  │       │                 │
└──────────────────┘       └─────────────────┘
```

### Physical Connection Tips

1. Use jumper wires or a small breadboard for prototyping
2. For permanent installation, use a JST connector or solder wires directly
3. Keep wire lengths short (under 30cm) for reliable UART communication
4. Avoid running sensor wires parallel to high-current wires

---

## ESP32-S3 Wiring

### Pin Assignments

| LD2410C | ESP32-S3 Pin | GPIO | Notes |
|---------|--------------|------|-------|
| VCC | VIN | — | 5V from USB power |
| GND | GND | — | Any GND pin |
| TX | RX1 | GPIO17 | ESP32-S3 receives sensor data |
| RX | TX1 | GPIO16 | ESP32-S3 sends commands to sensor |
| OUT | GPIO4 | GPIO4 | Digital presence detection |

The built-in RGB LED is on **GPIO48** — no external LED needed.

> **Note:** Pin assignments differ between ESP32 and ESP32-S3. The firmware auto-detects the board type and uses the correct pins automatically.

### Wiring Diagram (ASCII)

```
ESP32-S3 Dev Board         LD2410C Sensor
┌──────────────────┐       ┌─────────────────┐
│                  │       │                 │
│  VIN ────────────┼───────┼── VCC (5V)      │
│  GND ────────────┼───────┼── GND           │
│  GPIO17 (RX1) ───┼───────┼── TX            │
│  GPIO16 (TX1) ───┼───────┼── RX            │
│  GPIO4  ─────────┼───────┼── OUT           │
│                  │       │                 │
│  GPIO48 → [Built-in RGB LED]               │
│  BOOT   → [BOOT button - Service/Reset]    │
│                  │       │                 │
└──────────────────┘       └─────────────────┘
```

---

### BOOT Button Behavior

- **Short press:** Toggle service mode (LAN web/OTA access, purple blinking LED)
- **Hold 5 seconds:** Factory reset (clears configuration)

---

## Power Requirements

### Current Draw

| Component | Current |
|-----------|---------|
| ESP32 (idle) | ~80 mA |
| ESP32 (WiFi active) | ~160–240 mA |
| LD2410C sensor | ~65 mA |
| RGB LED (white, full brightness) | ~20 mA |
| **Total maximum** | ~325 mA |

### Power Source Options

| Source | Notes |
|--------|-------|
| USB (from computer) | Adequate for development |
| USB power adapter (1A+) | Recommended for permanent installation |
| 5V regulated power supply | Best for permanent/wall-mount installation |

> **Tip:** Use a quality USB cable. Thin or low-quality cables can cause voltage drops that lead to WiFi connectivity issues.

### Voltage Levels

- The LD2410C requires 5V power
- The LD2410C TX/OUT pins output at 3.3V levels, compatible with ESP32
- Do not connect LD2410C pins to 5V-only GPIO pins on ESP32

---

## Custom Pin Configuration

Advanced users can configure custom pins through the web interface under **Settings → Advanced Settings**.

### When to Use Custom Pins

- Connecting additional peripherals that conflict with default pins
- Using a custom PCB or shield
- Specific enclosure constraints

### Pin Validation Rules

The firmware enforces these rules when setting custom pins:
- No two pins may share the same GPIO number
- GPIO0 is reserved for the factory reset button
- GPIO1 and GPIO3 are reserved for Serial debug output
- Strapping pins (GPIO0, GPIO2, GPIO5, GPIO12, GPIO15) have restrictions

### Restoring Default Pins

In the web interface, go to **Settings → Advanced Settings** and click "Restore Default Pins for [Board Type]".

---

## Wiring Troubleshooting

### LED Does Not Light Up

1. Check that GPIO16 (ESP32) or GPIO48 (ESP32-S3) is correct for your board
2. Verify the board is powered (USB connected, power LED on)
3. Check Serial Monitor for any error messages during boot
4. If using custom pins, verify the custom pin configuration in settings

### Sensor Not Detecting Presence

1. Verify VCC is connected to 5V (not 3.3V)
2. Check GND connection
3. Confirm TX/RX wires are not swapped (sensor TX → ESP32 RX)
4. Verify OUT pin is connected to GPIO4
5. Check Serial Monitor — you should see "Presence detected" messages when the sensor is triggered

### UART Communication Issues

1. Ensure sensor TX → ESP32 RX (cross-wired)
2. Verify baud rate is 256000 (the LD2410C default)
3. Keep wire length under 30cm
4. Try a different set of jumper wires (faulty wires are common)

### Intermittent Disconnections

1. Check all connections are secure (not loose in breadboard holes)
2. Measure VCC voltage — should be 4.8–5.2V under load
3. Try a different USB cable or power adapter
4. Add a 100µF capacitor between VCC and GND near the sensor
