#include <SPI.h>

// Pin Definitions
////Pin connected to OE of 74HC595
const int ENABLE_PIN = 12;
const int rowPins[8] = {2, 3, 4, 5, 6, 7, 8, 9}; // Row driver pins

// Simple 5x8 Font Map (ASCII 32 to 90: Space, Punctuation, Numbers, Uppercase Letters)
// Each column is 1 byte; 5 bytes total per character + 1 blank column for spacing.
const byte font[][5] PROGMEM = 
{
  {0x00, 0x00, 0x00, 0x00, 0x00}, // Space (32)
  {0x00, 0x00, 0x5F, 0x00, 0x00}, // !
  {0x00, 0x07, 0x00, 0x07, 0x00}, // "
  {0x14, 0x7F, 0x14, 0x7F, 0x14}, // #
  {0x24, 0x2A, 0x7F, 0x2A, 0x12}, // $
  {0x23, 0x13, 0x08, 0x64, 0x62}, // %
  {0x36, 0x49, 0x55, 0x22, 0x50}, // &
  {0x00, 0x05, 0x03, 0x00, 0x00}, // '
  {0x00, 0x1C, 0x22, 0x41, 0x00}, // (
  {0x00, 0x41, 0x22, 0x1C, 0x00}, // )
  {0x14, 0x08, 0x3E, 0x08, 0x14}, // *
  {0x08, 0x08, 0x3E, 0x08, 0x08}, // +
  {0x00, 0x50, 0x30, 0x00, 0x00}, // ,
  {0x08, 0x08, 0x08, 0x08, 0x08}, // -
  {0x00, 0x60, 0x60, 0x00, 0x00}, // .
  {0x20, 0x10, 0x08, 0x04, 0x02}, // /
  {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
  {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
  {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
  {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
  {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
  {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
  {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
  {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
  {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
  {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
  {0x00, 0x36, 0x36, 0x00, 0x00}, // :
  {0x00, 0x56, 0x36, 0x00, 0x00}, // ;
  {0x08, 0x14, 0x22, 0x41, 0x00}, // <
  {0x14, 0x14, 0x14, 0x14, 0x14}, // =
  {0x00, 0x41, 0x22, 0x14, 0x08}, // >
  {0x02, 0x01, 0x51, 0x09, 0x06}, // ?
  {0x32, 0x49, 0x79, 0x41, 0x3E}, // @
  {0x7E, 0x11, 0x11, 0x11, 0x7E}, // A
  {0x7F, 0x49, 0x49, 0x49, 0x36}, // B
  {0x3E, 0x41, 0x41, 0x41, 0x22}, // C
  {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D
  {0x7F, 0x49, 0x49, 0x49, 0x41}, // E
  {0x7F, 0x09, 0x09, 0x09, 0x01}, // F
  {0x3E, 0x41, 0x49, 0x49, 0x7A}, // G
  {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H
  {0x00, 0x41, 0x7F, 0x41, 0x00}, // I
  {0x20, 0x40, 0x41, 0x3F, 0x01}, // J
  {0x7F, 0x08, 0x14, 0x22, 0x41}, // K
  {0x7F, 0x40, 0x40, 0x40, 0x40}, // L
  {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // M
  {0x7F, 0x04, 0x08, 0x10, 0x7F}, // N
  {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O
  {0x7F, 0x09, 0x09, 0x09, 0x06}, // P
  {0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q
  {0x7F, 0x09, 0x19, 0x29, 0x46}, // R
  {0x46, 0x49, 0x49, 0x49, 0x31}, // S
  {0x01, 0x01, 0x7F, 0x01, 0x01}, // T
  {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U
  {0x1F, 0x20, 0x40, 0x20, 0x1F}, // V
  {0x3F, 0x40, 0x38, 0x40, 0x3F}, // W
  {0x63, 0x14, 0x08, 0x14, 0x63}, // X
  {0x07, 0x08, 0x70, 0x08, 0x07}, // Y
  {0x61, 0x51, 0x49, 0x45, 0x43}  // Z
};

// Message Settings

#define MSG_ENTRY(str)  str, sizeof(str) - 1 

// The active 60-column canvas (stored as 8 rows x 8 bytes mapping a 64-bit frame)

typedef byte LineBuffer[8][30];

// Define the new structure containing the array and the additional integers

typedef struct 
{
    LineBuffer data;
    const int LATCH_PIN;
    int scrollPos;
     char *text;
    int length;
} Display;

Display mydisplay[4] = 
{
    { {0}, A3, 0, MSG_ENTRY("          THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG.") }, 
    { {0}, A4, 0, MSG_ENTRY("          HELLO WORLD!   ") }, 
    { {0}, A5, 0, MSG_ENTRY("          DJRM") }, 
    { {0}, A2, 0, MSG_ENTRY("  FATAL ERROR ") } 
};

// Tracking scroll animation variables
//int scrollPos = 0;
unsigned long lastScrollTime = 0;
const int scrollSpeed = 50; // Milliseconds per column shift
const int pixelHold = 1000; 

void setup() 
{
  pinMode(mydisplay[0].LATCH_PIN, OUTPUT);
  pinMode(mydisplay[1].LATCH_PIN, OUTPUT);
  pinMode(mydisplay[2].LATCH_PIN, OUTPUT);
  pinMode(mydisplay[3].LATCH_PIN, OUTPUT);

  digitalWrite(ENABLE_PIN, 0); // preset enable 

  for (int i = 0; i < 8; i++) 
  {
    pinMode(rowPins[i], OUTPUT);
    digitalWrite(rowPins[i], LOW);
  }
  
  SPI.begin();
  SPI.beginTransaction(SPISettings(8000000, LSBFIRST, SPI_MODE0));
}

void loop() {

  // 1. Refresh the display layout continuously
  refreshDisplay();

  // 2. Animate the layout buffer independently of the scan speed
  if (millis() - lastScrollTime >= scrollSpeed) 
  {
    lastScrollTime = millis();
    updateBuffer(&mydisplay[0]);
    updateBuffer(&mydisplay[1]);
    updateBuffer(&mydisplay[2]);
    mydisplay[3].scrollPos = 0;
    sprintf(mydisplay[3].text, " %6ul   ", lastScrollTime/100L);
    updateBuffer(&mydisplay[3]);
  }
}

// Re-maps your scrolling string directly into the active 64-bit columns
void updateBuffer(Display *displayBuffer) 
{
  // Clear layout canvas
  memset(displayBuffer->data, 0xFF, sizeof(LineBuffer));

  // Step column by column through your active 60-LED display window
  for (int displayCol = 0; displayCol < 240; displayCol++) 
  {
    int totalColIndex = displayBuffer->scrollPos + displayCol;
    int charIndex = totalColIndex / 6;
    int colInChar = totalColIndex % 6;

    if (charIndex < displayBuffer->length) 
    {
      char c = displayBuffer->text[charIndex];
      if (c >= 32 && c <= 90) 
      { // Keep boundaries within font constraints
        byte columnData = 0;
        if (colInChar < 5) 
        { // Load font tracking data
          columnData = pgm_read_byte(&(font[c - 32][colInChar]));
        } else {
          columnData = 0x00; // 6th column acting as letter spacing
        }

        // Project the bits horizontally across our 8 separate byte rows
        for (int row = 0; row < 8; row++) 
        {
          if (columnData & (1 << row)) 
          {
            int targetByte = displayCol / 8;
            int targetBit = displayCol % 8;
            displayBuffer->data[row][targetByte] &= ~(1 << targetBit);
          }
        }

      }
    }

  }

  displayBuffer->scrollPos++;
  // Reset layout loop when message passes entirely through the 60-column window
  if (displayBuffer->scrollPos >= (displayBuffer->length * 6)) 
  { 
    displayBuffer->scrollPos = 0;
  }
  
}

// Standard Multiplex Scanning Pattern
void refreshDisplay( ) 
{
  for (int thisRow = 7; thisRow >= 0 ; thisRow--) 
  {
    for (int line = 0; line <= 3; line++)
    {
      ///line = 3;
      // Push and latch the 64 bits (8 bytes) out to the cascaded registers
      digitalWrite(mydisplay[line].LATCH_PIN, LOW);
      for (int shiftReg = 0; shiftReg <= 7; shiftReg++) 
      {
        SPI.transfer(mydisplay[line].data[thisRow][shiftReg]);
      }
      digitalWrite(mydisplay[line].LATCH_PIN, HIGH);
    }
    // Light up target row
    digitalWrite(rowPins[thisRow], HIGH);
    delayMicroseconds(pixelHold); // Fine-tuned illumination period
    //updateBuffer(&mydisplay[thisRow % 3]);
    
    // Clear to eliminate text ghosting/blurring artifacts
    digitalWrite(rowPins[thisRow], LOW);
  }
}
