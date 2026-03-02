#pragma once

/*
 * ================================================================
 * SECTION: LIGHT CONTROL (EISY/ISY)
 * ================================================================
 */

/*
 * sendISYCommand - Send a REST command to the EISY/ISY controller.
 * @param url Full REST URL to send GET request to
 * @return true if HTTP 200 received, false on error
 */
bool sendISYCommand(const String& url) {
  if (WiFi.status() != WL_CONNECTED) {
    serialPrintln(F("WiFi not connected - cannot send command"));
    return false;
  }

  HTTPClient http;
  bool success = false;

  if (useHTTPS) {
    WiFiClientSecure* client = new WiFiClientSecure;
    if (client) {
      client->setInsecure();  // Accept self-signed certs (local EISY)
      http.begin(*client, url);
      http.setAuthorization(isyUsername.c_str(), isyPassword.c_str());
      int code = http.GET();
      success = (code == 200);
      verbosePrint("ISY HTTPS response: " + String(code));
      http.end();
      delete client;
    }
  } else {
    http.begin(url);
    http.setAuthorization(isyUsername.c_str(), isyPassword.c_str());
    int code = http.GET();
    success = (code == 200);
    verbosePrint("ISY HTTP response: " + String(code));
    http.end();
  }

  return success;
}

/*
 * turnLightOn - Send DON command to EISY/ISY to turn light on.
 */
void turnLightOn() {
  serialPrintln(F("Turning light ON..."));
  String deviceID = isyDeviceID;
  deviceID.replace(" ", "%20");
  String protocol = useHTTPS ? "https" : "http";
  String url = protocol + "://" + isyIP + "/rest/nodes/" + deviceID + "/cmd/DON";

  if (sendISYCommand(url)) {
    lightOn = true;
    serialPrintln(F("Light ON"));
  } else {
    serialPrintln(F("Failed to turn light ON"));
  }
}

/*
 * turnLightOff - Send DOF command to EISY/ISY to turn light off.
 */
void turnLightOff() {
  serialPrintln(F("Turning light OFF..."));
  String deviceID = isyDeviceID;
  deviceID.replace(" ", "%20");
  String protocol = useHTTPS ? "https" : "http";
  String url = protocol + "://" + isyIP + "/rest/nodes/" + deviceID + "/cmd/DOF";

  if (sendISYCommand(url)) {
    lightOn = false;
    serialPrintln(F("Light OFF"));
  } else {
    serialPrintln(F("Failed to turn light OFF"));
  }
}

/*
 * insteonAddrToHex - Convert "1A.2B.3C" to "1A2B3C" for Insteon Hub API.
 * @param addr Insteon address with dot separators
 * @return Hex string without separators, uppercase
 */
String insteonAddrToHex(const String& addr) {
  String h = addr;
  h.replace(".", "");
  h.toUpperCase();
  return h;
}

/*
 * sendInsteonHubCommand - Send a direct ON/OFF command to Insteon Hub 2 (Model 2245).
 * @param on true to turn on, false to turn off
 * @return true if HTTP 200 received
 */
bool sendInsteonHubCommand(bool on) {
  if (WiFi.status() != WL_CONNECTED) return false;
  String hex = insteonAddrToHex(insteonHubAddr);
  String cmd = on ? "11" : "13";

  HTTPClient http;
  // Step 1: clear hub receive buffer
  String clearUrl = "http://" + insteonHubIP + ":" + insteonHubPort + "/1?XB=M=1";
  http.begin(clearUrl);
  http.setAuthorization(insteonHubUser.c_str(), insteonHubPass.c_str());
  http.GET();
  http.end();

  // Step 2: send command
  String cmdUrl = "http://" + insteonHubIP + ":" + insteonHubPort +
                  "/3?0262" + hex + "0F" + cmd + "=I=3";
  http.begin(cmdUrl);
  http.setAuthorization(insteonHubUser.c_str(), insteonHubPass.c_str());
  int code = http.GET();
  http.end();
  verbosePrint("Insteon Hub response: " + String(code));
  return (code == 200);
}

/*
 * sendHACommand - Send turn_on / turn_off to the Home Assistant REST API.
 * @param on true to turn on, false to turn off
 * @return true if HTTP 200 or 201 received
 */
bool sendHACommand(bool on) {
  if (WiFi.status() != WL_CONNECTED) return false;
  String service = on ? "turn_on" : "turn_off";
  String url = String(haHTTPS ? "https" : "http") + "://" + haIP + ":" + haPort +
               "/api/services/homeassistant/" + service;
  String body = "{\"entity_id\":\"" + haEntityId + "\"}";
  bool success = false;

  HTTPClient http;
  if (haHTTPS) {
    WiFiClientSecure* client = new WiFiClientSecure;
    client->setInsecure();
    http.begin(*client, url);
    http.addHeader("Authorization", "Bearer " + haToken);
    http.addHeader("Content-Type", "application/json");
    int code = http.POST(body);
    success = (code == 200 || code == 201);
    verbosePrint("HA HTTPS response: " + String(code));
    http.end();
    delete client;
  } else {
    http.begin(url);
    http.addHeader("Authorization", "Bearer " + haToken);
    http.addHeader("Content-Type", "application/json");
    int code = http.POST(body);
    success = (code == 200 || code == 201);
    verbosePrint("HA HTTP response: " + String(code));
    http.end();
  }
  return success;
}

/*
 * controlLight - Decide whether to turn the light on or off based on presence.
 * Dispatches to the configured integration (ISY, Insteon Hub, or Home Assistant).
 */
void controlLight() {
  if (!integrationConfigured) return;
  if (integrationMode == "homekit") return;  // HomeKit pushes state changes in loop()

  bool shouldBeOn = presenceDetected ||
    (millis() - lastDetectionTime < (unsigned long)noDetectionTimeout * 1000UL);
  if (shouldBeOn == lightOn) return;

  bool ok = false;
  if (shouldBeOn) {
    if      (integrationMode == "isy")         { turnLightOn();  return; }
    else if (integrationMode == "insteon_hub") ok = sendInsteonHubCommand(true);
    else if (integrationMode == "ha")          ok = sendHACommand(true);
    if (ok) lightOn = true;
  } else {
    if      (integrationMode == "isy")         { turnLightOff(); return; }
    else if (integrationMode == "insteon_hub") ok = sendInsteonHubCommand(false);
    else if (integrationMode == "ha")          ok = sendHACommand(false);
    if (ok) lightOn = false;
  }
}
