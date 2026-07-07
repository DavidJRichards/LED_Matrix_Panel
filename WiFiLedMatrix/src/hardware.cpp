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
    static char row2Buffer[40] = {0};
    static char row3Buffer[40] = {0};

    // FIXED: The VFD now ONLY checks for literal text modifications from the web server.
    // Removed the currentSecond checking completely to leave the glass static and quiet!
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

        // One-time chime check when text arrives
        String t1 = String(appText1); String t2 = String(appText2);
        String t3 = String(appText3); String t4 = String(appText4);
        
        if (t1.indexOf("^g") >= 0 || t1.indexOf("^G") >= 0 || t1.indexOf("\\x07") >= 0 || t1.indexOf((char)0x07) >= 0 ||
            t2.indexOf("^g") >= 0 || t2.indexOf("^G") >= 0 || t2.indexOf("\\x07") >= 0 || t2.indexOf((char)0x07) >= 0 ||
            t3.indexOf("^g") >= 0 || t3.indexOf("^G") >= 0 || t3.indexOf("\\x07") >= 0 || t3.indexOf((char)0x07) >= 0 ||
            t4.indexOf("^g") >= 0 || t4.indexOf("^G") >= 0 || t4.indexOf("\\x07") >= 0 || t4.indexOf((char)0x07) >= 0) {
            ringBell(); 
        }
    }

    // THE INTERLEAVED ENGINE STEPPER: Ticks along quietly behind the scenes
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
            default:
                sequenceActive = false;
                writeStep = 0;
                break;
        }
    }
}

void ringBell() {
    tone(BELL_PIN, 2500, 200);
}

void initMyHardware() {
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
    // 1. Capture the raw timestamp integers from the core clock engine
    time_t now = time(nullptr);
    struct tm* timeInfo = localtime(&now);
    char clockBuffer[40] = {0};

    // If the NTP core has handshaked with atomic servers successfully
    if (timeInfo->tm_year > 70) {
        snprintf(clockBuffer, sizeof(clockBuffer), "%02d:%02d:%02d", 
                 timeInfo->tm_hour, timeInfo->tm_min, timeInfo->tm_sec);
    } else {
        // Safe fallback indicator while the network handshakes
        snprintf(clockBuffer, sizeof(clockBuffer), "Syncing...");
    }

    // 2. FIXED: Create TEMPORARY local string configurations 
    // This preserves the original raw '$TIME$' tokens inside the global appText arrays!
    String s1 = String(appText1);
    String s2 = String(appText2);
    String s3 = String(appText3);
    String s4 = String(appText4);

    // 3. Scan and replace the macro token strings on local frames
    s1.replace("$TIME$", String(clockBuffer));
    s2.replace("$TIME$", String(clockBuffer));
    s3.replace("$TIME$", String(clockBuffer));
    s4.replace("$TIME$", String(clockBuffer));

    // 4. Dispatch the dynamically expanded 64-character text blocks to channels 0-3
    print_line(0, s1.c_str());
    print_line(1, s2.c_str());
    print_line(2, s3.c_str());
    print_line(3, s4.c_str());
}

struct LutPoint {
    int adcVal;         // Range: 0 to 4095
    int brightnessVal;  // Range: 0 to 255
};

// 2. Updated Lookup Table (LUT) for Pull-Down Configuration
// Note: Low ADC values = High light intensity = High brightness
const LutPoint LOOKUP_TABLE[] = {
    {282,   100},  // Direct intense light: Max brightness (100%)
    {1000,  100},   // Bright indoor / daylight (~70%)
    {1969,  35},   // Typical room lighting: Baseline (~35%)
    {3000,  15},   // Very dim room (~15%)
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

            Serial.print("Raw ADC: ");
            Serial.print(rawAdc);
            Serial.print(" -> Matched 8-bit Brightness: ");
            Serial.println(targetBrightness);

        }  
        return targetBrightness;
    }
