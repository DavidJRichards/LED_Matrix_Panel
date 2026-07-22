#include "hardware.h"
#include "config.h"
#include <WiFi.h>

void checkVfdReady() {
    uint32_t watchdogTime = micros();
    while (digitalRead(VFD_BUSY_PIN) == LOW) { 
        if (micros() - watchdogTime > 5000) {
            break; 
        }
        delayMicroseconds(2);
    }
}

void vfdWriteByte(uint8_t data) {
    checkVfdReady();
    VfdSerial.write(data);
}

void clearDisplay() {
    vfdWriteByte(0x0C); // Form-Feed Full Clear
    delay(15);          
}

void moveCursor(uint16_t x, uint16_t y) {
    vfdWriteByte(0x1F);
    vfdWriteByte(0x24);
    vfdWriteByte(x & 0xFF);         
    vfdWriteByte((x >> 8) & 0xFF);  
    vfdWriteByte(y & 0xFF);         
    vfdWriteByte((y >> 8) & 0xFF);  
    delayMicroseconds(100);
}

void printAtTextRow(uint8_t row, String text, uint8_t max_len) {
    uint16_t pixelY = ((row + 1) * 16) - 1; 
    moveCursor(0, pixelY);

    // ====================================================================
    // SIMPLIFIED: No $TIME$ macro expansions are performed for the VFD screen!
    // The raw text block string flows straight to the tag stripper filter.
    // ====================================================================
    char cleanLineBuffer[65] = {0};
    uint8_t cleanIdx = 0;
    int textLength = text.length();

    for (int i = 0; i < textLength && cleanIdx < max_len; i++) {
        if (text[i] == '^' && (i + 1 < textLength) && (text[i + 1] == 'g' || text[i + 1] == 'G')) {
            i++; 
        }
        else if (text[i] == '\\' && (i + 3 < textLength) && text[i + 1] == 'x' && text[i + 2] == '0' && text[i + 3] == '7') {
            i += 3; 
        }
        else if (text[i] == 0x07) {
            // Drop byte
        } 
        else {
            cleanLineBuffer[cleanIdx] = text[i];
            cleanIdx++;
        }
    }

    char finalOutputBlock[65] = {0};
    snprintf(finalOutputBlock, sizeof(finalOutputBlock), "%-*.*s", max_len, max_len, cleanLineBuffer);

    // Stream text bytes straight across the hardware wire
    for (uint8_t k = 0; k < max_len; k++) {
        vfdWriteByte(finalOutputBlock[k]);
    }
}

void refreshDisplay() {
    static String lastFixedMsg1 = "";
    static String lastText1 = "";
    static String lastText2 = "";
    static String lastText3 = "";
    static String lastText4 = "";

    static bool sequenceActive = false;
    static uint8_t writeStep = 0;
    
    // Sized safely to handle text layouts securely without local scope overlaps
    static char row2Buffer[40] = {0};
    static char row3Buffer[40] = {0};

    // VFD Change Guard: Only awaken the compositor engine if text actually changes
    if (fixedMsgLine1 != lastFixedMsg1 || String(appText1) != lastText1 || 
        String(appText2) != lastText2 || String(appText3) != lastText3 || 
        String(appText4) != lastText4) {
        
        lastFixedMsg1 = fixedMsgLine1;
        lastText1 = String(appText1);
        lastText2 = String(appText2);
        lastText3 = String(appText3);
        lastText4 = String(appText4);
        
        sequenceActive = true;
        writeStep = 0;
    }

    // THE 5-STEP INTERLEAVED COMPOSITOR ENGINE (Runs quietly only on actual text saves)
    if (sequenceActive) {
        switch (writeStep) {
            case 0: clearDisplay(); writeStep++; break;
            case 1: printAtTextRow(0, fixedMsgLine1, 36); writeStep++; break;
            case 2: printAtTextRow(1, "------------------------------------", 36); writeStep++; break;
            case 3:
                snprintf(row2Buffer, sizeof(row2Buffer), "%-18.18s%-18.18s", appText1, appText2);
                printAtTextRow(2, String(row2Buffer), 36);
                writeStep++;
                break;
            case 4:
                snprintf(row3Buffer, sizeof(row3Buffer), "%-18.18s%-18.18s", appText3, appText4);
                printAtTextRow(3, String(row3Buffer), 36);
                sequenceActive = false;
                writeStep = 0;
                break;
            default: sequenceActive = false; writeStep = 0; break;
        }
    }
}

void ringBell() {
    // FIXED: Instead of a single blocking chime, we arm the 3-pulse burst register
    bellPulseCounter = 3; 
    lastBellPulseTime = 0; // Forces the first ring to execute immediately
}

void initMyHardware() {

    analogReadResolution(ADC_RESOLUTION);
    pinMode(LDR_PIN, INPUT);   

    pinMode(BELL_PIN, OUTPUT);
    digitalWrite(BELL_PIN, LOW);

    pinMode(VFD_RESET_PIN, OUTPUT);
    pinMode(VFD_BUSY_PIN, INPUT);

    digitalWrite(VFD_RESET_PIN, LOW);
    delay(50);

    VfdSerial.setInvertTX(true); 
    VfdSerial.begin(38400); 

    digitalWrite(VFD_RESET_PIN, HIGH);
    delay(600); 

    vfdWriteByte(0x1F);
    vfdWriteByte(0x58);
    vfdWriteByte(0x00); 

    vfdWriteByte(0x1F);
    vfdWriteByte(0x28);
    vfdWriteByte(0x67);
    vfdWriteByte(0x40); 
    vfdWriteByte(0x01); 
    vfdWriteByte(0x02); 

    clearDisplay();
}

void syncExternalHardware() {
    unsigned long currentMillis = millis();

    // 1. Asynchronous non-blocking multi-chime oscillator loop tracking ticks
    if (bellPulseCounter > 0) {
        if (currentMillis - lastBellPulseTime >= 150) {
            lastBellPulseTime = currentMillis;
            tone(BELL_PIN, 2500, 70); 
            bellPulseCounter--; 
        }
    }

    // ====================================================================
    // SHARED UNIFIED SCOPE DECLARATIONS
    // Declared once at the top so variables can be safely read anywhere inside the function
    // ====================================================================
    time_t now = time(nullptr);
    struct tm* timeInfo = localtime(&now);
    char clockBuffer[40] = {0};

    if (timeInfo->tm_year > 70) {
        snprintf(clockBuffer, sizeof(clockBuffer), "%02d:%02d:%02d", 
                 timeInfo->tm_hour, timeInfo->tm_min, timeInfo->tm_sec);
    } else {
        snprintf(clockBuffer, sizeof(clockBuffer), "Syncing...");
    }

    String ipAddressStr = "0.0.0.0";
    if (isPortalMode) {
        ipAddressStr = "192.168.4.1";
    } else if (WiFi.status() == 3) { // 3 = WL_CONNECTED
        ipAddressStr = WiFi.localIP().toString();
    }

    static unsigned long lastHardwareScrollTime = 0;
    static uint8_t scrollOffset = 0;

    if (currentMillis - lastHardwareScrollTime >= 300) {
        lastHardwareScrollTime = currentMillis;
        scrollOffset++;
    }

    // ====================================================================
    // TRANSITION OPERATING MODES (Runs during boot or configuration)
    // ====================================================================
    if (isBooting || isConnectingWifi || isPortalMode) {
        if (isPortalMode) {
            // FIXED STREAMLINED AP MODE LAYOUT: 
            // Fits cleanly inside your 30/40 character external matrix boundaries!
            print_line(0, ">  AP SETUP PORTAL ACTIVE    <");
            print_line(1, ("Connect to: " + dynamicApSSID).c_str());     // Displays "Connect to: LedMatrix_Portal"
            print_line(2,  "Open URL: http://192.168.4.1");
            print_line(3, ("Hostname: " + dynamicHostname + ".local").c_str()); // Displays "Host Name: ledmatrix-xxxx.local"
        } else {
            // Handshake Mode Status Layout
            print_line(0, "> CONNECTING TO ACCESS POINT <");
            print_line(1, ("Host Id: " + dynamicHostname).c_str());
            print_line(2, ("WiFi AP: " + currentTargetSSID).c_str());
            print_line(3, "Dynamic IP Lease Pending...      ");
        }
        return; // Halt text scrollers while network interfaces are shifting
    }

    // ====================================================================
    // STANDARD PRODUCTION MODE (Executes only when fully online)
    // ====================================================================
    String rawArray[4] = { String(appText1), String(appText2), String(appText3), String(appText4) };
    String finalLines[4];

    for (int i = 0; i < 4; i++) {
        // Apply unified $ token macro expansions
        rawArray[i].replace("$TIME$", String(clockBuffer));
        rawArray[i].replace("$HOSTNAME$", dynamicHostname);
        rawArray[i].replace("$IP_ADDR$", ipAddressStr);

        // Strip acoustic codes out so they don't corrupt matrix panels
        String processedString = "";
        int rawLength = rawArray[i].length();
        for (int k = 0; k < rawLength; k++) {
            if (rawArray[i][k] == '^' && (k + 1 < rawLength) && (rawArray[i][k + 1] == 'g' || rawArray[i][k + 1] == 'G')) {
                k++; 
            }
            else if (rawArray[i][k] == '\\' && (k + 3 < rawLength) && rawArray[i][k + 1] == 'x' && rawArray[i][k + 2] == '0' && rawArray[i][k + 3] == '7') {
                k += 3; 
            }
            else if (rawArray[i][k] == 0x07) {
                // Drop byte
            }
            else {
                processedString += rawArray[i][k];
            }
        }

        // Apply 40-column dynamic marquee scroller slicing
        if (processedString.length() > 30) {
            String paddedText = processedString + "        "; 
            int currentOffset = scrollOffset % paddedText.length();
            String rolledText = paddedText.substring(currentOffset) + paddedText.substring(0, currentOffset);
            finalLines[i] = rolledText.substring(0, 30); 
        } else {
            finalLines[i] = processedString;
        }
    }

    if(!isUpdating)
    {
        print_line(0, finalLines[0].c_str());
        print_line(1, finalLines[1].c_str());
        print_line(2, finalLines[2].c_str());
        print_line(3, finalLines[3].c_str());
    }
}


struct LutPoint {
    int adcVal;         // Range: 0 to 4095
    int brightnessVal;  // Range: 0 to 255
};

// 2. Updated Lookup Table (LUT) for Pull-Down Configuration
// Note: Low ADC values = High light intensity = High brightness
const LutPoint LOOKUP_TABLE[] = {
    {282,   100},  // Direct intense light: Max brightness (100%)
    {1000,  90},   // Bright indoor / daylight (~70%)
    {1969,  80},   // Typical room lighting: Baseline (~35%)
    {3000,  70},   // Very dim room (~15%)
    {3833,  5}     // Total darkness: Minimum safety brightness (~5%)
};

const int LUT_SIZE = sizeof(LOOKUP_TABLE) / sizeof(LOOKUP_TABLE[0]);
// Initialize with your typical room baseline (1969), scaled up by the shift factor
int32_t filteredAdcScaled = 1969 << FILTER_SHIFT;

// 3. Robust Interpolation for Inverse/Descending Tables
int getInterpolatedBrightness(int currentAdc) {
    // Clamp to absolute bounds (Handling the inverted logic safely)
    if (currentAdc <= LOOKUP_TABLE[0].adcVal) {
        return LOOKUP_TABLE[0].brightnessVal;
    }
    if (currentAdc >= LOOKUP_TABLE[LUT_SIZE - 1].adcVal) {
        return LOOKUP_TABLE[LUT_SIZE - 1].brightnessVal;
    }

    // Find the bounding interval
    for (int i = 0; i < LUT_SIZE - 1; i++) {
        int x0 = LOOKUP_TABLE[i].adcVal;
        int y0 = LOOKUP_TABLE[i].brightnessVal;
        int x1 = LOOKUP_TABLE[i + 1].adcVal;
        int y1 = LOOKUP_TABLE[i + 1].brightnessVal;

        // Check if the current reading falls between these points
        if (currentAdc >= x0 && currentAdc <= x1) {
            // Integer Interpolation Formula: y = y0 + ((x - x0) * (y1 - y0)) / (x1 - x0)
            return y0 + ((currentAdc - x0) * (y1 - y0)) / (x1 - x0);
        }
    }

    return LOOKUP_TABLE[LUT_SIZE - 1].brightnessVal;
}

 int read_ldr_brightness(unsigned long currentTime){
        static int targetBrightness = 35;
        static unsigned long lastCheckTime = 0;

        if (currentTime - lastCheckTime >= FILTER_INTERVAL_MS) {
            lastCheckTime = currentTime;

            int rawAdc = analogRead(LDR_PIN);

        // Formula: Scaled = Scaled + Weight * (Raw - (Scaled >> Shift))
        filteredAdcScaled = filteredAdcScaled + FILTER_WEIGHT * (rawAdc - (filteredAdcScaled >> FILTER_SHIFT));

        // 3. Shift back down to get the clean 12-bit filtered result
        int smoothedAdc = filteredAdcScaled >> FILTER_SHIFT;

        // 4. Derive 0-100% brightness scale using the clean integer
        targetBrightness = getInterpolatedBrightness(smoothedAdc);
#if 0
            Serial.print("Raw ADC: ");
            Serial.print(rawAdc);
            Serial.print(" -> Matched 8-bit Brightness: ");
            Serial.println(targetBrightness);
#endif
        }  
        return targetBrightness;
    }
