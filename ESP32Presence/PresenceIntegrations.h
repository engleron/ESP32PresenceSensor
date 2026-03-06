#pragma once

#include "PresenceState.h"

bool sendISYCommand(const String& url);
void turnLightOn();
void turnLightOff();
String insteonAddrToHex(const String& addr);
bool sendInsteonHubCommand(bool on);
bool sendHACommand(bool on);
bool sendHASensorState(const String& state);
void updateHASensorEntity();
void initIntegrationWorker();
void controlLight();

#ifdef ENABLE_HOMEKIT
void initHomeKit();
void homeKitLoop();
void updateHomeKitState();
#endif
