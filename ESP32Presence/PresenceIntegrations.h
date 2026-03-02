#pragma once

#include "PresenceState.h"

bool sendISYCommand(const String& url);
void turnLightOn();
void turnLightOff();
String insteonAddrToHex(const String& addr);
bool sendInsteonHubCommand(bool on);
bool sendHACommand(bool on);
void initIntegrationWorker();
void controlLight();
