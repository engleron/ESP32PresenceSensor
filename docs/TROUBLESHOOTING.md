# Troubleshooting Guide

This guide covers common issues and solutions for the ESP32 Presence Detection System.

---

## Table of Contents

- [LED Issues](#led-issues)
- [Sensor Issues](#sensor-issues)
- [WiFi Issues](#wifi-issues)
- [Web Interface Issues](#web-interface-issues)
- [Admin Password Issues](#admin-password-issues)
- [EISY / ISY Not Responding](#eisy--isy-not-responding)
- [Compile Errors](#compile-errors)
- [Upload Errors](#upload-errors)
- [Board Detection Issues](#board-detection-issues)
- [Getting Help](#getting-help)

---

## LED Issues

### LED Does Not Light Up At All

**Symptoms:** No LED activity after powering on

**Check:**
1. Verify the board is powered (the small power LED on the board should be on)
2. Open Serial Monitor (115200 baud) and look for initialization messages
3. Confirm you selected the correct board in Arduino IDE (Tools → Board)
4. Verify the correct RGB LED GPIO pin for your board:
   - ESP32-DevKitC-32E: GPIO16
   - ESP32-S3: GPIO48
5. If using custom pins, check Advanced Settings in the web interface

**Solutions:**
- Re-flash with the correct board selected
- Check that the `Adafruit NeoPixel` library is installed
- Try reducing LED brightness in Advanced Settings

### LED Shows Wrong Colors

**Symptoms:** LED color doesn't match expected status

**Check:**
- Verify sensor wiring (see [docs/WIRING.md](WIRING.md))
- Open Serial Monitor to see what state the device thinks it's in
- Ensure sensor OUT pin is connected correctly

---

## Sensor Issues

### Sensor Not Detecting Presence

**Symptoms:** LED stays yellow (or warning-red) even when you're in the room

**Check:**
1. Verify VCC is connected to **5V** (not 3.3V) — the LD2410C requires 5V
2. Verify GND is connected
3. Check that OUT pin is connected to GPIO4
4. Check Serial Monitor for any sensor error messages
5. The sensor needs 5–10 seconds to initialize after power-on

**Solutions:**
- Move the sensor closer to the detection area
- Ensure nothing is blocking the sensor's antenna (plastic is fine, metal blocks radar)
- Try pointing the sensor directly at the area to detect
- Use the LD2410C configuration software to verify sensor is working

### Sensor Detects False Presence (Triggers When Empty)

**Symptoms:** LED shows green/yellow even when no one is in the room

**Check:**
- Moving objects near the sensor (fans, curtains, HVAC vents)
- Sensor sensitivity settings

**Solutions:**
- Use the LD2410C configuration software to reduce sensitivity
- Reposition the sensor away from moving objects
- Increase the Light Off Delay so brief false detections don't keep the light on

### No Separate Moving/Static LED State

**Symptoms:** LED does not show separate moving/static colors

**Check:**
- This is expected in default OUT-pin mode
- LD2410C OUT pin reports presence HIGH/LOW, not a separate moving/static classification

**Solutions:**
- This is normal behavior in current firmware (green = presence, yellow = no presence while light is still on, red pulse = final-minute warning, solid red = no presence with light off)
- If you need true moving vs static reporting, use a UART-parsing firmware mode and tune LD2410C moving/static sensitivity

---

## WiFi Issues

### Can't Find the Setup WiFi Network

**Symptoms:** `Presence_XXXX` network doesn't appear on your phone

**Check:**
1. Ensure you're looking at 2.4 GHz networks
2. Wait 30 seconds after powering on before scanning
3. Open Serial Monitor and look for "*** SETUP MODE ***" message with the SSID
4. Check the LED is blinking blue (setup mode)

**Solutions:**
- Move closer to the ESP32
- Try scanning for WiFi networks again
- Restart the ESP32 by pressing the RESET button (not BOOT)

### Can't Connect to `Presence_XXXX` Network

**Symptoms:** Phone shows the network but fails to connect

**Check:**
- The setup AP has no password — leave the password field blank
- Some phones require you to tap "Connect without internet" or "Use this network anyway"

**Solutions:**
- Tap "Connect anyway" or "Keep this connection" when your phone warns about no internet
- Try connecting from a laptop instead

### Device Can't Connect to Your Home WiFi

**Symptoms:** After configuration, device doesn't connect to WiFi; returns to setup mode

**Check:**
1. Verify the correct network was selected (must be 2.4 GHz)
2. Verify the password was entered correctly (case-sensitive!)
3. Check if your router has any MAC address filtering enabled
4. Check if your router has 2.4 GHz band enabled

**Solutions:**
- Try re-entering the WiFi password carefully
- Temporarily disable MAC address filtering on your router
- Move the ESP32 closer to your router during initial setup
- Check your router's security type (WPA2 is recommended; some older protocols may have issues)

### Device Keeps Dropping WiFi Connection

**Symptoms:** Device loses WiFi intermittently

**Check:**
- WiFi signal strength (shown in Status Dashboard — should be above -70 dBm)
- Power supply quality (voltage drops during WiFi transmission)

**Solutions:**
- Move the device closer to the router or add a WiFi extender
- Use a quality USB power adapter (1A minimum)
- Replace the USB cable with a higher quality one

---

## Web Interface Issues

### Can't Access Web Interface

**Symptoms:** `http://presence.local` or IP address doesn't load

**Check:**
1. Ensure the device is connected to WiFi (LED should not be blinking blue)
2. Enable service mode with a short BOOT press (purple blinking LED)
3. Find the IP address in Serial Monitor output
4. Try accessing by IP address directly (not mDNS)
5. Ensure your computer is on the same WiFi network as the device

**Solutions:**
- Disable any VPN on your computer
- Try a different browser
- Check that your firewall isn't blocking port 80
- mDNS (`presence.local`) may not work on all networks — try the IP address

### Captive Portal Doesn't Open Automatically

**Symptoms:** Connected to setup WiFi but configuration page doesn't open

**Solutions:**
- Manually navigate to `http://192.168.4.1`
- Wait a few seconds for WiFi scan results; the setup page auto-refreshes while scanning
- Try opening any non-HTTPS website (like `http://example.com`) — it should redirect
- On Android: tap the notification that appears after connecting ("Sign in to network")
- On Windows: a browser window should open automatically; if not, try `http://192.168.4.1`

### Login Page Shows "Too Many Attempts"

**Symptoms:** After several failed logins, login is locked

**Solution:**
- Wait 1 minute for the lockout to expire
- The counter resets after a successful login or device restart

### Session Expires Too Quickly

**Symptoms:** Getting logged out before 15 minutes

**Solutions:**
- The Status Dashboard auto-refreshes every 2 seconds, which resets the timeout
- If using the Settings page without interaction, the session may expire
- Log in again — your settings are preserved

---

## Admin Password Issues

### Forgot Admin Password

**Solution:** Perform a factory reset:
1. Hold the BOOT button for **5 seconds**
2. Release when you see rapid red/blue blinking
3. Purple LED flashes 3 times to confirm reset
4. Device reboots into setup mode
5. Reconfigure the device and set a new admin password

> **Note:** Factory reset erases all settings including WiFi credentials and EISY/ISY configuration.

### Admin Password Doesn't Work

**Check:**
- Passwords are case-sensitive
- Check for spaces at the beginning or end of the password
- Try using the keyboard directly instead of copy-paste (to avoid invisible characters)

---

## EISY / ISY Not Responding

### Light Control Not Working

**Symptoms:** Presence is detected but light doesn't turn on/off

**Check:**
1. Go to Settings → EISY/ISY Settings → click **Test Connection**
2. Verify the controller IP address is correct
3. Verify username and password are correct
4. Verify the Insteon device address format: `1A.2B.3C`
5. Check HTTPS setting (EISY uses HTTPS, ISY994 uses HTTP)

**Solutions:**
- Ping the controller IP from your computer to confirm it's reachable
- Try accessing `http://192.168.1.XXX/rest/nodes` in your browser with credentials
- Check if the controller is online and not in maintenance mode
- Verify the device address by right-clicking the device in Admin Console → Copy → Address

### HTTP 401 Unauthorized Error (in Serial Monitor)

**Solution:** Check username and password. They are case-sensitive.

### HTTP 404 Not Found Error (in Serial Monitor)

**Solution:** The device address is incorrect. Verify the Insteon device address in Admin Console.

### HTTPS Certificate Error

**Symptoms:** HTTPS connection fails even with "Use HTTPS" checked

**Solution:** The firmware uses `setInsecure()` to skip certificate validation for self-signed certificates. This is expected behavior for local EISY connections. If this fails, check that your EISY is actually configured for HTTPS.

---

## Compile Errors

### "Adafruit_NeoPixel.h: No such file or directory"

**Solution:** Install the Adafruit NeoPixel library:
1. Arduino IDE → Sketch → Include Library → Manage Libraries
2. Search for "Adafruit NeoPixel"
3. Click Install on "Adafruit NeoPixel by Adafruit"

### "WiFi.h: No such file or directory"

**Solution:** Install the ESP32 board package:
1. Arduino IDE → File → Preferences
2. Add to "Additional Boards Manager URLs":
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. Tools → Board → Boards Manager
4. Search "esp32" and install "esp32 by Espressif Systems"

### "#error Unsupported board!"

**Solution:** Select a supported board in Arduino IDE:
- Tools → Board → ESP32 Arduino → ESP32 Dev Module (for ESP32-DevKitC-32E)
- Tools → Board → ESP32 Arduino → ESP32S3 Dev Module (for ESP32-S3)

### "mbedtls/md.h: No such file or directory"

**Solution:** This is part of the ESP32 core library. Update the ESP32 board package to version 3.3.7 or later.

### NeoPixel duplicate/redefinition errors (files ending in `\" 2.cpp\"`, `\" 2.h\"`, etc.)

**Symptoms:** Errors like:
- `no declaration matches 'Adafruit_NeoPixel::Adafruit_NeoPixel(...)'`
- `redefinition of ...`
- compile output references files such as `Adafruit_NeoPixel 2.cpp`

**Cause:** A duplicated/corrupted library install in `~/Documents/Arduino/libraries/Adafruit_NeoPixel`.

**Solution:**
1. Arduino IDE → Library Manager → uninstall `Adafruit NeoPixel`
2. Delete the folder `~/Documents/Arduino/libraries/Adafruit_NeoPixel`
3. Reinstall `Adafruit NeoPixel` from Library Manager
4. Re-verify compile

---

## Upload Errors

### "Failed to connect to ESP32: Timed out"

**Solutions:**
1. Hold the BOOT button while clicking Upload, release when you see "Connecting..."
2. Check USB cable (try a different cable — many cables are power-only with no data wires)
3. Try a different USB port
4. Close the Serial Monitor before uploading
5. Lower the upload speed: Tools → Upload Speed → 460800

### "A fatal error occurred: Could not open /dev/ttyUSBX"

**Linux solution:**
```bash
sudo usermod -a -G dialout $USER
```
Log out and back in, then try again.

### "A fatal error occurred: Wrong boot mode detected"

**Solution:** Put the ESP32 in bootloader mode:
1. Hold the BOOT button
2. Press and release the RESET button
3. Release the BOOT button
4. Click Upload

---

## Board Detection Issues

### Wrong Board Detected

**Symptoms:** Compilation warning shows wrong board type

**Solution:** In Arduino IDE, select the correct board:
- Tools → Board → ESP32 Arduino → [select your board]

The board is detected at **compile time** based on which board you select in the IDE, not at runtime. The `#warning` message during compilation tells you which board configuration was used.

### "Compiling for ESP32" But I Have an ESP32-S3

**Solution:** You have the wrong board selected. Go to Tools → Board → ESP32 Arduino → ESP32S3 Dev Module.

### No Warning Message During Compilation

**Solution:** Enable verbose compilation output:
- Arduino IDE → File → Preferences → "Show verbose output during: compilation"
- Compile again and look for the warning in the output

---

## Getting Help

If you've tried the steps above and still have issues:

1. **Check Serial Monitor output** — set to 115200 baud and look for error messages
2. **Open an issue** on the GitHub repository with:
   - Your board type
   - Firmware version (shown in web interface footer)
   - Arduino IDE version
   - Full Serial Monitor output from boot
   - Description of the problem
3. **Enable Verbose debug** in Advanced Settings for more detailed logging
