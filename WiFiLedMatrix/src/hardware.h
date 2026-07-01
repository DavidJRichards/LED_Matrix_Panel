#pragma once
#include <Arduino.h>

void checkVfdReady();
void vfdWriteByte(uint8_t data);
void clearDisplay();
void moveCursor(uint16_t x, uint16_t y);
void printAtTextRow(uint8_t row, String text, uint8_t max_len);
void autoWrapText(String text);
void refreshDisplay();
void ringBell();
void initMyHardware();
extern void print_line(int channel, const char* text);
void syncExternalHardware();
extern void led_setup(void);
extern void scan_and_render_display(void); // Continuous display refresh matrix sweep
extern void led_display_cont(bool enable);