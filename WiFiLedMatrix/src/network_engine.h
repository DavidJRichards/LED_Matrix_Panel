#pragma once

void handleRoot();
void handleSave();
void handleList();
void handleDelete();
void handleScan();
void handleBeep();
void handleCurrentSSID();
void handleUpdateText();
void setupServerRoutes();
void startConfigPortal();
bool loadConfigAndConnect();
// REMOVED: void setupOTA();
// FIXED: Ensure these match the signatures inside network_engine.cpp exactly
bool startConfigAndConnect();
void checkWifiConnectionStep();
// Add this declaration line inside your src/network_engine.h file:
void handleToggleFeature();
