#include "config.h"

SerialPIO VfdSerial(VFD_TX_PIN, -1);
WebServer server(80);

const char* CONFIG_AP_SSID = "PicoW_Config_Portal";

unsigned long lastCheckTime = 0;
const unsigned long checkInterval = 45000;
const int RSSI_THRESHOLD = -88;
bool isPortalMode = false;

char appText1[65] = {0};
char appText2[65] = {0};
char appText3[65] = {0};
char appText4[65] = {0};

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
int ledMatrixBrightness = 60; // Default startup intensity register
