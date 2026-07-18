#include <Arduino.h>
#include "matrix_pulse_manager.h"

#define TRIGGER_PIN 14  // Scope Channel 1 -> Fixed 1600ns Positive Trigger Pulse
#define ENABLE_PIN  15  // Scope Channel 2 -> Active LOW Pulse, then HIGH Remainder
#define POT_PIN     26  

MatrixPulseManager pulseManager;

volatile uint32_t debug_pulse_counter = 0;
volatile uint32_t debug_current_low_ns = 0;

void setup() {
    Serial.begin(115200);
    delay(1000); 
    
    Serial.println("\n=== 1-to-1 Loop Mapping 1600ns PWM Engine Booted ===");
    analogReadResolution(12);
    
    if (pulseManager.begin(pio0, TRIGGER_PIN, ENABLE_PIN)) {
        Serial.println("[SUCCESS] System locked onto factory default 125MHz clock profile.");
    } else {
        Serial.println("[ERROR] Core hardware allocation failure!");
    }
}

void loop() {
    uint32_t raw_adc = analogRead(POT_PIN);
    
    // Map 12-bit ADC cleanly to standard 0-100 integers
    uint32_t mapped_percent = (raw_adc * 100 + 2048) / 4096;
    if (mapped_percent > 100) mapped_percent = 100;
    
    uint8_t current_brightness_percent = (uint8_t)mapped_percent;

    // Send the integer directly to the manager class API
    pulseManager.set_width_percent(current_brightness_percent);
    
    // Tracking calculation: Each 1% step corresponds to a 16ns hardware loop pass
    debug_current_low_ns = current_brightness_percent * 16; 

    static uint32_t logger_tick = 0;
    if (millis() - logger_tick >= 1000) {
        logger_tick = millis();
        Serial.printf("[OBJECT LOG] ADC: %4u | Input: %3u%% | Active LOW: %4u ns | Pulses: %u\n", 
                      raw_adc, current_brightness_percent, debug_current_low_ns, debug_pulse_counter);
    }
    
    delay(20); 
}

void setup1() {
    while (!pulseManager.is_hardware_ready()) {
        __compiler_memory_barrier();
    }
}

void loop1() {
    if (true) {
        pulseManager.fire_blocking_pulse(); 
        delayMicroseconds(50); 
        debug_pulse_counter++;
    }
}
