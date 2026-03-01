# Configuration Guide

This document provides a detailed walkthrough of all configuration options for the ESP32 Presence Detection System.

---

## Table of Contents

- [First-Time Setup](#first-time-setup)
- [WiFi Configuration](#wifi-configuration)
- [Detection Settings](#detection-settings)
- [Admin Password](#admin-password)
- [EISY / ISY / Polisy Integration](#eisy--isy--polisy-integration)
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
   - **SSID:** `Presence_XXXX` (where XXXX = last 4 chars of MAC address, e.g., `Presence_A3F2`)
   - **Password:** None (open network)
3. Connect to this network from your phone, tablet, or laptop
4. The configuration page opens automatically (captive portal)
   - If it doesn't open automatically, navigate to `http://192.168.4.1`

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

**Tip:** If the light turns off while you're sitting still, increase this time. The LD2410C detects static presence, but may occasionally miss it depending on positioning.

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

After initial setup, access the web interface via:
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
| Sensor Status | Present / No Presence, Moving / Static |
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
5. **Update firmware** periodically via OTA updates to get security fixes

---

## Changing Settings After Setup

If you need to change settings without a factory reset:

1. Navigate to `http://presence.local` (or the device's IP address)
2. Log in with your admin password
3. Click **Settings** in the navigation
4. Make your changes
5. Click **Save Settings** or **Save & Reboot**

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
