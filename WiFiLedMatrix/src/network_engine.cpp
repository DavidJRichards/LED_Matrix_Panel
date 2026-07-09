#include "network_engine.h"
#include "config.h"
#include "hardware.h"
#include <WiFi.h>
#include <LEAmDNS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <time.h>

void handleRoot() {
    if (server.hasArg("msg")) {
        variableMsg = server.arg("msg");
        ringBell();
        refreshDisplay();
    }

    File file = LittleFS.open("/index.html", "r");
    if (!file) { server.send(404, "text/plain", "FS Error"); return; }

    String htmlStream = file.readString();
    file.close();

    htmlStream.replace("%TXT1%", String(appText1));
    htmlStream.replace("%TXT2%", String(appText2));
    htmlStream.replace("%TXT3%", String(appText3));
    htmlStream.replace("%TXT4%", String(appText4));

    String featureStateStr = appFeatureFlag ? "ACTIVE" : "INACTIVE";
    String featureColorStr = appFeatureFlag ? "#107c41" : "#6c757d";

    htmlStream.replace("%FEAT_STATE%", featureStateStr);
    htmlStream.replace("%FEAT_COLOR%", featureColorStr);

    // FIXED WEB HOOK: Swaps out the layout placeholder with your actual live brightness level
    htmlStream.replace("%BRIGHT_VAL%", String(ledMatrixBrightness));
    // FIXED WEB HOOK: Replaces placeholder tags with your live interval minutes variable
    htmlStream.replace("%INTERVAL_VAL%", String(bellIntervalMinutes));

    server.send(200, "text/html", htmlStream);
}

void handleSave() {
    if (!server.hasArg("ssid") || !server.hasArg("password")) { server.send(400, "text/plain", "Bad Request"); return; }
    String newSSID = server.arg("ssid");
    String newPass = server.arg("password");
    String t1 = server.arg("txt1").substring(0, 64);
    String t2 = server.arg("txt2").substring(0, 64);
    String t3 = server.arg("txt3").substring(0, 64);
    String t4 = server.arg("txt4").substring(0, 64);

    File file = LittleFS.open("/networks.json", "r");
    JsonDocument doc;
    if (file) { deserializeJson(doc, file); file.close(); }
    
    JsonArray arr = doc.as<JsonArray>();
    if (arr.isNull()) { arr = doc.to<JsonArray>(); }

    JsonObject obj = arr.add<JsonObject>();
    obj["ssid"] = newSSID; 
    obj["password"] = newPass;
    obj["txt1"] = t1; 
    obj["txt2"] = t2; 
    obj["txt3"] = t3; 
    obj["txt4"] = t4;

    file = LittleFS.open("/networks.json", "w");
    serializeJson(doc, file); file.close();
    
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
    int targetIndex = server.arg("index").toInt();
    File file = LittleFS.open("/networks.json", "r");
    JsonDocument doc;
    if (file) { deserializeJson(doc, file); file.close(); }
    JsonArray arr = doc.as<JsonArray>();
    if (targetIndex >= 0 && targetIndex < arr.size()) { arr.remove(targetIndex); }
    file = LittleFS.open("/networks.json", "w");
    serializeJson(doc, file); file.close();
    server.sendContent("HTTP/1.1 303 See Other\r\nLocation: /\r\n\r\n");
}

void handleScan() {
    int foundNetworks = WiFi.scanNetworks();
    JsonDocument doc; JsonArray arr = doc.to<JsonArray>();
    int limit = min(foundNetworks, 15);
    for (int i = 0; i < limit; i++) {
        const char* ssidStr = WiFi.SSID(i);
        if (ssidStr != nullptr && strlen(ssidStr) > 0) {
            JsonObject net = arr.add<JsonObject>();
            net["ssid"] = ssidStr; net["rssi"] = WiFi.RSSI(i);
        }
    }
    WiFi.scanDelete();
    String response; serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleManualBeep() {
    ringBell();
    server.send(200, "text/plain", "OK");
}

void handleCurrentSSID() {
    if (WiFi.status() == WL_CONNECTED) {
        server.send(200, "text/plain", WiFi.SSID());
    } else {
        server.send(200, "text/plain", "OFFLINE");
    }
}

    // Capture the selected regular interval period from the incoming web form data
    int webInterval = server.hasArg("bell_interval") ? server.arg("bell_interval").toInt() : 0;

    void handleUpdateText() {
    if (WiFi.status() != WL_CONNECTED) { server.send(400, "text/plain", "Not Connected"); return; }

     // FIXED: Parse the slider value at the very top of the function block scope
    int webBrightness = server.hasArg("vfd_bright") ? server.arg("vfd_bright").toInt() : 60;

    String currentSSID = WiFi.SSID();
    String t1 = server.arg("txt1").substring(0, LED_TEXT_SIZE);
    String t2 = server.arg("txt2").substring(0, LED_TEXT_SIZE);
    String t3 = server.arg("txt3").substring(0, LED_TEXT_SIZE);
    String t4 = server.arg("txt4").substring(0, LED_TEXT_SIZE);

    File file = LittleFS.open("/networks.json", "r");
    JsonDocument doc;
    if (file) { deserializeJson(doc, file); file.close(); }

    JsonArray arr = doc.as<JsonArray>();
    bool matchedAndUpdated = false;

    for (JsonObject profile : arr) {
        if (profile["ssid"].as<String>() == currentSSID) {
            profile["txt1"] = t1; profile["txt2"] = t2;
            profile["txt3"] = t3; profile["txt4"] = t4;
             // Commit to the non-volatile database profile key
            profile["vfd_bright"] = webBrightness;
            // Save settings into the non-volatile JSON profile attributes
            profile["bell_int"] = webInterval;        matchedAndUpdated = true;
            break;
        }
    }

    if (matchedAndUpdated) {
        file = LittleFS.open("/networks.json", "w");
        serializeJson(doc, file); file.close();

        strlcpy(appText1, t1.c_str(), sizeof(appText1));
        strlcpy(appText2, t2.c_str(), sizeof(appText2));
        strlcpy(appText3, t3.c_str(), sizeof(appText3));
        strlcpy(appText4, t4.c_str(), sizeof(appText4));
        
        // Push configuration straight to the active runtime system register
        ledMatrixBrightness = webBrightness;
        // Push the interval parameter right into the active running system register
        bellIntervalMinutes = webInterval;
        lastIntervalBellTime = millis(); // Reset the tracking clock reference window
    
        syncExternalHardware();
        // Check for text-embedded tags if present
        if (t1.indexOf("^g") >= 0 || t2.indexOf("^g") >= 0 || t3.indexOf("^g") >= 0 || t4.indexOf("^g") >= 0) {
            ringBell();
        }

        fixedMsgLine1 = "IP: " + WiFi.localIP().toString();
    }
    server.sendContent("HTTP/1.1 303 See Other\r\nLocation: /\r\n\r\n");
}

// FIXED: RESTORED NON-VOLATILE TOGGLE HANDLER FUNCTION
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
        JsonObject profile = arr[0].as<JsonObject>();
        profile["feat_flag"] = appFeatureFlag;
    }

    file = LittleFS.open("/networks.json", "w");
    serializeJson(doc, file); file.close();

    if (appFeatureFlag) {
        server.send(200, "text/plain", "ACTIVE");
    } else {
        server.send(200, "text/plain", "INACTIVE");
    }
}
// split --------
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

        printAtTextRow(0, ">> MANUAL OVERRIDE PORTAL", 36);
        printAtTextRow(1, "Target: " + targetSSID, 36);
        printAtTextRow(2, "Resetting wireless...", 36);
        printAtTextRow(3, "Please wait safely...", 36);

        String transitionHtml = 
            "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1.0'>"
            "<title>Switching Networks</title>"
            "<style>body{font-family:sans-serif;text-align:center;padding:50px;background:#f7f9fa;color:#333;}</style>"
            "<script>"
            "setTimeout(function(){ window.location.href = 'http://LedMatrix.local'; }, 3500);"
            "</script></head><body>"
            "<h2>\xF0\x9F\x84\x94 Switching Network Profiles...</h2>"
            "<p>Connecting terminal to <strong>" + targetSSID + "</strong>.</p>"
            "<p>Please allow 3 seconds for the radio layers to settle.</p>"
            "</body></html>";

        server.send(200, "text/html", transitionHtml);
        delay(150); 

        MDNS.end();
        server.stop();
        
        WiFi.softAPdisconnect(true);
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF); 
        delay(100);
        
        WiFi.mode(WIFI_STA); 
        delay(100);

        WiFi.begin(targetSSID.c_str(), targetPass.c_str());

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
//    server.on("/beep", HTTP_GET, handleBeep);
    server.on("/current-ssid", HTTP_GET, handleCurrentSSID);
    server.on("/updatetext", HTTP_POST, handleUpdateText);
    server.on("/select-profile", HTTP_GET, handleSelectProfile);
    
    // FIXED: RE-REGISTERED FEATURE TOGGLE ROUTE LINK
    server.on("/toggle-feature", HTTP_GET, handleToggleFeature);
    // NEW ROUTE: Listens for the manual portal bell button click
    server.on("/portal-beep", HTTP_GET, handleManualBeep);
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

    if (MDNS.begin("LedMatrix")) {
        MDNS.addService("http", "tcp", 80);
    }

    fixedMsgLine1 = "SETUP MODE: http://192.168.4.1";
    fixedMsgLine2 = "------------------------------------";
    variableMsg  = "Connect to portal to save sites.";
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

    printAtTextRow(1, "Scanning matching profiles...", 36);
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
        
        printAtTextRow(0, ">> WI-FI STATION PORTAL", 36);
        printAtTextRow(1, "Linking: " + targetSSID, 36);
        printAtTextRow(2, "Connecting to router...", 36);
        printAtTextRow(3, "Please wait safely...", 36);

        MDNS.end();
        server.stop();
        
        WiFi.disconnect(true);
        WiFi.mode(WIFI_STA);
        WiFi.begin(targetSSID.c_str(), targetPass.c_str());

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

        if (WiFi.status() == WL_CONNECTED) {
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
                         // Sync the active brightness level whenever the chip changes network locations
                        ledMatrixBrightness  = profile["vfd_bright"] | 60;     
                        bellIntervalMinutes = profile["bell_int"] | 0;               }
                }
                file.close();
            }

            syncExternalHardware();
            
            server.stop();             
            setupServerRoutes();       
            server.begin();            
            
            if (MDNS.begin("LedMatrix")) {
                MDNS.addService("http", "tcp", 80);
            }

            setenv("TZ", "GMT0BST,M3.5.0/1,M10.5.0/2", 1); 
            tzset();                                       
            configTime(0, 0, "pool.ntp.org", "time.nist.gov"); 

            fixedMsgLine1 = "IP: " + WiFi.localIP().toString();
            fixedMsgLine2 = "------------------------------------";
            variableMsg  = String(appText1); 

            ringBell();
            clearDisplay();
            refreshDisplay();
        } 
        else if (wifiAttemptCount >= 40) {
            isConnectingWifi = false;
            digitalWrite(LED_BUILTIN, LOW);
            printAtTextRow(1, "No saved profile in range.", 36);
            wifiStepStart = millis(); 
            startConfigPortal();
        }
    }
}
