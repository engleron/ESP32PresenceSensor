#pragma once

#include "PresenceState.h"

void sendPageStart(const String& title, const String& extraHead = "");
void sendPageEnd();
void sendNavBar();
String scanWiFiNetworks();
void startWiFiScanAsync();
void handleCaptivePortal();
void sendIntegrationSection(bool isSetup);
void handleSetupRoot();
void handleSetupSave();

void handleLogin();
void handleStatus();
void handleConfig();
void handleReset();
void handleLogout();

void handleApiStatus();
void handleApiConfigExport();
void handleApiLogin();
void handleApiLogout();

void setupWebServerSetupMode();
void setupWebServerRunMode();
