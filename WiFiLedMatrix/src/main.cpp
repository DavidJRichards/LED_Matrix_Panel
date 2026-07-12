#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <LEAmDNS.h>
#include <time.h>

#include <ElegantOTA.h>
#include "config.h"
#include "hardware.h"
#include "network_engine.h"


void onOTAStart() {
//  Serial.println("OTA Update Started. Unmounting Filesystem...");
  LittleFS.end(); // Safely closes all active files and drops the block handles
}

// Callback function triggered immediately after upload blocks are written
void onOTAEnd(bool success) {
  if (success) {
    //Serial.println("OTA block write complete. Reinitialising filesystem...");
    
    // 1. Force release of the old block allocation handles
    //LittleFS.end(); 
    //delay(100);
    
    // 2. Remount the filesystem to parse the newly uploaded 'littlefs.bin'
    if (LittleFS.begin()) {
      //Serial.println("Filesystem successfully reinitialised without reboot!");
    } else {
      //Serial.println("Failed to mount the newly uploaded filesystem partition.");
    }
  }
}

//pico_ota picoOta;

void setup() {
    // 1. Initialize the USB hardware serial debugging pipeline
    Serial.begin(115200);

    // Time-Bounded Serial Monitor Handshake Pass
    unsigned long serialWaitStart = millis();
    while (!Serial) {
        if (millis() - serialWaitStart >= 3000) {
            break; 
        }
        delay(10); 
    }

    // ====================================================================
    // FIXED BUFFER PASS: Allow the built-in terminal cache to stabilize
    // ====================================================================
    if (Serial) {
        delay(200);      // Give the VS Code monitoring pane 200ms to open up
        Serial.flush();  // Force the internal chip hardware queues to clear completely
        Serial.println(); // Print a clean empty spacer line to sync the terminal rows
    }

    Serial.println("[SYSTEM] Debugging stream linked successfully. Commencing initialization...");

    // 2. Initialize physical hardware components and custom CGRAM font profiles
    initMyHardware();
    Serial.println("[SYSTEM] Motherboard peripheral pins and custom font profiles armed.");
    
    // ... Keep the remainder of your setup() loading routines exactly as they are ...
    // 3. Mount and check the local flash file storage partition
    if (!LittleFS.begin()) {
        Serial.println("[CRITICAL ERROR] LittleFS flash storage partition failed to mount!");
    } else {
        Serial.println("[LittleFS] Storage partition mounted cleanly.");
    }

    // ... Keep the remainder of your setup() loading routines and json file parsing loops exactly as they are ...

    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(CONFIG_PIN, INPUT_PULLUP);
 
    analogReadResolution(ADC_RESOLUTION);
    pinMode(LDR_PIN, INPUT);   

    led_setup();

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
                bellIntervalMinutes = prof["bell_int"] | 0;
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

  // Attach ElegantOTA dashboard at /update
  ElegantOTA.begin(&server);    
  ElegantOTA.onStart(onOTAStart); // Register the pre-flash hook
  // Register the end hook to refresh mid-runtime
  ElegantOTA.onEnd(onOTAEnd);
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

    // 4. Driven rapidly at 50ms intervals for custom output scrolling loops
    if (currentMillis - lastDisplayUpdate >= 50) {
        lastDisplayUpdate = currentMillis;
        
        // STATIC REFRESH CLOCK TICKER
        // As long as the device is not handshaking or running the local configuration portal,
        // it continuously dispatches background matrix scroll slices to your custom hardware!
        if (!isConnectingWifi && !isPortalMode) {
            syncExternalHardware(); 
        }
        
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
   

    // ====================================================================
    // FIXED: Non-Volatile Feature Flag Controls the On-Board LED
    // Maps the physical hardware pin state directly to your web setting!
    // ====================================================================
    if (appFeatureFlag) {
        digitalWrite(LED_BUILTIN, HIGH); // LED stays on when flag is ACTIVE
        int led_brightness = read_ldr_brightness(currentMillis);
        scan_and_render_display(led_brightness);
    } else {
        digitalWrite(LED_BUILTIN, LOW);  // LED stays off when flag is INACTIVE
    //    digitalWrite(LED_BUILTIN, HIGH); // LED stays on when flag is ACTIVE
    //    int led_brightness = read_ldr_brightness(currentMillis);
        scan_and_render_display(ledMatrixBrightness);
    }

    // ====================================================================
    // NEW: AUTOMATED REGULAR INTERVAL CHIME SUPERVISOR
    // Evaluates your non-volatile interval clock without dynamic loop delays!
    // ====================================================================
    if (bellIntervalMinutes > 0 && !isConnectingWifi && !isPortalMode) {
        // Convert the user-selected slider minutes target directly to milliseconds
        unsigned long intervalMsTarget = (unsigned long)bellIntervalMinutes * 60000;

        if (currentMillis - lastIntervalBellTime >= intervalMsTarget) {
            lastIntervalBellTime = currentMillis; // Advance the baseline tracking frame
            
            ringBell(); // Sounds the professional 3-pulse acoustic alert burst!
        }
    }



}
