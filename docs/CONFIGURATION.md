# Configuration Guide

This document provides a detailed walkthrough of all configuration options for the ESP32 Presence Detection System.

---

## Table of Contents

- [First-Time Setup](#first-time-setup)
- [Compile-Time Defaults](#compile-time-defaults)
- [WiFi Configuration](#wifi-configuration)
- [Detection Settings](#detection-settings)
- [Admin Password](#admin-password)
- [Integration Mode](#integration-mode)
- [EISY / ISY / Polisy Integration](#eisy--isy--polisy-integration)
- [Insteon Hub 2 Integration](#insteon-hub-2-integration)
- [Home Assistant Integration](#home-assistant-integration)
- [HomeKit Integration](#homekit-integration)
- [Advanced Settings](#advanced-settings)
- [Web Interface Usage](#web-interface-usage)
- [Security Best Practices](#security-best-practices)
- [Changing Settings After Setup](#changing-settings-after-setup)
- [Export and Import Configuration](#export-and-import-configuration)

---

## First-Time Setup

When the device has no saved configuration, it automatically enters **Setup Mode**:

1. **Blue LED blinks** (1 second on, 1 second off)
2. Device creates a WiFi access point:
   - **SSID:** `Presence_XXXX` (where XXXX = unique chip MAC suffix, e.g., `Presence_A3F2`)
   - **Password:** None (open network)
3. Connect to this network from your phone, tablet, or laptop
4. The configuration page may open automatically (captive portal)
   - If it does not, navigate to `http://192.168.4.1`

---

## Compile-Time Defaults

Runtime configuration in the web UI is for deployed devices.  
Developer defaults are centralized in:

- [`ESP32Presence/PresenceConfig.h`](../ESP32Presence/PresenceConfig.h)

This is the recommended place to adjust:

- firmware version (`FIRMWARE_VERSION`)
- board default pins (`SENSOR_*`, `RGB_LED_PIN`, `PIN_RESET`)
- timeout and cadence defaults (`SESSION_TIMEOUT_MS`, `WDT_TIMEOUT_SECONDS`, etc.)
- default service ports (`DEFAULT_INSTEON_HUB_PORT`, `DEFAULT_HA_PORT`)
- optional compile-time features (`ENABLE_HOMEKIT`)
- run-mode web service default (`ENABLE_RUNMODE_WEB_OTA_DEFAULT`)

Use this file when you want repository-level default behavior for all future flashes.  
Use the web settings page when you want per-device behavior stored in NVS.

---

## WiFi Configuration

### Network Selection

The setup page displays a list of nearby WiFi networks sorted by signal strength. Select your home network from the dropdown.

**Important:** The ESP32 **only supports 2.4 GHz WiFi**. If your router broadcasts separate 2.4 GHz and 5 GHz networks, select the 2.4 GHz one (often labeled with "2.4G" or "2GHz").

### WiFi Password

Enter your WiFi network password. This is stored encrypted in the ESP32's flash memory.

**Tip:** If you have trouble typing your password, use your phone's WiFi password sharing feature:
- **iPhone:** Long-press the WiFi network in Settings → WiFi, then tap "Share Password"
- **Android:** Go to Settings → WiFi → tap your network → tap the QR code icon

### Hidden Networks

If your network doesn't appear in the dropdown (e.g., hidden SSID), scroll to the bottom of the dropdown and select "Enter manually..." then type the SSID.

---

## Detection Settings

### Light Off Delay

How long to wait after no presence is detected before turning off the light.

| Setting | Best For |
|---------|----------|
| 1 minute | High-traffic areas (hallways, bathrooms) |
| 2–3 minutes | General rooms |
| 5 minutes (default) | Living rooms, offices |
| 10–15 minutes | Rooms where you sit still for long periods |

**LED behavior in run mode:**

| LED | Meaning |
|-----|---------|
| Green (solid) | Presence detected |
| Yellow (solid) | No presence — light still on, timeout counting down |
| Red (pulsing, accelerating) | No presence — final-minute warning before auto-off |
| Red (solid) | No presence — light is off |

---

## Admin Password

Setting an admin password is **required** during initial setup. This password protects the configuration page so unauthorized users cannot change your settings.

### Requirements

- Minimum 4 characters
- Maximum 64 characters
- Any characters allowed

### Storage

Passwords are hashed using **SHA-256** before storage. The plain-text password is never stored on the device.

### If You Forget Your Password

There is no password recovery option. You must perform a factory reset:
1. Hold the BOOT button for 5 seconds
2. Reconfigure the device from scratch

**Tip:** Write your password down and store it with your device's physical location.

---

## Integration Mode

The firmware supports multiple home automation integrations. Use the **Integration Mode** dropdown in the setup or settings page to select which one is active. Only one integration can be active at a time.

| Mode | Description |
|------|-------------|
| `none` | Sensor-only mode — no light control; LED and web dashboard still function |
| `isy` | EISY / ISY / Polisy REST API |
| `insteon_hub` | Insteon Hub 2 (Model 2245) direct HTTP control |
| `ha` | Home Assistant REST API |
| `homekit` | Native HomeKit (compile-time only; see [HomeKit Integration](#homekit-integration)) |

The selected mode is stored in NVS under the key `int_mode`. Changing the mode takes effect immediately after saving — no reboot required.

> **Auto-migration:** If you upgraded from a version before v2.1.0 with ISY settings already saved, the device automatically sets `int_mode = isy` on first boot so light control continues without a factory reset.

---

## EISY / ISY / Polisy Integration

This section is **optional**. Skip it if you only want presence detection without light control.

### Supported Controllers

| Controller | Protocol | Notes |
|------------|----------|-------|
| Universal Devices EISY | HTTPS | Check "Use HTTPS" |
| Universal Devices Polisy | HTTP | Leave "Use HTTPS" unchecked |
| Universal Devices ISY994i | HTTP | Leave "Use HTTPS" unchecked |
| Universal Devices ISY994 | HTTP | Leave "Use HTTPS" unchecked |

### Controller IP Address

Enter the IP address of your EISY/ISY/Polisy controller (e.g., `192.168.1.100`).

**Finding the IP address:**
1. Open the EISY/ISY Admin Console (web browser or Java application)
2. Go to **Configuration → System**
3. The IP address is shown in the network information section

**Tip:** Set a static/reserved IP for your controller in your router's DHCP settings so it doesn't change.

### Use HTTPS

- **EISY:** Check this box — EISY requires HTTPS
- **Polisy, ISY994:** Leave unchecked — these use HTTP only

### Username and Password

Enter your controller's admin credentials. These are the same credentials you use to log into the Admin Console.

Default credentials for most Universal Devices controllers:
- Username: `admin`
- Password: `admin` (change this if you haven't already!)

### Insteon Device Address

The address of the Insteon light switch or device you want to control.

**Finding the device address:**
1. Open the EISY/ISY Admin Console
2. Find your device in the left panel (e.g., "Living Room Light")
3. **Right-click** the device
4. Select **Copy → Address**
5. The address format is: `1A.2B.3C` (three hex pairs separated by dots)

### How Light Control Works

| Sensor State | Light Action |
|--------------|--------------|
| Presence detected | Light turns ON (`DON` command) |
| No presence detected | Start countdown timer |
| Timer expires (Light Off Delay) | Light turns OFF (`DOF` command) |

The REST API endpoint used: `/rest/nodes/{address}/cmd/{DON|DOF}`

### Test Connection

After saving settings, go to the Settings page and click **Test Connection** to verify the EISY/ISY connection is working.

---

## Insteon Hub 2 Integration

Select `insteon_hub` as the Integration Mode to control Insteon devices directly via an Insteon Hub 2 (Model 2245) on your local network. No EISY/ISY controller is required.

### Required Settings

| Setting | Description |
|---------|-------------|
| **Hub IP Address** | IP address of your Insteon Hub 2 (e.g., `192.168.1.25`) |
| **Hub Port** | Default: `25105` |
| **Hub Username** | Hub admin username (found on the Hub label) |
| **Hub Password** | Hub admin password (found on the Hub label) |
| **Insteon Device Address** | Address of the Insteon device to control (format: `1A.2B.3C`) |

### How It Works

Commands are sent via HTTP GET to the Hub's local API using the `cmd1+cmd2` payload format:
- ON: `0262{hex}0F11FF=I=3`
- OFF: `0262{hex}0F1300=I=3`

Commands execute off the main sensor loop via a FreeRTOS worker task to prevent blocking. If a command fails, it retries automatically after a short delay.

---

## Home Assistant Integration

Select `ha` as the Integration Mode to control a Home Assistant entity via the HA REST API.

### Required Settings

| Setting | Description |
|---------|-------------|
| **HA IP / Hostname** | IP or hostname of your Home Assistant instance |
| **Port** | Default: `8123` |
| **Use HTTPS** | Check if your HA instance uses HTTPS |
| **Long-Lived Access Token** | Bearer token generated in HA (Profile → Long-Lived Access Tokens) |
| **Entity ID** | The entity to control or update (e.g., `light.living_room` or `sensor.office_presence`) |
| **HA Mode** | `light_control` (default) or `sensor_entity` — see below |
| **Sensor Data Source** | `out_pin` or `uart` — only shown when HA Mode is `sensor_entity` |

### HA Mode: Light Control (default)

The default mode. Controls a Home Assistant `light.*` or `switch.*` entity based on presence:

- Presence detected → POST to `/api/services/homeassistant/turn_on` with `{"entity_id": "..."}`
- Timeout expires → POST to `/api/services/homeassistant/turn_off`

### HA Mode: Sensor Entity

Instead of controlling a light, this mode creates or updates a `sensor.*` entity in Home Assistant with presence state values. Use this when you want to build your own automations in HA based on the sensor's state.

State is pushed via HTTP POST to `/api/states/{entity_id}`.

#### Sensor Data Source: OUT Pin (`out_pin`)

Reports binary presence via the GPIO OUT pin:

| State value | Meaning |
|-------------|---------|
| `detected` | OUT pin is HIGH (presence detected) |
| `clear` | OUT pin is LOW (no presence) |

#### Sensor Data Source: UART (`uart`)

Reports richer presence detail from the LD2410C UART data stream:

| State value | Meaning |
|-------------|---------|
| `movement` | Moving target only |
| `stationary` | Stationary target only |
| `presence` | Both moving and stationary target (or OUT-pin fallback when UART is stale) |
| `clear` | No target detected |

When UART data is selected but no valid UART frames have been received within the last 3 seconds, the firmware falls back to the OUT pin and reports `presence` or `clear` to keep the state vocabulary consistent.

Additional attributes are included in the HA state payload when UART data is fresh:
- `moving_distance_cm`, `moving_energy` (0–100)
- `stationary_distance_cm`, `stationary_energy` (0–100)
- `detection_distance_cm`

> **Note:** Wire TX/RX from the LD2410C sensor to the GPIO pins shown in the board pin table
> (default: GPIO17/GPIO18 on ESP32, GPIO16/GPIO17 on ESP32-S3) to use UART mode.

### Supports HTTP and HTTPS

Both modes (light control and sensor entity) support HTTP and HTTPS. When HTTPS is selected, certificate verification is skipped (`setInsecure()`) to support self-signed certificates common in local HA installs.

### Generating a Long-Lived Access Token

1. Log into Home Assistant
2. Click your profile icon (bottom-left)
3. Scroll to **Long-Lived Access Tokens**
4. Click **Create Token**, give it a name (e.g., `PresenceSensor`)
5. Copy the token and paste it into the HA Token field

---

## HomeKit Integration

HomeKit support is **compile-time only** and disabled by default.

### Enabling HomeKit

1. Install the `arduino-homekit-esp32` library by Mixiaoxiao via Arduino Library Manager
2. Open `ESP32Presence/PresenceConfig.h`
3. Uncomment `#define ENABLE_HOMEKIT`
4. Select `homekit` as the Integration Mode in the web UI after flashing
5. Enter your HomeKit pairing code in the HomeKit settings section

> **Note:** HomeKit cannot be combined with other integrations. When `homekit` is the active mode, `controlLight()` returns immediately — state changes are pushed directly in the main loop by the HomeKit library.

---

## Advanced Settings

Access these under **Settings → Advanced Settings** (collapsible section).

### Custom Pin Configuration

> **Warning:** Only change these if you know what you're doing. Incorrect pin settings will prevent the sensor from working.

The firmware auto-detects pin assignments based on the board type. Custom pins are available for special configurations.

| Setting | Default (ESP32) | Default (ESP32-S3) |
|---------|-----------------|---------------------|
| Sensor OUT pin | GPIO4 | GPIO4 |
| Sensor TX pin | GPIO18 | GPIO17 |
| Sensor RX pin | GPIO17 | GPIO16 |
| RGB LED pin | GPIO16 | GPIO48 |

Click **Restore Default Pins** to revert to board defaults.

### Debug Level

| Level | Description |
|-------|-------------|
| None | No serial output (minimal performance impact) |
| Basic | Key events only (default) |
| Verbose | All events and data (useful for troubleshooting) |

### LED Brightness

Adjust the RGB LED brightness from 10% to 100%. Default is 50% (to reduce glare in dark rooms).

---

## Web Interface Usage

### Accessing the Interface

In default low-latency mode, run-time web services are disabled until service mode is enabled.

To access the web interface after setup:
1. Short-press **BOOT** to enable service mode (purple blinking LED)
2. Open the web interface via:
- **IP address:** `http://192.168.1.XXX` (check your Serial Monitor or router's device list)
- **mDNS hostname:** `http://presence.local` (works on most networks)

### Login

Navigate to `http://presence.local/login` (redirects automatically if not logged in).

Enter your admin password. The session lasts **15 minutes** of inactivity before automatic logout.

**Rate limiting:** After 5 failed login attempts, the login page will lock out for 1 minute.

### Status Dashboard (`/`)

The main page shows live information updated every 2 seconds:

| Field | Description |
|-------|-------------|
| Sensor Status | Present / No Presence |
| LED Color | Current LED state and meaning |
| Light Control | On/Off status (if EISY/ISY configured) |
| WiFi Signal | Signal strength in dBm and percentage |
| Uptime | Days, hours, minutes since last boot |
| IP Address | Current network IP |
| Board Type | Detected board (ESP32 or ESP32-S3) |
| Pin Configuration | Current pin assignments |
| Free Memory | Available heap memory in bytes |
| Last Detection | Timestamp of most recent presence event |

### Settings Page (`/config`)

Change any configuration setting. Click:
- **Save Settings** — saves without rebooting
- **Save & Reboot** — saves and restarts (required for pin changes)
- **Cancel** — discards unsaved changes

### Changing Admin Password

On the Settings page, scroll to "Change Admin Password":
1. Enter your current password
2. Enter a new password
3. Confirm the new password
4. Click Save Settings

---

## Security Best Practices

1. **Set a strong admin password** during initial setup
2. **Use HTTPS** if your EISY supports it (reduces eavesdropping risk on local network)
3. **Don't expose the device** to the internet (keep it on your local network only)
4. **Change default EISY/ISY credentials** if you haven't already
5. **Update firmware** periodically via OTA updates (service mode) to get security fixes

---

## Changing Settings After Setup

If you need to change settings without a factory reset:

1. Short-press **BOOT** to enable service mode (purple blinking LED)
2. Navigate to `http://presence.local` (or the device's IP address)
3. Log in with your admin password
4. Click **Settings** in the navigation
5. Make your changes
6. Click **Save Settings** or **Save & Reboot**

### Changing WiFi Network

If you need to connect to a different WiFi network:

1. Go to Settings → WiFi Settings
2. Select the new network from the dropdown
3. Enter the password
4. Click **Save & Reboot**

The device will restart and attempt to connect to the new network. If it fails, it will fall back to setup mode.

---

## Export and Import Configuration

### Export Configuration

1. Log in to the web interface
2. Go to **Settings → Export Configuration**
3. Click **Download Config (JSON)**
4. Save the file to your computer

The JSON file contains all settings except the admin password hash.

### Import Configuration

1. Log in to the web interface
2. Go to **Settings → Import Configuration**
3. Click **Choose File** and select your saved JSON file
4. Click **Import**
5. Review the settings and click **Save & Reboot**

**Use cases:**
- Back up your configuration before a factory reset
- Copy settings to a second device
- Restore settings after a firmware update
