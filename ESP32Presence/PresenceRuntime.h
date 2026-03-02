#pragma once

#include "PresenceState.h"

void blinkBlueHeartbeat();
void blinkRedBlue();
void updateLED();
void readSensorData();
bool checkResetButton();
bool checkResetButtonHeld();
void enterConfigMode();
void connectToWiFi();

void appSetup();
void appLoop();
