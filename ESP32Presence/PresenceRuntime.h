#pragma once

#include "PresenceState.h"

void blinkBlueHeartbeat();
void blinkRedBlue();
void updateLED();
void readSensorData();
void parseLD2410CSerial();
bool applyLd2410Config();
bool checkResetButton();
bool checkResetButtonHeld();
void enterConfigMode();
void connectToWiFi();

void presenceInit();
void presenceTick();
