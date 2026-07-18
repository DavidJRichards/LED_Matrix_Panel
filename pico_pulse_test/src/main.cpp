#include <Arduino.h>
#include "hardware/pio.h"
#include "hardware/clocks.h"

#include "pulse_generator.pio.h"

#define TRIGGER_PIN 14  // Scope Channel 1 -> Fixed 200ns Positive Pulse
#define ENABLE_PIN  15  // Scope Channel 2 -> Variable Positive-Going (Active-HIGH) Pulse
#define POT_PIN     26  

// Global hardware allocation trackers
PIO pio_hw = pio0;
uint pio_sm = 0;
uint pio_offset = 0;

volatile bool pio_is_ready = false; 
volatile uint32_t dynamic_bitmask_packet = 0x00000000; 
volatile uint32_t debug_pulse_counter = 0;
volatile uint32_t debug_current_low_ns = 0;

// =========================================================================
// FORWARD DECLARATIONS
// =========================================================================
static inline void local_pulse_generator_init(PIO pio, uint sm, uint offset, uint trigger_pin, uint enable_pin);
inline void __not_in_flash_func(trigger_pulse)(uint32_t bitmask);

// =========================================================================
// CORE 0: Wi-Fi Operations, Serial Telemetry, and Bit Encoding
// =========================================================================
void setup() {
    Serial.begin(115200);
    delay(1000); 
    
    Serial.println("\n=== Calibrated 200ns Total PWM Pipeline Ready ===");
    analogReadResolution(12); 
    
    if (pio_can_add_program(pio_hw, &pulse_generator_program)) {
        pio_offset = pio_add_program(pio_hw, &pulse_generator_program);
        pio_sm = pio_claim_unused_sm(pio_hw, true);
        
        local_pulse_generator_init(pio_hw, pio_sm, pio_offset, TRIGGER_PIN, ENABLE_PIN);
        
        Serial.printf("[SYSTEM READY] Processing on SM: %u\n", pio_sm);
        pio_is_ready = true; 
    } else {
        Serial.println("[ERROR] Memory allocation failure!");
    }
}

void loop() {
    uint32_t raw_adc = analogRead(POT_PIN);
    
    // Scale across the calibrated 27 bitstream selections
    uint32_t low_bits = (raw_adc * 27) / 4096; 
    if (low_bits > 27) low_bits = 27;

    uint32_t temporary_mask = 0xFFFFFFFF; 
    for (uint32_t i = 0; i < low_bits; i++) {
        temporary_mask &= ~(1u << i); 
    }
    
    // CHANGE 1: Removed the tilde '~'. This reverts the polarity to standard active-HIGH.
    dynamic_bitmask_packet = temporary_mask;
    
    debug_current_low_ns = low_bits * 8; 

    static uint32_t logger_tick = 0;
    if (millis() - logger_tick >= 1000) {
        logger_tick = millis();
        Serial.printf("[SCOPE DATA] Pot ADC: %4u | Expected Width: %3u ns | Pulses: %u\n", 
                      raw_adc, debug_current_low_ns, debug_pulse_counter);
    }
    
    delay(10); 
}

// =========================================================================
// CORE 1: High-Speed Isolated Trigger Pipeline (Wi-Fi Immune)
// =========================================================================
void setup1() {
    while (!pio_is_ready) {
        __compiler_memory_barrier();
    }
}

void loop1() {
    bool trigger_condition_met = true; 

    if (trigger_condition_met) {
        uint32_t current_mask = dynamic_bitmask_packet;
        trigger_pulse(current_mask); 
        delayMicroseconds(50); 
    }
}

// =========================================================================
// FUNCTION IMPLEMENTATIONS
// =========================================================================
static inline void local_pulse_generator_init(PIO pio, uint sm, uint offset, uint trigger_pin, uint enable_pin) {
    pio_gpio_init(pio, trigger_pin);
    pio_gpio_init(pio, enable_pin);
    
    pio_sm_set_consecutive_pindirs(pio, sm, trigger_pin, 1, true);
    pio_sm_set_consecutive_pindirs(pio, sm, enable_pin, 1, true);

    pio_sm_config c = pulse_generator_program_get_default_config(offset);
    
    sm_config_set_out_pins(&c, enable_pin, 1);
    sm_config_set_out_shift(&c, true, false, 32); 
    sm_config_set_sideset_pins(&c, trigger_pin);
    
    sm_config_set_clkdiv(&c, 1.0f); 
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

    pio_sm_init(pio, sm, offset, &c);
    
    // CHANGE 2: Updated resting pin mask. 
    // Both Pin 14 (Trigger) and Pin 15 (Enable) now correctly start resting at 0V (LOW).
    pio_sm_set_pins_with_mask(pio, sm, (0u << enable_pin), (1u << enable_pin) | (1u << trigger_pin));
    pio_sm_set_enabled(pio, sm, true);
}

inline void __not_in_flash_func(trigger_pulse)(uint32_t bitmask) {
    pio_hw->irq = 1u << 0; 
    pio_sm_put_blocking(pio_hw, pio_sm, bitmask);
    while (!(pio_hw->irq & (1u << 0))) {
        __compiler_memory_barrier(); 
    }
    pio_hw->irq = 1u << 0; 
    debug_pulse_counter++;
}
