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
            }
        }
        file.close();
    }

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    // Check if configuration mode is forced via hardware jumper or if empty storage detected
    if (digitalRead(CONFIG_PIN) == LOW || !LittleFS.exists("/networks.json")) {
        startConfigPortal();
    } else {
        // Launch your non-blocking network connection handshake sequence
        if (!startConfigAndConnect()) {
            printAtTextRow(1, "No saved profile in range.", 36);
            startConfigPortal();
        }
    }
    // CRITICAL HOOK: Fires the initialization call to sync external channels at boot
    syncExternalHardware(); 

    clearDisplay();
    refreshDisplay();
}

void loop() {
    // FIXED: Service your web routes if connected to a router OR if running the setup portal!
    if (isPortalMode || WiFi.status() == WL_CONNECTED) {
        server.handleClient();
    }
    
    // Service local network name discovery packets over the air
    MDNS.update(); 

    // Service the non-blocking background Wi-Fi connection handshake step engine
    checkWifiConnectionStep();

    unsigned long currentMillis = millis();

    // Asynchronous text page updates and display maintenance
    if (currentMillis - lastDisplayUpdate >= displayInterval) {
        lastDisplayUpdate = currentMillis;

        // Skip background string modifications if the portal page is open
        if (!isPortalMode && WiFi.status() == WL_CONNECTED && !isConnectingWifi) {
            time_t now = time(nullptr);
            struct tm* timeInfo = localtime(&now);
            char clockRowBuf[50] = {0};

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

    scan_and_render_display();

}
