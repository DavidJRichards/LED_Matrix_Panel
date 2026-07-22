#include "network_helpers.h"
#include "config.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <WiFi.h>

void buildPreloadedDashboard(String& htmlOut) {
    File file = LittleFS.open("/index.html", "r");
    if (!file) { htmlOut = "FS Error: File Missing"; return; }
    htmlOut = file.readString();
    file.close();

    // FIXED: Ensured the original raw array configuration blocks flow back to the web portal forms
    htmlOut.replace("%TXT1%", String(appText1));
    htmlOut.replace("%TXT2%", String(appText2));
    htmlOut.replace("%TXT3%", String(appText3));
    htmlOut.replace("%TXT4%", String(appText4));

    String featState = appFeatureFlag ? "ACTIVE" : "INACTIVE";
    String featColor = appFeatureFlag ? "#107c41" : "#6c757d";

    htmlOut.replace("%FEAT_STATE%", featState);
    htmlOut.replace("%FEAT_COLOR%", featColor);
    htmlOut.replace("%BRIGHT_VAL%", String(ledMatrixBrightness));
    htmlOut.replace("%INTERVAL_VAL%", String(bellIntervalMinutes));

    if (WiFi.status() == WL_CONNECTED) {
        htmlOut.replace("%CONNECTED_SSID%", WiFi.SSID());
        htmlOut.replace("%CONNECTED_IP%", WiFi.localIP().toString());
    } else {
        htmlOut.replace("%CONNECTED_SSID%", "OFFLINE");
        htmlOut.replace("%CONNECTED_IP%", "0.0.0.0 (No Link)");
    }

         // Maps the clickable hyperlink straight to your friendly local router domain name
        htmlOut.replace("%HOSTNAME%", dynamicHostname);
}


void commitProfileToFlash(const String& ssid, const String& pass, 
                          const String& t1, const String& t2, 
                          const String& t3, const String& t4, 
                          int bright, int interval) {
    File file = LittleFS.open("/networks.json", "r");
    JsonDocument doc;
    if (file) { deserializeJson(doc, file); file.close(); }
    
    JsonArray arr = doc.as<JsonArray>();
    if (arr.isNull()) { arr = doc.to<JsonArray>(); }

    JsonObject obj = arr.add<JsonObject>();
    obj["ssid"] = ssid; 
    obj["password"] = pass;
    obj["txt1"] = t1; obj["txt2"] = t2;
    obj["txt3"] = t3; obj["txt4"] = t4;
    obj["feat_flag"] = false;
    obj["vfd_bright"] = bright;
    obj["bell_int"]   = interval;

    file = LittleFS.open("/networks.json", "w");
    if (file) { serializeJson(doc, file); file.close(); }
}

String executeAirwaveNetworkScan() {
    int foundNetworks = WiFi.scanNetworks();
    JsonDocument doc; 
    JsonArray arr = doc.to<JsonArray>();
    int limit = min(foundNetworks, 15);
    
    for (int i = 0; i < limit; i++) {
        const char* ssidStr = WiFi.SSID(i);
        if (ssidStr != nullptr && strlen(ssidStr) > 0) {
            JsonObject net = arr.add<JsonObject>();
            net["ssid"] = ssidStr; 
            net["rssi"] = WiFi.RSSI(i);
        }
    }
    WiFi.scanDelete();
    String response; 
    serializeJson(doc, response);
    return response;
}
