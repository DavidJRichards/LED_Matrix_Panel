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
#include "matrix_pulse_manager.h"

#define TRIGGER_PIN 27//14  // Scope Channel 1 -> Fixed 1600ns Positive Trigger Pulse
#define ENABLE_PIN  15  // Scope Channel 2 -> Active LOW Pulse, then HIGH Remainder
//#define POT_PIN     26  

MatrixPulseManager pulseManager;

volatile uint32_t debug_pulse_counter = 0;
volatile uint32_t debug_current_low_ns = 0;


#if 0
#include <DNSServer.h>  // attempt to make autoload 
const byte DNS_PORT = 53;
DNSServer dnsServer;
#endif

void onOTAStart() {
//  Serial.println("OTA Update Started. Unmounting Filesystem...");

    isUpdating = true;

  LittleFS.end(); // Safely closes all active files and drops the block handles

            print_line(0, ">> WARNING: ElegantOTA UPDATE  ");
            print_line(1, ">> IS ACTIVELY RUNNING     ");
            //print_line(2, "");
            print_line(3, "Do not cycle power line grids.    ");
}

void onOTAProgress(size_t current, size_t final) {
    static unsigned long ota_progress_millis = 0;
    char buff[30]="Line 2";
    // 1. Calculate custom metric
    int percentage = (current * 100L) / final;
 
   if (millis() - ota_progress_millis > 250) {
    ota_progress_millis = millis();
    Serial.printf("OTA Progress: %u bytes\n", current);
    // 2. Execute your own extended behavior (e.g., update a physical LED bar or display)
//    Serial.printf("Extended Progress Tracker: %d%%\n", percentage);
    sprintf(buff,"Progress %u %u", current, final);
    print_line(2,buff);
  }   

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
    print_line(0, ">> SUCCESS: UPDATE INSTALLED      ");
    print_line(1, ">> FLASH SYNCHRONIZED    ");
    print_line(2, "Restarting microprocessor...      ");
    print_line(3, "Please stay clear safely.         ");
    delay(1500);

  }
  isUpdating = false;

}

void setup() {
    // 1. Initialize the USB hardware serial debugging pipeline
    Serial.begin(115200);


    // ====================================================================
    // INITIAL PINMODES FOR STABLE HARDWARE INITIALIZATION
    // Prevents display and control lines from floating during the 3s pause
    // ====================================================================
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    
    // Explicit hardware setup for your recovery configuration override pin
    #ifdef CONFIG_PIN
    pinMode(CONFIG_PIN, INPUT_PULLUP);
    #endif

    // ====================================================================
    // RESTORED: DYNAMIC SELF-NAMING CHIP REGISTER ENGINE
    // This must execute at the absolute top so syncExternalHardware() 
    // can read your unique MAC address suffix during the 3-second USB pause!
    // ====================================================================
    uint8_t mac[6];
    WiFi.macAddress(mac); // Fetches the unique 6-byte hardware chip ID
    
    char uniqueSuffix[5];
    snprintf(uniqueSuffix, sizeof(uniqueSuffix), "%02x%02x", mac[4], mac[5]); // Grab last 4 hex characters
    
    dynamicHostname = "ledmatrix-" + String(uniqueSuffix);
    dynamicApSSID   = "LedMatrix_Portal";
    currentTargetSSID = "Searching..."; // Initial state before scan completes

    isBooting = true; // Engage your custom boot-screen freeze layout

    // Time-Bounded Serial Monitor Handshake Pass (Max 3 seconds)
    unsigned long serialWaitStart = millis();
    while (!Serial) {
        if (millis() - serialWaitStart >= 3000) {
            break; 
        }
        
        // This loop now safely streams your unique "Host Id: ledmatrix-xxxx" 
        // to your external panels instantly while waiting for the USB port!
        syncExternalHardware();
        delay(10); 
    }

    if (Serial) {
        delay(200);      
        Serial.flush();  
        Serial.println(); 
    }
    
    // ... Keep the remainder of your hardware pin initializations and LittleFS calls exactly as they are ...

    Serial.println("[SYSTEM] Debugging stream linked successfully. Commencing initialization...");

    // 2. Initialize physical hardware components and custom CGRAM font profiles
    initMyHardware();
    Serial.println("[SYSTEM] Motherboard peripheral pins and custom font profiles armed.");

    // Force an immediate refresh to display your custom initialization messages right away
    syncExternalHardware();

    // 3. Mount and check the local flash file storage partition
    if (!LittleFS.begin()) {
        Serial.println("[CRITICAL ERROR] LittleFS flash storage partition failed to mount!");
    } else {
        Serial.println("[LittleFS] Storage partition mounted cleanly.");

        // ====================================================================
        // RESTORED CODE: CORE SYSTEM DATABASE FILE LOAD PIPELINE
        // Restores your saved parameters into global RAM array slots immediately on boot
        // ====================================================================
        File file = LittleFS.open("/networks.json", "r");
        if (file) {
            JsonDocument doc;
            if (deserializeJson(doc, file) == DeserializationError::Ok) {
                JsonArray storedProfiles = doc.as<JsonArray>();
                
                // If profiles exist, pre-load the baseline profile attributes into active registers
                if (storedProfiles.size() > 0) {
                    // Pull indices from the first record fallback profile safely via JsonVariant
                    JsonVariant profile = storedProfiles[0];
                    
                    strlcpy(appText1, profile["txt1"] | "$TIMME$", sizeof(appText1));
                    strlcpy(appText2, profile["txt2"] | "$HOSTNAME$", sizeof(appText2));
                    strlcpy(appText3, profile["txt3"] | "$IP_ADDR$", sizeof(appText3));
                    strlcpy(appText4, profile["txt4"] | "djrm", sizeof(appText4));
                    
                    appFeatureFlag = profile["feat_flag"] | false;
                    ledMatrixBrightness = profile["vfd_bright"] | 35;
                    bellIntervalMinutes = profile["bell_int"] | 0;
                    
                    Serial.println("[DATABASE] Successfully loaded message text matrices into running RAM registers on boot!");
                    Serial.print("[DATABASE] Line 1 Text: "); Serial.println(appText1);
                } else {
                    Serial.println("[DATABASE INFO] Profiles array is currently empty. Waiting for web GUI configuration.");
                }
            } else {
                Serial.println("[DATABASE ERROR] Failed to parse networks.json formatting structures.");
            }
            file.close();
        } else {
            Serial.println("[DATABASE INFO] No existing /networks.json config file found. Instantiating fresh environment.");
        }
    }
    
    // ====================================================================
    // CONFIGURATION PIN HARDWARE OVERRIDE HOOK
    // Forces the board directly into AP Setup mode if the physical pin is shorted
    // ====================================================================
    bool forceAPMode = false;
    #ifdef CONFIG_PIN
    if (digitalRead(CONFIG_PIN) == LOW) { 
        Serial.println("[SYSTEM] Hardware CONFIG_PIN detected LOW! Forcing configuration portal mode manually.");
        forceAPMode = true;
    }
    #endif

    // 4. Connect to Network or Fall Back into standalone Access Point (AP) mode
    if (forceAPMode) {
        isBooting = false;
        startConfigPortal();
    } else {
        // Attempt connecting to saved configuration profiles inside LittleFS
        if (!startConfigAndConnect()) {
            Serial.println("[SYSTEM] No saved profiles available or in range. Reverting to local fallback portal.");
            isBooting = false; 
            startConfigPortal();
        }
    }

    // Attach ElegantOTA dashboard at /update
    ElegantOTA.begin(&server);    
    ElegantOTA.onStart(onOTAStart); // Register the pre-flash hook
    ElegantOTA.onProgress(onOTAProgress); // Register the pre-flash hook
    // Register the end hook to refresh mid-runtime
    ElegantOTA.onEnd(onOTAEnd);

     // Synchronize your independent loop timer variables right at the finish line
    lastDisplayUpdate = millis(); 
    lastIntervalBellTime = millis();  // Sync chime tracker
    lastWatchdogCheckTime = millis(); // Sync watchdog tracker
    
    Serial.println("[SYSTEM] Initialization complete. Dropping into asynchronous state loop.\n");
}


void loop() {
#if 0
    // 1. ADD THIS: Keep processing background DNS rerouting
    dnsServer.processNextRequest();
#endif
    // Keep web routes active at all times to prevent unresponsive port 80 blocks
    server.handleClient();
    
    // Service network name discovery profiles
    MDNS.update(); 

    ElegantOTA.loop();

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
            // ====================================================================
            // FIXED STARTUP TIMING HANDSHAKE
            // Synchronize your loop timer variables right at the finish line.
            // This guarantees your panels capture the boot text instead of skipping it!
            // ====================================================================
            lastDisplayUpdate = millis(); 
            Serial.println("[SYSTEM] Initialization complete. Dropping into asynchronous state loop.\n");

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
        led_brightness = read_ldr_brightness(currentMillis);
    } else {
        digitalWrite(LED_BUILTIN, LOW);  // LED stays off when flag is INACTIVE
        read_ldr_brightness(currentMillis); // ignore value, keep filter running
        led_brightness = ledMatrixBrightness;
    }

#if 0
   // Send the integer directly to the manager class API
    pulseManager.set_width_percent(led_brightness);
#endif

    // scan_and_render_display(led_brightness); used to be called here before split off to loop1() task

    // ====================================================================
    // 2. AUTOMATED REGULAR INTERVAL CHIME SUPERVISOR
    // FIXED: Uses its own isolated lastIntervalBellTime tracker clock!
    // ====================================================================
    if (bellIntervalMinutes > 0 && !isConnectingWifi && !isPortalMode && !isBooting) {
        unsigned long intervalMsTarget = (unsigned long)bellIntervalMinutes * 60000;

        if (currentMillis - lastIntervalBellTime >= intervalMsTarget) {
            lastIntervalBellTime = currentMillis; // FIXED: Added missing timer step update!
            ringBell(); // Trigger your distinct 3-pulse audio signal burst
        }
    }

    // ====================================================================
    // 3. BACKGROUND NETWORK HEALTH & ROAMING WATCHDOG
    // FIXED: Employs its own dedicated lastWatchdogCheckTime clock to prevent
    // reset loops from crashing your open AP hotspot mode!
    // ====================================================================
    if (!isPortalMode && !isConnectingWifi && !isBooting && (currentMillis - lastWatchdogCheckTime >= checkInterval)) {
        lastWatchdogCheckTime = currentMillis; // Advance the isolated watchdog clock window
        
        uint8_t currentStatus = WiFi.status();
        if (currentStatus != 3) { // 3 = WL_CONNECTED
            Serial.println("[WATCHDOG] Connection dropped! Re-initiating scan maps...");
            server.stop();
            startConfigAndConnect();
        } else if (WiFi.RSSI() < RSSI_THRESHOLD) {
            Serial.println("[WATCHDOG] Signal threshold depleted. Roaming to stronger profile...");
            server.stop();
            WiFi.disconnect();
            startConfigAndConnect();
        }
    }
}
