#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include <SerialPIO.h>

// --- Physical Hardware Pin Maps ---
#define BELL_PIN      17
#define VFD_RESET_PIN 18 //3 blue
#define VFD_TX_PIN    19 //8 green
#define VFD_BUSY_PIN  20 //9 yellow resistor red 
#define CONFIG_PIN    16 // Physical button jumper to GND to clear files / enter AP mode
#define LDR_PIN       26

// --- Global Core Drivers & Infrastructure ---
extern SerialPIO VfdSerial;

// --- Global Wi-Fi and Core Infrastructure State ---
extern const char* CONFIG_AP_SSID;  // <-- Ensure this line is present
extern WebServer server;
extern unsigned long lastCheckTime;
extern WebServer server;

// --- Global State Variables ---
extern unsigned long lastCheckTime;
extern const unsigned long checkInterval;
extern const int RSSI_THRESHOLD;
extern bool isPortalMode;

// --- Global Character String Containers ---
#define LED_TEXT_SIZE 250
extern char appText1[LED_TEXT_SIZE+1];
extern char appText2[LED_TEXT_SIZE+1];
extern char appText3[LED_TEXT_SIZE+1];
extern char appText4[LED_TEXT_SIZE+1];

// --- Persistent Screen Variables ---
extern String fixedMsgLine1;
extern String fixedMsgLine2;
extern String variableMsg;

extern unsigned long lastDisplayUpdate;
extern const unsigned long displayInterval;

// FIXED: Add these global state references so main.cpp can track them
extern bool isConnectingWifi;
extern unsigned long wifiStepStart;
extern int wifiAttemptCount;
extern int bestProfileIndex;

// Add these to the absolute bottom of src/config.h if they aren't there already:
extern bool isConnectingWifi;
extern unsigned long wifiStepStart;
extern int wifiAttemptCount;
extern int bestProfileIndex; // <--- Crucial for tracking the manual profile jumper link!
// Custom application feature variables
extern bool appFeatureFlag; // Toggled via the web portal button

// Add this line at the absolute bottom of your existing src/config.h:
extern int ledMatrixBrightness; // Active brightness level (0 - 100)
extern const unsigned long FILTER_INTERVAL_MS; // used for LDR ambient brightness
extern const int FILTER_SHIFT;
extern const int FILTER_WEIGHT; 

// Add these to the absolute bottom of src/config.h
extern int bellPulseCounter;
extern unsigned long lastBellPulseTime;

// --- Regular Interval Chime Registers ---
extern int bellIntervalMinutes;         // 0 = Off, otherwise interval in minutes
extern unsigned long lastIntervalBellTime; // Non-blocking timer clock
