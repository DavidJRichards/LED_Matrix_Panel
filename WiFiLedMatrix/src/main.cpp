#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <LEAmDNS.h>
#include <time.h>

#include "config.h"
#include "hardware.h"
#include "network_engine.h"

void setup() {
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(CONFIG_PIN, INPUT_PULLUP);

    initMyHardware();
    led_setup();

    printAtTextRow(0, "System Initialization...", 36);

    if (!LittleFS.begin()) {
        printAtTextRow(1, "FS MOUNT FAILURE! Format Required.", 36);
        return;
    }

    File file = LittleFS.open("/networks.json", "r");
    if (file) {
        JsonDocument doc;
        if (deserializeJson(doc, file) == DeserializationError::Ok) {
            JsonArray arr = doc.as<JsonArray>();
            if (arr.size() > 0) {
                JsonObject prof = arr[0].as<JsonObject>();
                strlcpy(appText1, prof["txt1"] | "", sizeof(appText1));
                strlcpy(appText2, prof["txt2"] | "", sizeof(appText2));
                strlcpy(appText3, prof["txt3"] | "", sizeof(appText3));
                strlcpy(appText4, prof["txt4"] | "", sizeof(appText4));
                variableMsg = String(appText1);
                 // FIXED NON-VOLATILE HOOK: Recover your flag setting right on bootup!
                appFeatureFlag = prof["feat_flag"] | false; // Defaults to false if empty
                    // Recover your brightness layout settings on initial boot
                ledMatrixBrightness  = prof["vfd_bright"] | 60; // Defaults to 600 if empty
       }
        }
        file.close();
    }

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    if (digitalRead(CONFIG_PIN) == LOW || !LittleFS.exists("/networks.json")) {
        startConfigPortal();
    } else {
        if (!startConfigAndConnect()) {
            printAtTextRow(1, "No saved profile in range.", 36);
            startConfigPortal();
        }
    }

    syncExternalHardware(); 

    clearDisplay();
    refreshDisplay();
}



void loop() {
    // Keep web routes active at all times to prevent unresponsive port 80 blocks
    server.handleClient();
    
    // Service network name discovery profiles
    MDNS.update(); 

    // Service the non-blocking asynchronous Wi-Fi link step checker
    checkWifiConnectionStep();

    unsigned long currentMillis = millis();

    // ====================================================================
    // 1. INDEPENDENT HIGH-SPEED HARDWARE TIME TICKER
    // This runs completely unhooked from the VFD display timers!
    // ====================================================================
    static int lastSentSecond = -1;
    time_t now = time(nullptr);
    struct tm* timeInfo = localtime(&now);
    int currentSecond = timeInfo->tm_sec;

    // High-speed, heap-safe standard C strstr check inside the raw char buffers
    bool usesTimeMacro = (strstr(appText1, "$TIME$") != nullptr || 
                          strstr(appText2, "$TIME$") != nullptr || 
                          strstr(appText3, "$TIME$") != nullptr || 
                          strstr(appText4, "$TIME$") != nullptr);

    // If the macro is present and the atomic second increments, update your custom output instantly!
    if (usesTimeMacro && currentSecond != lastSentSecond && !isConnectingWifi && !isPortalMode) {
        lastSentSecond = currentSecond;
        
        // Pushes the freshly ticked time digits straight to lines 0-3 of your external hardware
        syncExternalHardware(); 
    }

    // ====================================================================
    // 2. SLOW STATIC DISPLAY STEPPER (10,000ms / 10 Seconds)
    // ====================================================================
    if (currentMillis - lastDisplayUpdate >= displayInterval) {
        lastDisplayUpdate = currentMillis;
        
        // Keep driving your 5-step interleaved row compositor safely at slow speeds
        refreshDisplay();
    }

    // Automated background network supervision and roaming tracking
    if (!isPortalMode && !isConnectingWifi && (currentMillis - lastCheckTime >= checkInterval)) {
        lastCheckTime = currentMillis;
        
        if (WiFi.status() != WL_CONNECTED) {
            server.stop();
            startConfigAndConnect();
        } else if (WiFi.RSSI() < RSSI_THRESHOLD) {
            server.stop();
            WiFi.disconnect();
            startConfigAndConnect();
        }
    }
   //scan_and_render_display();
    // ====================================================================
    // FIXED: Non-Volatile Feature Flag Controls the On-Board LED
    // Maps the physical hardware pin state directly to your web setting!
    // ====================================================================
    if (appFeatureFlag) {
        digitalWrite(LED_BUILTIN, HIGH); // LED stays on when flag is ACTIVE
        if(ledMatrixBrightness>0)
            scan_and_render_display();
    } else {
        digitalWrite(LED_BUILTIN, LOW);  // LED stays off when flag is INACTIVE
    }
}
