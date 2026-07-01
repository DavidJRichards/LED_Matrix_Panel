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
    server.streamFile(file, "text/html");
    file.close();
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

    JsonObject obj = doc.add<JsonObject>();
    obj["ssid"] = newSSID; obj["password"] = newPass;
    obj["txt1"] = t1; obj["txt2"] = t2; obj["txt3"] = t3; obj["txt4"] = t4;

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

void handleBeep() {
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

void handleUpdateText() {
    if (WiFi.status() != WL_CONNECTED) {
        server.send(400, "text/plain", "Not Connected to a network");
        return;
    }

    String currentSSID = WiFi.SSID();
    String t1 = server.arg("txt1").substring(0, 64);
    String t2 = server.arg("txt2").substring(0, 64);
    String t3 = server.arg("txt3").substring(0, 64);
    String t4 = server.arg("txt4").substring(0, 64);

    File file = LittleFS.open("/networks.json", "r");
    JsonDocument doc;
    if (file) { deserializeJson(doc, file); file.close(); }

    JsonArray arr = doc.as<JsonArray>();
    bool matchedAndUpdated = false;

    for (JsonObject profile : arr) {
        if (profile["ssid"].as<String>() == currentSSID) {
            profile["txt1"] = t1;
            profile["txt2"] = t2;
            profile["txt3"] = t3;
            profile["txt4"] = t4;
            matchedAndUpdated = true;
            break;
        }
    }

    if (matchedAndUpdated) {
        file = LittleFS.open("/networks.json", "w");
        serializeJson(doc, file); file.close();

        // 1. Force the dynamic string allocations into global state arrays
        strlcpy(appText1, t1.c_str(), sizeof(appText1));
        strlcpy(appText2, t2.c_str(), sizeof(appText2));
        strlcpy(appText3, t3.c_str(), sizeof(appText3));
        strlcpy(appText4, t4.c_str(), sizeof(appText4));
        
        // 2. CRITICAL HOOK: Update your custom channels instantly when modified over Wi-Fi
        syncExternalHardware();
        ringBell();
    }

    server.sendContent("HTTP/1.1 303 See Other\r\nLocation: /\r\n\r\n");
}

void setupServerRoutes() {
    server.on("/", HTTP_GET, handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.on("/list", HTTP_GET, handleList);
    server.on("/delete", HTTP_GET, handleDelete);
    server.on("/scan", HTTP_GET, handleScan);
    server.on("/beep", HTTP_GET, handleBeep);
    server.on("/current-ssid", HTTP_GET, handleCurrentSSID);
    server.on("/updatetext", HTTP_POST, handleUpdateText);
}

void startConfigPortal() {
    isPortalMode = true;
    WiFi.mode(WIFI_AP);
    WiFi.softAP(CONFIG_AP_SSID);
    setupServerRoutes();
    server.begin();

    if (MDNS.begin("pico4x20led")) {
        MDNS.addService("http", "tcp", 80);
    }

    fixedMsgLine1 = "URL: http://pico4x20led.local";
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

        WiFi.disconnect(true);
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
            if (WiFi.localIP().toString() == "0.0.0.0" && wifiAttemptCount < 40) {
                return; 
            }

            isConnectingWifi = false;
            digitalWrite(LED_BUILTIN, LOW);

            // Re-open local storage to securely parse the matched roaming network text arrays
            File file = LittleFS.open("/networks.json", "r");
            if (file) {
                JsonDocument doc;
                if (deserializeJson(doc, file) == DeserializationError::Ok) {
                    JsonArray storedProfiles = doc.as<JsonArray>();
                    JsonObject profile = storedProfiles[bestProfileIndex].as<JsonObject>();

                    // Securely assign memory strings inside local RAM bounds
                    strlcpy(appText1, profile["txt1"] | "", sizeof(appText1));
                    strlcpy(appText2, profile["txt2"] | "", sizeof(appText2));
                    strlcpy(appText3, profile["txt3"] | "", sizeof(appText3));
                    strlcpy(appText4, profile["txt4"] | "", sizeof(appText4));
                }
                file.close();
            }

            // CRITICAL HOOK: Called immediately after text elements are successfully recovered from flash!
            syncExternalHardware();

            if (MDNS.begin("pico4x20led")) {
                MDNS.addService("http", "tcp", 80);
            }

            setenv("TZ", "GMT0BST,M3.5.0/1,M10.5.0/2", 1); 
            tzset();                                       
            configTime(0, 0, "pool.ntp.org", "time.nist.gov"); 

            fixedMsgLine1 = "URL: http://pico4x20led.local";
            fixedMsgLine2 = "------------------------------------";
            variableMsg  = String(appText1); 

            ringBell();
            setupServerRoutes();
            server.begin();

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
