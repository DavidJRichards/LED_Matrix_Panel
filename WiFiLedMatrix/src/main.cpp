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
    // FIXED: Web interface handler runs ALWAYS to ensure port 80 is never unresponsive
    server.handleClient();
    
    // Service local network name discovery packets over the air
    MDNS.update(); 

    // Service the non-blocking background Wi-Fi connection handshake step engine
    checkWifiConnectionStep();

    unsigned long currentMillis = millis();

    // Asynchronous text page updates and display maintenance
    if (currentMillis - lastDisplayUpdate >= displayInterval) {
        lastDisplayUpdate = currentMillis;

        // Skip background clock alterations if the portal page is active
        if (!isPortalMode && WiFi.status() == WL_CONNECTED && !isConnectingWifi) {
            time_t now = time(nullptr);
            struct tm* timeInfo = localtime(&now);
            char clockRowBuf[37] = {0};

            if (timeInfo->tm_year > 70) {
                snprintf(clockRowBuf, sizeof(clockRowBuf), "------------- %02d:%02d:%02d -------------", 
                         timeInfo->tm_hour, timeInfo->tm_min, timeInfo->tm_sec);
            } else {
                snprintf(clockRowBuf, sizeof(clockRowBuf), "---------- Syncing Clock -----------");
            }
            fixedMsgLine2 = String(clockRowBuf);
        }

        // Keep driving your interleaved single-row state machine ticks
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
        scan_and_render_display();
    } else {
        digitalWrite(LED_BUILTIN, LOW);  // LED stays off when flag is INACTIVE
    }
}

