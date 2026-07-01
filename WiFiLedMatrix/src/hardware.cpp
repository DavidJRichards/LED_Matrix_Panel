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

    char finalOutputBlock[40] = {0};
    snprintf(finalOutputBlock, sizeof(finalOutputBlock), "%-*.*s", max_len, max_len, cleanLineBuffer);

    for (uint8_t k = 0; k < max_len; k++) {
        vfdWriteByte(finalOutputBlock[k]);
    }
}

void refreshDisplay() {
    // Persistent tracking buffers to monitor user web text updates
    static String lastFixedMsg1 = "";
    static String lastText1 = "";
    static String lastText2 = "";
    static String lastText3 = "";
    static String lastText4 = "";

    // Sequential step machine variables
    static bool sequenceActive = false;
    static uint8_t writeStep = 0;

    // FIXED: Moved array buffer definitions out of the local switch scope to guarantee zero stack overflow corruption
    static char row2Buffer[40] = {0};
    static char row3Buffer[40] = {0};

    // Repaint only if the URL or Custom Web Text strings change
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

    // THE INTERLEAVED ENGINE STEPPER
    if (sequenceActive) {
        switch (writeStep) {
            case 0:
                clearDisplay();
                writeStep++;
                break;
            case 1:
                printAtTextRow(0, fixedMsgLine1, 36); 
                writeStep++;
                break;
            case 2:
                printAtTextRow(1, "------------------------------------", 36); 
                writeStep++;
                break;
            case 3:
                // Merge App Text 1 and App Text 2 side-by-side onto Row 2
                snprintf(row2Buffer, sizeof(row2Buffer), "%-18.18s%-18.18s", appText1, appText2);
                printAtTextRow(2, String(row2Buffer), 36);
                writeStep++;
                break;
            case 4:
                // Merge App Text 3 and App Text 4 side-by-side onto Row 3
                snprintf(row3Buffer, sizeof(row3Buffer), "%-18.18s%-18.18s", appText3, appText4);
                printAtTextRow(3, String(row3Buffer), 36);
                
                // FIXED COMPACT CONTROL: Turn off the engine state completely to clear up data buses
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
    print_line(0, appText1);
    print_line(1, appText2);
    print_line(2, appText3);
    print_line(3, appText4);
}
