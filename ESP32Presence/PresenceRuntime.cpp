#include "PresenceRuntime.h"
#include "PresenceCore.h"
#include "PresenceIntegrations.h"
#include "PresenceWeb.h"

namespace {
bool runModeServicesActive = false;
bool runModeWebHandlersConfigured = false;
bool otaCallbacksConfigured = false;
bool serviceModeActive = false;
}  // namespace

void stopRunModeServices();
bool startRunModeServices(bool asServiceMode);
void toggleServiceMode();


/*
 * ================================================================
 * SECTION: LED CONTROL
 * ================================================================
 */

/*
 * blinkBlueHeartbeat - Blink blue LED at 1-second intervals (setup mode).
 */
void blinkBlueHeartbeat() {
  if (millis() - lastHeartbeat > HEARTBEAT_INTERVAL) {
    heartbeatState = !heartbeatState;
    lastHeartbeat  = millis();
    setRGB(0, 0, heartbeatState ? 255 : 0);
  }
}

/*
 * blinkPurpleHeartbeat - Blink purple LED at 1-second intervals (service mode).
 */
void blinkPurpleHeartbeat() {
  if (millis() - lastHeartbeat > HEARTBEAT_INTERVAL) {
    heartbeatState = !heartbeatState;
    lastHeartbeat  = millis();
    setRGB(heartbeatState ? 255 : 0, 0, heartbeatState ? 255 : 0);
  }
}

/*
 * blinkRedBlue - Alternate red/blue at 500ms intervals (error state).
 */
void blinkRedBlue() {
  static unsigned long lastBlink = 0;
  static bool isRed = true;
  if (millis() - lastBlink > 500) {
    isRed = !isRed;
    lastBlink = millis();
    setRGB(isRed ? 255 : 0, 0, isRed ? 0 : 255);
  }
}

/*
 * pulseRedWarning - Pulse red LED during final-minute no-presence warning.
 * Pulse speeds up every 15 seconds as timeout approaches.
 */
void pulseRedWarning(unsigned long remainingMs) {
  unsigned long periodMs = 1600;  // 60s..45s remaining (slow)
  if (remainingMs <= 45000UL) periodMs = 1100;  // 45s..30s
  if (remainingMs <= 30000UL) periodMs = 700;   // 30s..15s
  if (remainingMs <= 15000UL) periodMs = 400;   // 15s..0s (fast)

  unsigned long halfPeriod = periodMs / 2;
  unsigned long phase = millis() % periodMs;
  unsigned long ramp = (phase <= halfPeriod) ? phase : (periodMs - phase);

  // Keep a visible red floor and pulse up to full red.
  int redLevel = 32 + (int)((223UL * ramp) / halfPeriod);
  setRGB(redLevel, 0, 0);
}

/*
 * updateLED - Set LED color based on current sensor state.
 */
void updateLED() {
  static int prevState = -1;
  int state = 0;
  unsigned long remainingMs = 0;

  if (sensorError) {
    blinkRedBlue();
    state = 5;  // reserved for sensor error
  } else if (presenceDetected) {
    setRGB(0, 255, 0);  // Green: presence
    state = 1;
  } else {
    bool warningState = false;
    if (lightOn && noDetectionTimeout > 0) {
      unsigned long timeoutMs = (unsigned long)noDetectionTimeout * 1000UL;
      unsigned long elapsedMs = millis() - lastDetectionTime;
      if (elapsedMs < timeoutMs) {
        remainingMs = timeoutMs - elapsedMs;
        warningState = (remainingMs <= 60000UL);
      }
    }

    if (!lightOn) {
      setRGB(255, 0, 0);  // Red: no presence, hold timer elapsed
      state = 3;
    } else if (warningState) {
      pulseRedWarning(remainingMs);  // Red pulse: hold timer about to elapse
      state = 4;
    } else {
      setRGB(255, 255, 0);  // Yellow: no presence, hold timer counting down
      state = 2;
    }
  }

  if (state != prevState && state != 5) {
    prevState = state;
    // In HomeKit mode the device is a sensor (no light), so the LED reflects
    // the occupancy-hold timer rather than a light. Use matching labels.
    bool hk = (integrationMode == "homekit");
    unsigned long remainingSecs = (remainingMs + 999UL) / 1000UL;
    String msg;
    switch (state) {
      case 1:
        msg = hk ? "GREEN (occupancy detected)" : "GREEN (presence)";
        break;
      case 2:
        msg = hk ? "YELLOW (no presence, occupancy held, clearing in "
                     + String(noDetectionTimeout) + "s)"
                 : "YELLOW (no presence, light on, off in "
                     + String(noDetectionTimeout) + "s)";
        break;
      case 3:
        msg = hk ? "RED (occupancy cleared)" : "RED (no presence, light off)";
        break;
      case 4:
        msg = hk ? "RED PULSE (occupancy clearing in " + String(remainingSecs) + "s)"
                 : "RED PULSE (light off in " + String(remainingSecs) + "s)";
        break;
    }
    if (state >= 1 && state <= 4) {
      serialPrintln("LED: " + msg);
    }
  }
}

/*
 * ================================================================
 * SECTION: SENSOR FUNCTIONS
 * ================================================================
 */

/*
 * ----------------------------------------------------------------
 * LD2410C configuration command protocol
 *
 * Config/ACK frames use a different framing from the data report
 * frames parsed in parseLD2410CSerial():
 *   Header (4): FD FC FB FA
 *   Length (2): payload length, little-endian (command word + value)
 *   Payload   : command word (2, LE) + value bytes
 *   Footer (4): 04 03 02 01
 *
 * Commands used here (per Hi-Link LD2410 serial protocol):
 *   0x00FF enable configuration   (value 0x0001)
 *   0x00FE end configuration
 *   0x0060 set max moving/static gate + no-one duration
 *   0x0064 set per-gate sensitivity (gate 0xFFFF = all gates)
 *
 * Sensitivity values are detection THRESHOLDS (0-100): a gate reports a
 * target only when its energy exceeds the threshold, so a HIGHER value
 * means LESS sensitive / fewer false triggers.
 * ----------------------------------------------------------------
 */

// Send one LD2410 command frame and wait briefly for its ACK.
// Returns true if an ACK with success status (0x0000) was received.
static bool ld2410SendCommand(uint16_t cmd, const uint8_t* value, uint16_t valueLen) {
  uint16_t payloadLen = 2 + valueLen;  // command word + value
  uint8_t header[6] = {0xFD, 0xFC, 0xFB, 0xFA,
                       (uint8_t)(payloadLen & 0xFF), (uint8_t)(payloadLen >> 8)};
  uint8_t cmdWord[2] = {(uint8_t)(cmd & 0xFF), (uint8_t)(cmd >> 8)};
  uint8_t footer[4] = {0x04, 0x03, 0x02, 0x01};

  // Drop any pending RX bytes so we read this command's ACK, not stale data.
  while (Serial2.available()) Serial2.read();

  Serial2.write(header, sizeof(header));
  Serial2.write(cmdWord, sizeof(cmdWord));
  if (valueLen > 0) Serial2.write(value, valueLen);
  Serial2.write(footer, sizeof(footer));
  Serial2.flush();

  // Read ACK: header FD FC FB FA ... footer 04 03 02 01.
  uint8_t buf[64];
  int len = 0;
  unsigned long start = millis();
  while ((millis() - start) < 200 && len < (int)sizeof(buf)) {
    esp_task_wdt_reset();  // ACK wait must not trip the 8s watchdog
    while (Serial2.available() && len < (int)sizeof(buf)) {
      buf[len++] = (uint8_t)Serial2.read();
    }
    // Look for a complete ACK frame in what we have so far.
    for (int i = 0; i + 10 <= len; i++) {
      if (buf[i] == 0xFD && buf[i+1] == 0xFC && buf[i+2] == 0xFB && buf[i+3] == 0xFA) {
        // buf[i+6..7] = (cmd | 0x0100); buf[i+8..9] = status (0 = success)
        uint16_t ackCmd = (uint16_t)buf[i+6] | ((uint16_t)buf[i+7] << 8);
        uint16_t status = (uint16_t)buf[i+8] | ((uint16_t)buf[i+9] << 8);
        if (ackCmd == (cmd | 0x0100)) return (status == 0x0000);
      }
    }
  }
  return false;  // no/!valid ACK within timeout
}

// Send a command, retrying a few times if no ACK arrives (the radar can be
// briefly unready right after power-up).
static bool ld2410SendCommandRetry(uint16_t cmd, const uint8_t* value, uint16_t valueLen) {
  for (int attempt = 0; attempt < 3; attempt++) {
    if (ld2410SendCommand(cmd, value, valueLen)) return true;
    delay(80);
  }
  return false;
}

// Read the sensor's currently stored parameters (must be in config mode) and
// log them, so we can confirm a write actually took effect.
static void ld2410ReadParams() {
  uint8_t header[6] = {0xFD, 0xFC, 0xFB, 0xFA, 0x02, 0x00};
  uint8_t cmdWord[2] = {0x61, 0x00};  // 0x0061 read parameters, no value
  uint8_t footer[4] = {0x04, 0x03, 0x02, 0x01};

  while (Serial2.available()) Serial2.read();
  Serial2.write(header, sizeof(header));
  Serial2.write(cmdWord, sizeof(cmdWord));
  Serial2.write(footer, sizeof(footer));
  Serial2.flush();

  uint8_t buf[80];
  int len = 0;
  unsigned long start = millis();
  while ((millis() - start) < 300 && len < (int)sizeof(buf)) {
    esp_task_wdt_reset();
    while (Serial2.available() && len < (int)sizeof(buf)) buf[len++] = (uint8_t)Serial2.read();
    for (int i = 0; i + 38 <= len; i++) {
      if (buf[i] == 0xFD && buf[i+1] == 0xFC && buf[i+2] == 0xFB && buf[i+3] == 0xFA &&
          buf[i+6] == 0x61 && buf[i+7] == 0x01) {
        // Layout after cmd echo: status(2), 0xAA, maxGate, maxMovGate, maxStaGate,
        // moving sens g0..g8 (9), static sens g0..g8 (9), no-one duration (2 LE).
        uint8_t maxGate    = buf[i+11];
        uint8_t maxMovGate = buf[i+12];
        uint8_t maxStaGate = buf[i+13];
        uint16_t noOne     = (uint16_t)buf[i+32] | ((uint16_t)buf[i+33] << 8);
        String mov, sta;
        for (int g = 0; g <= 8; g++) {
          mov += String(buf[i+14+g]) + (g < 8 ? "," : "");
          sta += String(buf[i+23+g]) + (g < 8 ? "," : "");
        }
        serialPrintln("LD2410C readback: maxGate=" + String(maxGate) +
                      " maxMov=" + String(maxMovGate) + " maxSta=" + String(maxStaGate) +
                      " noOne=" + String(noOne) + "s");
        serialPrintln("  moving sens g0-8: " + mov);
        serialPrintln("  static sens g0-8: " + sta);
        return;
      }
    }
  }
  serialPrintln(F("LD2410C readback: no parameter ACK"));
}

// Helper: pack a 16-bit parameter word + 32-bit little-endian value into a buffer.
static void ld2410PackParam(uint8_t* dst, uint16_t word, uint32_t val) {
  dst[0] = (uint8_t)(word & 0xFF);
  dst[1] = (uint8_t)(word >> 8);
  dst[2] = (uint8_t)(val & 0xFF);
  dst[3] = (uint8_t)((val >> 8) & 0xFF);
  dst[4] = (uint8_t)((val >> 16) & 0xFF);
  dst[5] = (uint8_t)((val >> 24) & 0xFF);
}

/*
 * applyLd2410Config - Push the saved sensitivity/range tuning to the radar.
 *
 * No-op (returns true) when ld2410TuningEnabled is false so the sensor keeps
 * its own factory/per-gate settings. Safe to call at boot and after a save;
 * the LD2410 persists these values in its own flash. Briefly stops data
 * reporting while config mode is active.
 */
bool applyLd2410Config() {
  if (!ld2410TuningEnabled) return true;

  serialPrintln(F("Applying LD2410C sensor tuning..."));

  bool ok = true;

  // 1) Enable configuration mode (value 0x0001). Retry: the radar may be
  //    briefly unready right after power-up.
  uint8_t enable[2] = {0x01, 0x00};
  ok &= ld2410SendCommandRetry(0x00FF, enable, sizeof(enable));

  // 2) Max moving gate (word 0), max static gate (word 1), no-one duration
  //    in seconds (word 2). Keep absence delay short (5 s) and let the firmware
  //    occupancy timeout own the long off-delay.
  uint8_t maxCfg[18];
  ld2410PackParam(maxCfg + 0,  0x0000, (uint32_t)ld2410MaxGate);
  ld2410PackParam(maxCfg + 6,  0x0001, (uint32_t)ld2410MaxGate);
  ld2410PackParam(maxCfg + 12, 0x0002, (uint32_t)5);
  ok &= ld2410SendCommandRetry(0x0060, maxCfg, sizeof(maxCfg));

  // 3) Sensitivity for all gates at once (gate word value 0xFFFF).
  uint8_t sensCfg[18];
  ld2410PackParam(sensCfg + 0,  0x0000, (uint32_t)0xFFFF);
  ld2410PackParam(sensCfg + 6,  0x0001, (uint32_t)ld2410MovingSens);
  ld2410PackParam(sensCfg + 12, 0x0002, (uint32_t)ld2410StaticSens);
  ok &= ld2410SendCommandRetry(0x0064, sensCfg, sizeof(sensCfg));

  // 4) Read the stored values back and log them for verification.
  ld2410ReadParams();

  // 5) End configuration mode (resumes data reporting).
  ok &= ld2410SendCommandRetry(0x00FE, nullptr, 0);

  if (ok) {
    serialPrintln("LD2410C tuned: gate<=" + String(ld2410MaxGate) +
                  " (~" + String(ld2410MaxGate * 75) + "cm)  moving=" +
                  String(ld2410MovingSens) + "  static=" + String(ld2410StaticSens));
  } else {
    serialPrintln(F("LD2410C tuning FAILED (no/!ACK) - check sensor TX/RX wiring & 256000 baud"));
  }
  return ok;
}

/*
 * readSensorData - Read the LD2410C OUT pin and update presence state.
 */
void readSensorData() {
  int outPinState = digitalRead(pinSensorOut);
  bool rawPresence = (outPinState == HIGH);

  // Debounce: require PRESENCE_DEBOUNCE_COUNT consecutive matching reads before
  // flipping presence state to suppress noise at sensor detection boundary.
  static bool lastRawPresence = false;
  static int  debounceCount   = 0;

  if (rawPresence == lastRawPresence) {
    if (debounceCount < PRESENCE_DEBOUNCE_COUNT) debounceCount++;
  } else {
    lastRawPresence = rawPresence;
    debounceCount = 1;
  }

  if (debounceCount < PRESENCE_DEBOUNCE_COUNT) {
    // Update lastDetectionTime even during debounce when pin is high so
    // the no-detection timeout doesn't fire during a transient drop.
    if (rawPresence) lastDetectionTime = millis();
    sensorError = false;
    return;
  }

  if (rawPresence) {
    presenceDetected = true;
    lastDetectionTime = millis();
    sensorError = false;
    isMoving = true;

    if (millis() - lastStatusPrint > 2000) {
      verbosePrint(F("Presence detected: PRESENT"));
      lastStatusPrint = millis();
    }
  } else {
    presenceDetected = false;
    isMoving = false;

    if (lightOn && (millis() - lastDetectionTime > ((unsigned long)noDetectionTimeout * 1000UL))) {
      verbosePrint("Timeout: no detection for " + String(noDetectionTimeout / 60) + " min");
    }
  }

  sensorError = false;
}

/*
 * parseLD2410CSerial - Non-blocking parser for LD2410C UART data frames.
 *
 * The LD2410C sends basic reporting frames at ~10 Hz:
 *   Header (4): FD FC FB FA
 *   Length (2): 0D 00  (13 bytes of payload)
 *   Payload (13):
 *     [0]    0x02  data-type marker
 *     [1]    0xAA  frame head
 *     [2]    target state: 0=none 1=moving 2=stationary 3=both
 *     [3-4]  moving distance (LE, cm)
 *     [5]    moving energy (0-100)
 *     [6-7]  stationary distance (LE, cm)
 *     [8]    stationary energy (0-100)
 *     [9-10] detection distance (LE, cm)
 *     [11]   0x55  frame tail
 *     [12]   0x00  padding
 *   End (4): 04 03 02 01
 *
 * Uses a 64-byte linear shifting buffer. Validation is structural only
 * (frame markers and declared payload length); the LD2410C basic reporting
 * frame does not include a checksum/CRC byte so none is verified.
 * Populates the uartTarget* globals and sets lastUartUpdateMs.
 */
void parseLD2410CSerial() {
  static uint8_t buf[64];
  static int     bufLen = 0;

  // Drain whatever is in the UART FIFO without blocking.
  while (Serial2.available() > 0 && bufLen < (int)sizeof(buf)) {
    buf[bufLen++] = (uint8_t)Serial2.read();
  }

  // Process all complete frames that fit in the buffer.
  while (bufLen >= 23) {
    // Locate frame header FD FC FB FA.
    int startIdx = -1;
    for (int i = 0; i <= bufLen - 4; i++) {
      if (buf[i] == 0xFD && buf[i+1] == 0xFC &&
          buf[i+2] == 0xFB && buf[i+3] == 0xFA) {
        startIdx = i;
        break;
      }
    }
    if (startIdx < 0) {
      // No header yet — keep last 3 bytes in case it's a partial header.
      int keep = (bufLen >= 3) ? 3 : bufLen;
      memmove(buf, buf + (bufLen - keep), keep);
      bufLen = keep;
      return;
    }

    // Discard bytes before the header.
    if (startIdx > 0) {
      bufLen -= startIdx;
      memmove(buf, buf + startIdx, bufLen);
    }

    // Need a full 23-byte frame.
    if (bufLen < 23) return;

    // Validate data length, markers, and frame end.
    uint16_t dataLen = (uint16_t)buf[4] | ((uint16_t)buf[5] << 8);
    bool valid = (dataLen == 13 &&
                  buf[6]  == 0x02 && buf[7]  == 0xAA &&
                  buf[17] == 0x55 &&
                  buf[19] == 0x04 && buf[20] == 0x03 &&
                  buf[21] == 0x02 && buf[22] == 0x01);
    if (!valid) {
      // Not a basic reporting frame; skip one byte and retry.
      bufLen--;
      memmove(buf, buf + 1, bufLen);
      continue;
    }

    // Parse payload fields.
    uartTargetState        = buf[8];
    uartMovingDistance     = (int)((uint16_t)buf[9]  | ((uint16_t)buf[10] << 8));
    uartMovingEnergy       = (int)buf[11];
    uartStationaryDistance = (int)((uint16_t)buf[12] | ((uint16_t)buf[13] << 8));
    uartStationaryEnergy   = (int)buf[14];
    uartDetectionDistance  = (int)((uint16_t)buf[15] | ((uint16_t)buf[16] << 8));
    lastUartUpdateMs       = millis();

    // Consume the processed frame.
    bufLen -= 23;
    if (bufLen > 0) memmove(buf, buf + 23, bufLen);
  }
}

/*
 * ================================================================

 * SECTION: FACTORY RESET
 * ================================================================
 */

/*
 * checkResetButton - Check if BOOT button is held at startup.
 * @return true if button is pressed at boot time
 */
bool checkResetButton() {
  if (digitalRead(PIN_RESET) == LOW) {
    delay(30);  // debounce BOOT button at startup
    if (digitalRead(PIN_RESET) != LOW) return false;
    serialPrintln(F("BOOT button held at startup - entering setup mode"));
    return true;
  }
  return false;
}

/*
 * checkResetButtonHeld - Monitor BOOT button for 5-second factory reset.
 * Provides staged LED visual feedback during hold.
 * @return true only if factory reset was performed (device restarts)
 */
bool checkResetButtonHeld() {
  static unsigned long pressStartTime = 0;
  static bool wasPressed = false;

  bool isPressed = (digitalRead(PIN_RESET) == LOW);

  if (isPressed && !wasPressed) {
    pressStartTime = millis();
    wasPressed = true;
    serialPrintln(F("BOOT button pressed - hold 5 seconds for factory reset"));
  } else if (!isPressed && wasPressed) {
    unsigned long held = millis() - pressStartTime;
    wasPressed = false;
    setRGB(0, 0, 0);
    if (held >= 80 && held < RESET_HOLD_TIME) {
      if (!configMode) {
        serialPrintln(F("BOOT short press - toggling service mode"));
        toggleServiceMode();
      }
      return false;
    }
    serialPrintln(F("BOOT button released"));
  } else if (isPressed && wasPressed) {
    unsigned long held = millis() - pressStartTime;

    // Stage 1: 0-2 seconds - solid red
    if (held < 2000) {
      setRGB(255, 0, 0);
    }
    // Stage 2: 2-4 seconds - slow red/blue blink (500ms)
    else if (held < 4000) {
      static unsigned long lastBlink = 0;
      static bool blinkState = false;
      if (millis() - lastBlink > 500) {
        blinkState = !blinkState;
        lastBlink = millis();
        setRGB(blinkState ? 255 : 0, 0, blinkState ? 0 : 255);
      }
    }
    // Stage 3: 4-5 seconds - rapid red/blue blink (200ms)
    else if (held < 5000) {
      static unsigned long lastFast = 0;
      static bool fastState = false;
      if (millis() - lastFast > 200) {
        fastState = !fastState;
        lastFast = millis();
        setRGB(fastState ? 255 : 0, 0, fastState ? 0 : 255);
      }
    }
    // 5 seconds reached - perform reset
    else {
      Serial.println(F("\n*** FACTORY RESET TRIGGERED ***"));
      Serial.println(F("Reset in 5... 4... 3... 2... 1... RESETTING!"));

      // Purple confirmation flashes (3x)
      for (int i = 0; i < 3; i++) {
        setRGB(255, 0, 255);
        delay(150);
        setRGB(0, 0, 0);
        delay(150);
      }

      clearConfiguration();
      invalidateSession();

      delay(1000);
      ESP.restart();
      return true;
    }
  }

  return false;
}


/*
 * ================================================================
 * SECTION: WIFI FUNCTIONS
 * ================================================================
 */

void stopRunModeServices() {
  if (!runModeServicesActive) {
    serviceModeActive = false;
    return;
  }

  server.stop();
  MDNS.end();
  runModeServicesActive = false;
  serviceModeActive = false;
  serialPrintln(F("Run-mode web services stopped"));
}

bool startRunModeServices(bool asServiceMode) {
  if (runModeServicesActive) {
    serviceModeActive = asServiceMode;
    if (asServiceMode) {
      heartbeatState = true;
      lastHeartbeat = millis();
      setRGB(255, 0, 255);
    }
    return true;
  }
  if (WiFi.status() != WL_CONNECTED) {
    serialPrintln(F("Cannot start run-mode web services: WiFi not connected"));
    return false;
  }

  if (!runModeWebHandlersConfigured) {
    // esp32 Arduino core 3.x changed collectHeaders to require an array + count
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    const char* collectHeaderKeys[] = {"Cookie"};
    server.collectHeaders(collectHeaderKeys, 1);
#else
    server.collectHeaders("Cookie");
#endif
    setupWebServerRunMode();
    runModeWebHandlersConfigured = true;
  }

#ifdef ENABLE_HOMEKIT
  // HomeSpan owns the mDNS stack in HomeKit mode; calling MDNS.begin() here
  // would reset the hostname and wipe the _hap._tcp service record, making the
  // device invisible in Apple Home. HomeSpan registers presence.local itself.
  if (integrationMode != "homekit")
#endif
  {
    if (MDNS.begin("presence")) {
      serialPrintln(F("mDNS started: http://presence.local"));
    }
  }

  if (!otaCallbacksConfigured) {
    ArduinoOTA.setHostname("esp32-presence");
    ArduinoOTA.onStart([]() { serialPrintln(F("OTA update started...")); });
    ArduinoOTA.onEnd([]()   { serialPrintln(F("OTA update complete!")); });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      esp_task_wdt_reset();  // feed WDT during blocking OTA receive loop
    });
    ArduinoOTA.onError([](ota_error_t error) {
      serialPrintln("OTA error: " + String(error));
    });
    otaCallbacksConfigured = true;
  }
  ArduinoOTA.begin();
  server.begin();

  runModeServicesActive = true;
  serviceModeActive = asServiceMode;

  if (asServiceMode) {
    heartbeatState = true;
    lastHeartbeat = millis();
    setRGB(255, 0, 255);
    serialPrintln("Service mode enabled (LAN web): http://" + WiFi.localIP().toString());
  } else {
    serialPrintln(F("Run-mode web services enabled"));
  }
  return true;
}

void toggleServiceMode() {
  if (serviceModeActive) {
    stopRunModeServices();
    serialPrintln(F("Service mode disabled - returning to run mode"));
    return;
  }
  if (startRunModeServices(true)) {
    serialPrintln(F("Service mode active - BOOT short press again to exit"));
  }
}

/*
 * enterConfigMode - Start AP and captive portal for initial setup.
 */
void enterConfigMode() {
  serialPrintln(F("\n*** ENTERING SETUP MODE ***"));

  stopRunModeServices();
  configMode = true;

  // Force immediate visual setup indication (steady blue, then heartbeat).
  heartbeatState = true;
  lastHeartbeat = millis();
  setRGB(0, 0, 255);

  WiFi.mode(WIFI_AP_STA);  // AP+STA so we can scan networks
  generateDeviceSSID();

  WiFi.softAP(deviceSSID.c_str());
  startWiFiScanAsync();
  serialPrintln(F("Setup mode WiFi scan started"));

  IPAddress apIP = WiFi.softAPIP();

  Serial.println(F("\n================================================"));
  Serial.println(F("*** SETUP MODE ***"));
  Serial.print(F("Connect to WiFi: "));
  Serial.println(deviceSSID);
  Serial.print(F("AP IP Address:   "));
  Serial.println(apIP);
  Serial.println(F("================================================\n"));

  dnsServer.start(DNS_PORT, "*", apIP);

  setupWebServerSetupMode();
  server.begin();

  serialPrintln(F("Setup web server started"));
}

/*
 * connectToWiFi - Connect to configured WiFi network.
 */
void connectToWiFi() {
  serialPrint(F("Connecting to WiFi: "));
  serialPrintln(wifiSSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());

  const unsigned long connectTimeoutMs = 30000;
  const unsigned long dotIntervalMs    = 1000;
  unsigned long connectStart           = millis();
  unsigned long lastDotAt              = connectStart;

  while (WiFi.status() != WL_CONNECTED && (millis() - connectStart) < connectTimeoutMs) {
    delay(100);
    // setup() can block here for up to 30s; feed TWDT while waiting for association
    esp_task_wdt_reset();

    if (Serial && debugLevel > 0 && (millis() - lastDotAt) >= dotIntervalMs) {
      Serial.print(".");
      lastDotAt = millis();
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    serialPrintln(F("WiFi connected!"));
    serialPrint(F("IP Address: "));
    serialPrintln(WiFi.localIP().toString());

#ifdef ENABLE_HOMEKIT
    // Must initialise HomeSpan before startRunModeServices so HomeSpan owns the
    // mDNS stack. startRunModeServices skips MDNS.begin() in HomeKit mode.
    initHomeKit();
#endif

#if ENABLE_RUNMODE_WEB_OTA_DEFAULT
    startRunModeServices(false);
#else
    serialPrintln(F("Run mode active (web disabled by default)"));
    serialPrintln(F("Press BOOT briefly to toggle LAN web service mode"));
#endif

  } else {
    Serial.println();
    serialPrintln(F("WiFi connection failed! Entering setup mode..."));
    enterConfigMode();
  }
}

/*
 * ================================================================
 * SECTION: SETUP AND LOOP
 * ================================================================
 */

void presenceInit() {
  deviceStartTime = millis();

  Serial.begin(BAUD_RATE_DEBUG);
  delay(100);

  Serial.println(F("\n\n================================================"));
  Serial.println(F("ESP32 LD2410C Presence Detection System"));
  Serial.print(F("Firmware Version: "));
  Serial.println(FIRMWARE_VERSION);
  Serial.print(F("Board: "));
  Serial.println(BOARD_TYPE);
  Serial.println(F("Integrations: EISY/ISY, Insteon Hub 2, Home Assistant, HomeKit"));
  Serial.println(F("================================================\n"));

  // Initialize NeoPixel LED
  strip.begin();
  strip.show();
  strip.setBrightness(50);
  setRGB(0, 0, 0);
  Serial.print(F("RGB LED on GPIO"));
  Serial.println(RGB_LED_PIN);

  // Reset button
  pinMode(PIN_RESET, INPUT_PULLUP);

  // Load configuration (may override default pins)
  loadConfiguration();

  // Apply saved LED brightness
  strip.setBrightness(ledBrightness);

  // Initialize UART for sensor
  Serial2.begin(BAUD_RATE_SENSOR, SERIAL_8N1, pinSensorTx, pinSensorRx);
  Serial.print(F("Sensor UART: RX=GPIO"));
  Serial.print(pinSensorTx);
  Serial.print(F(", TX=GPIO"));
  Serial.println(pinSensorRx);

  // Initialize sensor OUT pin
  pinMode(pinSensorOut, INPUT);

  // Push saved sensitivity/range tuning to the radar (no-op if tuning disabled).
  // Give the LD2410C ~1 s after power-up before sending config commands; it can
  // ignore commands while still booting, which would silently drop the tuning.
  if (ld2410TuningEnabled) {
    delay(1000);
    applyLd2410Config();
  }

  // Integration worker handles potentially blocking network actions off the main loop
  initIntegrationWorker();

  // Start watchdog timer
  // esp32 Arduino core 3.x changed esp_task_wdt_init to take a config struct
  esp_err_t wdtInitResult = ESP_OK;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  esp_task_wdt_config_t wdtConfig = {
    .timeout_ms   = WDT_TIMEOUT_SECONDS * 1000,
    .idle_core_mask = 0,
    .trigger_panic  = true
  };
  // On Arduino-ESP32 3.x, TWDT may already be initialized by the core.
  // Reconfigure first to avoid "already initialized" boot errors.
  wdtInitResult = esp_task_wdt_reconfigure(&wdtConfig);
  if (wdtInitResult == ESP_ERR_INVALID_STATE) {
    wdtInitResult = esp_task_wdt_init(&wdtConfig);
  }
#else
  wdtInitResult = esp_task_wdt_init(WDT_TIMEOUT_SECONDS, true);
#endif
  if (wdtInitResult != ESP_OK) {
    serialPrintln("Watchdog init warning, err=" + String((int)wdtInitResult));
  }

  esp_err_t wdtAddResult = esp_task_wdt_add(NULL);
  if (wdtAddResult == ESP_ERR_INVALID_ARG) {
    serialPrintln(F("Watchdog task already registered"));
  } else if (wdtAddResult != ESP_OK) {
    serialPrintln("Watchdog task add warning, err=" + String((int)wdtAddResult));
  }
  serialPrintln(F("Watchdog timer started (8s timeout)"));

  // Decide mode: setup or connect
  if (wifiSSID == "" || checkResetButton()) {
    enterConfigMode();
  } else {
    connectToWiFi();
  }

  Serial.println(F("\n================================================"));
  if (configMode) {
    Serial.println(F("Initialization complete: SETUP MODE ACTIVE"));
    Serial.println(F("Open captive portal or browse http://192.168.4.1"));
  } else {
    Serial.println(F("Initialization complete: RUN MODE ACTIVE"));
  }
  Serial.println(F("================================================\n"));
}

/*
 * logHeapStats - Periodically report free-heap diagnostics.
 *
 * Prints current free heap, the largest allocatable block (a fragmentation
 * indicator), and the minimum free heap ever seen since boot. A steadily
 * falling minimum points to a leak; a free heap that stays high while the
 * largest block shrinks points to fragmentation. Either can precede the
 * heap-corruption / out-of-memory crashes seen under HomeKit load.
 */
void logHeapStats() {
#if HEAP_LOG_INTERVAL_MS > 0
  static unsigned long lastHeapLog = 0;
  unsigned long now = millis();
  if (now - lastHeapLog < HEAP_LOG_INTERVAL_MS) return;
  lastHeapLog = now;
  serialPrintln("Heap: free=" + String(ESP.getFreeHeap()) +
                " largestBlock=" + String(ESP.getMaxAllocHeap()) +
                " minFree=" + String(ESP.getMinFreeHeap()));
#endif
}

void presenceTick() {
  // Reset watchdog
  esp_task_wdt_reset();

  logHeapStats();

  if (configMode) {
    dnsServer.processNextRequest();
    server.handleClient();
    blinkBlueHeartbeat();
    // Check for reset button even in config mode
    checkResetButtonHeld();
    return;
  }

  // Normal operation
  if (runModeServicesActive) {
    ArduinoOTA.handle();
    server.handleClient();
  }
  checkResetButtonHeld();

  readSensorData();
  parseLD2410CSerial();
  if (serviceModeActive) {
    blinkPurpleHeartbeat();
  } else {
    updateLED();
  }

  controlLight();

#ifdef ENABLE_HOMEKIT
  homeKitLoop();
#endif

  delay(MAIN_LOOP_DELAY_MS);
}

void setup() {
  presenceInit();
}

void loop() {
  presenceTick();
}
