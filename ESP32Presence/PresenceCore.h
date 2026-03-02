#pragma once

#include "PresenceState.h"

void serialPrint(const String& message);
void serialPrintln(const String& message);
void verbosePrint(const String& message);
void setRGB(int r, int g, int b);
String hashPassword(const String& password);
String generateSessionId();
String getUptimeString();
int wifiSignalToPercent(int rssi);

String createSession();
bool validateSession();
void invalidateSession();
bool isLoginLocked();
bool requireAuth();

void generateDeviceSSID();
void loadConfiguration();
void saveConfiguration();
void clearConfiguration();
String buildConfigJson();
