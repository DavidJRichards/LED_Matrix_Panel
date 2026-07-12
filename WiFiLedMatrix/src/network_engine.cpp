#include "network_engine.h"
#include "network_helpers.h"
#include "config.h"
#include "hardware.h"
#include <WiFi.h>
#include <LEAmDNS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <time.h>

void handleRoot() {
    Serial.println("\n[HTTP] Incoming GET request for path: /");
    if (server.hasArg("msg")) {
        variableMsg = server.arg("msg");
        ringBell();
        refreshDisplay();
    }
    String pageBuffer;
    buildPreloadedDashboard(pageBuffer); // Invokes decoupled formatting engine
    server.send(200, "text/html", pageBuffer);
}

void handleSave() {
    Serial.println("\n[HTTP] Incoming POST request for path: /save");
    if (!server.hasArg("ssid")) { 
        server.send(400, "text/plain", "Bad Request: SSID Missing"); 
        return; 
    }
    
    String newSSID = server.arg("ssid");
    String newPass = server.hasArg("password") ? server.arg("password") : "";
    String t1 = server.arg("txt1").substring(0, LED_TEXT_SIZE);
    String t2 = server.arg("txt2").substring(0, LED_TEXT_SIZE);
    String t3 = server.arg("txt3").substring(0, LED_TEXT_SIZE);
    String t4 = server.arg("txt4").substring(0, LED_TEXT_SIZE);
    int bright = server.hasArg("vfd_bright") ? server.arg("vfd_bright").toInt() : 60;
    int interval = server.hasArg("bell_interval") ? server.arg("bell_interval").toInt() : 0;

    // Dispatches properties seamlessly down to the helper file database pipeline
    commitProfileToFlash(newSSID, newPass, t1, t2, t3, t4, bright, interval);
    server.sendContent("HTTP/1.1 303 See Other\r\nLocation: /\r\n\r\n");
}

void handleList() {
    File file = LittleFS.open("/networks.json", "r");
    if (!file) { server.send(200, "application/json", "[]"); return; }
    server.streamFile(file, "application/json");
    file.close();
}

void handleDelete() {
    if (!server.hasArg("index")) { server.send(400, "text/plain", "Missing Index"); return; }
    int idx = server.arg("index").toInt();
    File file = LittleFS.open("/networks.json", "r");
    JsonDocument doc;
    if (file) { deserializeJson(doc, file); file.close(); }
    JsonArray arr = doc.as<JsonArray>();
    if (idx >= 0 && idx < arr.size()) { arr.remove(idx); }
    file = LittleFS.open("/networks.json", "w");
    if (file) { serializeJson(doc, file); file.close(); }
    server.sendContent("HTTP/1.1 303 See Other\r\nLocation: /\r\n\r\n");
}

void handleScan() {
    server.send(200, "application/json", executeAirwaveNetworkScan());
}

void handleManualBeep() {
    ringBell();
    server.send(200, "text/plain", "OK");
}

void handleCurrentSSID() {
    server.send(200, "text/plain", (WiFi.status() == WL_CONNECTED) ? WiFi.SSID() : "OFFLINE");
}

void handleUpdateText() {
    Serial.println("\n[HTTP] Incoming POST request for path: /updatetext");
    if (WiFi.status() != WL_CONNECTED) { server.send(400, "text/plain", "Not Connected"); return; }

    int bright = server.hasArg("vfd_bright") ? server.arg("vfd_bright").toInt() : 60;
    int interval = server.hasArg("bell_interval") ? server.arg("bell_interval").toInt() : 0;
    String currentSSID = WiFi.SSID();

    String t1 = server.arg("txt1").substring(0, LED_TEXT_SIZE);
    String t2 = server.arg("txt2").substring(0, LED_TEXT_SIZE);
    String t3 = server.arg("txt3").substring(0, LED_TEXT_SIZE);
    String t4 = server.arg("txt4").substring(0, LED_TEXT_SIZE);

    File file = LittleFS.open("/networks.json", "r");
    JsonDocument doc;
    if (file) { deserializeJson(doc, file); file.close(); }

    JsonArray arr = doc.as<JsonArray>();
    bool updated = false;
    for (JsonObject profile : arr) {
        if (profile["ssid"].as<String>() == currentSSID) {
            profile["txt1"] = t1; profile["txt2"] = t2;
            profile["txt3"] = t3; profile["txt4"] = t4;
            profile["vfd_bright"] = bright;
            profile["bell_int"] = interval;
            updated = true; break;
        }
    }

    if (updated) {
        file = LittleFS.open("/networks.json", "w");
        if (file) { serializeJson(doc, file); file.close(); }

        strlcpy(appText1, t1.c_str(), sizeof(appText1));
        strlcpy(appText2, t2.c_str(), sizeof(appText2));
        strlcpy(appText3, t3.c_str(), sizeof(appText3));
        strlcpy(appText4, t4.c_str(), sizeof(appText4));
        
        ledMatrixBrightness = bright;
        bellIntervalMinutes = interval;
        lastIntervalBellTime = millis();

        syncExternalHardware();
        fixedMsgLine1 = "IP: " + WiFi.localIP().toString();

        if (t1.indexOf("^g") >= 0 || t1.indexOf("^G") >= 0 || t1.indexOf("\\x07") >= 0 || t1.indexOf((char)0x07) >= 0 ||
            t2.indexOf("^g") >= 0 || t2.indexOf("^G") >= 0 || t2.indexOf("\\x07") >= 0 || t2.indexOf((char)0x07) >= 0 ||
            t3.indexOf("^g") >= 0 || t3.indexOf("^G") >= 0 || t3.indexOf("\\x07") >= 0 || t3.indexOf((char)0x07) >= 0 ||
            t4.indexOf("^g") >= 0 || t4.indexOf("^G") >= 0 || t4.indexOf("\\x07") >= 0 || t4.indexOf((char)0x07) >= 0) {
            ringBell(); 
        }
    }
    server.sendContent("HTTP/1.1 303 See Other\r\nLocation: /\r\n\r\n");
}

// split --------

// ====================================================================
// FIXED: RESTORED NON-VOLATILE TOGGLE HANDLER FUNCTION
// ====================================================================
void handleToggleFeature() {
    appFeatureFlag = !appFeatureFlag;
    ringBell(); 

    File file = LittleFS.open("/networks.json", "r");
    JsonDocument doc;
    if (file) { deserializeJson(doc, file); file.close(); }

    String currentSSID = WiFi.SSID();
    JsonArray arr = doc.as<JsonArray>();
    bool profileMatched = false;

    if (!arr.isNull()) {
        for (JsonObject profile : arr) {
            if (profile["ssid"].as<String>() == currentSSID) {
                profile["feat_flag"] = appFeatureFlag;
                profileMatched = true;
                break;
            }
        }
    }

    if (!profileMatched && !arr.isNull() && arr.size() > 0) {
        // Safe bracket lookup protection
        JsonObject profile = arr[0].as<JsonObject>(); 
        profile["feat_flag"] = appFeatureFlag;
    }

    file = LittleFS.open("/networks.json", "w");
    if (file) { serializeJson(doc, file); file.close(); }

    if (appFeatureFlag) {
        server.send(200, "text/plain", "ACTIVE");
    } else {
        server.send(200, "text/plain", "INACTIVE");
    }
}

void handleSelectProfile() {
    if (!server.hasArg("index")) { server.send(400, "text/plain", "Missing Index"); return; }
    int targetIndex = server.arg("index").toInt();

    File file = LittleFS.open("/networks.json", "r");
    if (!file) { server.send(500, "text/plain", "FS Error"); return; }
    JsonDocument doc;
    deserializeJson(doc, file);
    file.close();

    JsonArray storedProfiles = doc.as<JsonArray>();
    if (targetIndex >= 0 && targetIndex < storedProfiles.size()) {
        JsonObject profile = storedProfiles[targetIndex].as<JsonObject>();
        String targetSSID = profile["ssid"].as<String>();
        String targetPass = profile["password"].as<String>();

        // VFD Progress Tracking
        printAtTextRow(0, ">> MANUAL OVERRIDE PORTAL", 36);
        printAtTextRow(1, "Target: " + targetSSID, 36);
        printAtTextRow(2, "Resetting wireless...", 36);
        printAtTextRow(3, "Please wait safely...", 36);

        // ====================================================================
        // DUPLICATED SERIAL ECHO
        // ====================================================================
        Serial.println("\n--- VFD ROW PRINT ECHO ---");
        Serial.println("Row 0: >> MANUAL OVERRIDE PORTAL");
        Serial.println("Row 1: Target: " + targetSSID);
        Serial.println("Row 2: Resetting wireless...");
        Serial.println("Row 3: Please wait safely...");
        Serial.println("--------------------------\n");

        String transitionHtml = 
            "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1.0'>"
            "<title>Switching Networks</title>"
            "<style>body{font-family:sans-serif;text-align:center;padding:50px;background:#f7f9fa;color:#333;}</style>"
            "<script>"
            "setTimeout(function(){ window.location.href = 'http://ledmatrix.local'; }, 3500);"
            "</script></head><body>"
            "<h2>\xF0\x9F\x84\x94 Switching Network Profiles...</h2>"
            "<p>Connecting terminal to <strong>" + targetSSID + "</strong>.</p>"
            "<p>Please allow 3 seconds for the radio layers to settle.</p>"
            "</body></html>";

        server.send(200, "text/html", transitionHtml);
        delay(150); // Let the browser packet clear the airwaves

        // ====================================================================
        // FIXED HARDWARE TEARDOWN: SEND GOODBYE AND ALLOW STREAM TO CLEAR
        // ====================================================================
        Serial.println("[mDNS] Sending Goodbye Packet to local router table...");
        MDNS.end(); // Generates and enqueues the TTL=0 goodbye packet
        
        // CRUCIAL: Allow the CYW43 core chip exactly 150ms to push the packet out the antenna
        delay(150); 
        
        Serial.println("[RADIO] Stopping active web port 80 listener channels...");
        server.stop();
        
        Serial.println("[RADIO] Gracefully dropping old radio station links...");
        WiFi.softAPdisconnect(true);
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF); 
        delay(100);
        
        WiFi.mode(WIFI_STA); 
        delay(100);

        if (targetPass.length() == 0 || targetPass == "") {
            WiFi.begin(targetSSID.c_str());
        } else {
            WiFi.begin(targetSSID.c_str(), targetPass.c_str());
        }

        isConnectingWifi = true;
        bestProfileIndex = targetIndex;
        wifiStepStart = millis();
        wifiAttemptCount = 0;
    } else {
        server.send(400, "text/plain", "Invalid Profile Index");
    }
}


void setupServerRoutes() {
    server.on("/", HTTP_GET, handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.on("/list", HTTP_GET, handleList);
    server.on("/delete", HTTP_GET, handleDelete);
    server.on("/scan", HTTP_GET, handleScan);
    server.on("/portal-beep", HTTP_GET, handleManualBeep);
    server.on("/current-ssid", HTTP_GET, handleCurrentSSID);
    server.on("/updatetext", HTTP_POST, handleUpdateText);
    server.on("/select-profile", HTTP_GET, handleSelectProfile);
    server.on("/toggle-feature", HTTP_GET, handleToggleFeature);
}

void startConfigPortal() {
    isPortalMode = true;
    MDNS.end();
    server.stop();
    
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);
    WiFi.softAP("Led_Matrix_Config_Portal");
    
    setupServerRoutes();
    server.begin();

    if (MDNS.begin("ledmatrix")) {
        MDNS.addService("http", "tcp", 80);
    }

    fixedMsgLine1 = "SETUP MODE: http://192.168.4.1";
    fixedMsgLine2 = "------------------------------------";
    variableMsg  = "Connect to portal to save sites.";
    
    // ====================================================================
    // DUPLICATED SERIAL ECHO
    // ====================================================================
    Serial.println("\n--- VFD ROW PRINT ECHO ---");
    Serial.println(fixedMsgLine1);
    Serial.println(fixedMsgLine2);
    Serial.println(variableMsg);
    Serial.println("--------------------------\n");

    refreshDisplay();
}

bool startConfigAndConnect() {
    if (isConnectingWifi) return true; 

    File file = LittleFS.open("/networks.json", "r");
    if (!file) return false;
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error) return false;
    JsonArray storedProfiles = doc.as<JsonArray>();
    if (storedProfiles.size() == 0) return false;

    // VFD Progress Tracking
    printAtTextRow(1, "Scanning matching profiles...", 36);
    
    // ====================================================================
    // DUPLICATED SERIAL ECHO
    // ====================================================================
    Serial.println("[VFD ECHO] Row 1: Scanning matching profiles...");

    int foundNetworks = WiFi.scanNetworks();
    bestProfileIndex = -1; 
    int highestRSSI = -100;
    
    for (int i = 0; i < foundNetworks; i++) {
        String currentSSID = WiFi.SSID(i); int currentRSSI = WiFi.RSSI(i);
        for (size_t j = 0; j < storedProfiles.size(); j++) {
            if (currentSSID == storedProfiles[j]["ssid"].as<String>()) {
                if (currentRSSI > highestRSSI) { highestRSSI = currentRSSI; bestProfileIndex = j; }
            }
        }
    }
    WiFi.scanDelete();

    if (bestProfileIndex != -1) {
        JsonObject profile = storedProfiles[bestProfileIndex].as<JsonObject>();
        String targetSSID = profile["ssid"].as<String>();
        String targetPass = profile["password"].as<String>();
        
        // VFD Progress Tracking
        printAtTextRow(0, ">> WI-FI STATION PORTAL", 36);
        printAtTextRow(1, "Linking: " + targetSSID, 36);
        printAtTextRow(2, "Connecting to router...", 36);
        printAtTextRow(3, "Please wait safely...", 36);

        // ====================================================================
        // DUPLICATED SERIAL ECHO
        // ====================================================================
        Serial.println("\n--- VFD ROW PRINT ECHO ---");
        Serial.println("Row 0: >> WI-FI STATION PORTAL");
        Serial.println("Row 1: Linking: " + targetSSID);
        Serial.println("Row 2: Connecting to router...");
        Serial.println("Row 3: Please wait safely...");
        Serial.println("--------------------------\n");

        MDNS.end();
        server.stop();
        
        WiFi.disconnect(true);
        WiFi.mode(WIFI_STA);

        if (targetPass.length() == 0 || targetPass == "") {
            WiFi.begin(targetSSID.c_str());
        } else {
            WiFi.begin(targetSSID.c_str(), targetPass.c_str());
        }

        isConnectingWifi = true;
        wifiStepStart = millis();
        wifiAttemptCount = 0;
        return true;
    }
    return false;
}

void checkWifiConnectionStep() {
    if (!isConnectingWifi) return;
    unsigned long currentMillis = millis();

    if (currentMillis - wifiStepStart >= 500) {
        wifiStepStart = currentMillis;
        wifiAttemptCount++;
        digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));

        uint8_t currentStatus = WiFi.status();
        Serial.print("[HANDSHAKE TICK #");
        Serial.print(wifiAttemptCount);
        Serial.print("/40] Status Code: ");
        Serial.println(currentStatus);

        if (currentStatus == WL_CONNECTED) { // 3 = WL_CONNECTED
            if (WiFi.localIP().toString() == "0.0.0.0" && wifiAttemptCount < 40) return; 

            isConnectingWifi = false;
            isPortalMode = false; 
            digitalWrite(LED_BUILTIN, LOW);

            File file = LittleFS.open("/networks.json", "r");
            if (file) {
                JsonDocument doc;
                if (deserializeJson(doc, file) == DeserializationError::Ok) {
                    JsonArray storedProfiles = doc.as<JsonArray>();
                    if (bestProfileIndex >= 0 && bestProfileIndex < storedProfiles.size()) {
                        JsonObject profile = storedProfiles[bestProfileIndex].as<JsonObject>();
                        strlcpy(appText1, profile["txt1"] | "", sizeof(appText1));
                        strlcpy(appText2, profile["txt2"] | "", sizeof(appText2));
                        strlcpy(appText3, profile["txt3"] | "", sizeof(appText3));
                        strlcpy(appText4, profile["txt4"] | "", sizeof(appText4));
                        appFeatureFlag = profile["feat_flag"] | false;
                        ledMatrixBrightness = profile["vfd_bright"] | 60;
                        bellIntervalMinutes = profile["bell_int"] | 0;
                    }
                }
                file.close();
            }

            syncExternalHardware();
            
            server.stop();             
            setupServerRoutes();       
            server.begin();            
            
            if (MDNS.begin("ledmatrix")) {
                MDNS.addService("http", "tcp", 80);
            }

            setenv("TZ", "GMT0BST,M3.5.0/1,M10.5.0/2", 1); 
            tzset();                                       
            configTime(0, 0, "pool.ntp.org", "time.nist.gov"); 

            fixedMsgLine1 = "IP: " + WiFi.localIP().toString();
            fixedMsgLine2 = "------------------------------------";
            variableMsg  = String(appText1); 

            // ====================================================================
            // DUPLICATED SERIAL ECHO
            // ====================================================================
            Serial.println("\n--- VFD ROW PRINT ECHO ---");
            Serial.println(fixedMsgLine1);
            Serial.println(fixedMsgLine2);
            Serial.println("Row 2/3 (Raw): " + variableMsg);
            Serial.println("--------------------------\n");

            ringBell();
            clearDisplay();
            refreshDisplay();
        } 
        else if (wifiAttemptCount >= 40) {
            isConnectingWifi = false;
            digitalWrite(LED_BUILTIN, LOW);
            
            // VFD Failure Warning
            printAtTextRow(1, "No saved profile in range.", 36);
            
            // ====================================================================
            // DUPLICATED SERIAL ECHO
            // ====================================================================
            Serial.println("[VFD ECHO] Row 1: No saved profile in range.");

            wifiStepStart = millis(); 
            startConfigPortal();
        }
    }
}
