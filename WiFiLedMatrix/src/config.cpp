#include "config.h"

SerialPIO VfdSerial(VFD_TX_PIN, -1);
WebServer server(80);

const char* CONFIG_AP_SSID = "PicoW_Config_Portal";

unsigned long lastCheckTime = 0;
const unsigned long checkInterval = 45000;
const int RSSI_THRESHOLD = -88;
bool isPortalMode = false;

char appText1[LED_TEXT_SIZE+1] = {0};
char appText2[LED_TEXT_SIZE+1] = {0};
char appText3[LED_TEXT_SIZE+1] = {0};
char appText4[LED_TEXT_SIZE+1] = {0};

String fixedMsgLine1 = "IP: Initialising...";
String fixedMsgLine2 = "------------------------------------"; 
String variableMsg   = "Awaiting Scan...";

unsigned long lastDisplayUpdate = 0;
const unsigned long displayInterval = 1000; 

// NON-BLOCKING ASYNCHRONOUS MEMORY STORAGE ALLOCATIONS
bool isConnectingWifi = false;
unsigned long wifiStepStart = 0;
int wifiAttemptCount = 0;
int bestProfileIndex = -1;
// Initialize your application feature state
bool appFeatureFlag = false; // Starts off by default

// Add this line at the absolute bottom of your existing src/config.cpp:
int ledMatrixBrightness = 35; // Default startup intensity register
int led_brightness = ledMatrixBrightness;

const unsigned long FILTER_INTERVAL_MS = 50; 
// Shift factor of 6 means internal values are scaled by 64 (2^6).
// A filter weight of 1 creates a slow, smooth response (~3 second transition time at 50ms intervals).
const int FILTER_SHIFT = 6;
const int FILTER_WEIGHT = 3; 

// Add these to the absolute bottom of src/config.cpp
int bellPulseCounter = 0; // Tracks active pulse count
unsigned long lastBellPulseTime = 0; // Non-blocking timer clock

// Initialize regular interval chime states
int bellIntervalMinutes = 0;         // Default: 0 (Off / Disabled)
unsigned long lastIntervalBellTime = 0; // Tracks historical minutes steps

// Add these to the absolute bottom of src/config.cpp
String dynamicHostname = "ledmatrix"; 
String dynamicApSSID   = "LedMatrix_Portal";

// Add this line at the absolute bottom of src/config.cpp:
bool isBooting = true; // Initializes to true on power-on

// Add this line at the absolute bottom of src/config.cpp:
String currentTargetSSID = ""; // Initializes clean on boot

// Initialize isolated network supervisor timer register
unsigned long lastWatchdogCheckTime = 0; // Tracks 10-second health loops independently

bool isUpdating = false;
