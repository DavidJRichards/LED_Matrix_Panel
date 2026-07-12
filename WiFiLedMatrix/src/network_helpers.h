#ifndef NETWORK_HELPERS_H
#define NETWORK_HELPERS_H

#include <Arduino.h>

// Streamlines raw HTML token parsing operations
void buildPreloadedDashboard(String& htmlOut);

// Dispatches unified JSON storage blocks to LittleFS flash partitions
void commitProfileToFlash(const String& ssid, const String& pass, 
                          const String& t1, const String& t2, 
                          const String& t3, const String& t4, 
                          int bright, int interval);

// Executes native background wireless environment airwave scans
String executeAirwaveNetworkScan();

#endif
