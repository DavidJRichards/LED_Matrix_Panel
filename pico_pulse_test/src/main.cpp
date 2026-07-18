#include <Arduino.h>
#include "hardware/pio.h"
#include "hardware/clocks.h"

#include "pulse_generator.pio.h"

#define TRIGGER_PIN 14  // Scope Channel 1 -> Fixed 200ns Positive Trigger Pulse
#define ENABLE_PIN  15  // Scope Channel 2 -> Active LOW Pulse, then HIGH Remainder

// Global hardware allocation trackers
PIO pio_hw = pio0;
uint pio_sm = 0;
uint pio_offset = 0;

volatile bool pio_is_ready = false; 
volatile uint32_t dynamic_bitmask_packet = 0xFFFFFFFF; // Rests HIGH by default
volatile uint32_t debug_pulse_counter = 0;
volatile uint32_t debug_current_low_ns = 0;

// FORWARD DECLARATIONS
static inline void local_pulse_generator_init(PIO pio, uint sm, uint offset, uint trigger_pin, uint enable_pin);
inline void __not_in_flash_func(trigger_pulse)(uint32_t bitmask);
void set_pulse_width_fixed_point(uint16_t value);

void setup() {
    Serial.begin(115200);
    delay(1000); 
    
    Serial.println("\n=== 200ns Total Fixed-Point PWM Engine Booted ===");
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
    // =========================================================================
    // TEST HARNESS: Simulates external program segment modifying value (0-1000)
    // =========================================================================
    static uint16_t mock_external_input = 0;
    static bool counting_up = true;

    set_pulse_width_fixed_point(mock_external_input);

    if (counting_up) {
        mock_external_input += 10; // 1% steps
        if (mock_external_input >= 1000) {
            mock_external_input = 1000;
            counting_up = false;
        }
    } else {
        mock_external_input -= 10; 
        if (mock_external_input <= 0) {
            mock_external_input = 0;
            counting_up = true;
        }
    }

    static uint32_t logger_tick = 0;
    if (millis() - logger_tick >= 1000) {
        logger_tick = millis();
        Serial.printf("[API STATUS] Input: %3u.%1u%% | Active LOW Width: %3u ns | Pulses: %u\n", 
                      mock_external_input / 10, mock_external_input % 10, debug_current_low_ns, debug_pulse_counter);
    }
    
    delay(100); 
}

// =========================================================================
// PUBLIC FIXED-POINT API: Range 0 to 1000 (0.0% to 100.0%)
// =========================================================================
void set_pulse_width_fixed_point(uint16_t value) {
    if (value > 1000) value = 1000;

    // Map 0-1000 smoothly onto 26 available streaming slots
    uint32_t low_bits = ((uint32_t)value * 26 + 500) / 1000;
    if (low_bits > 26) low_bits = 26;

    // Construct the streaming bit mask
    uint32_t temporary_mask = 0xFFFFFFFF; // Start all bits HIGH
    for (uint32_t i = 0; i < low_bits; i++) {
        temporary_mask &= ~(1u << i);     // Pull targeted active bits LOW
    }
    
    dynamic_bitmask_packet = temporary_mask;
    debug_current_low_ns = low_bits * 8; 
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
// SUB-SYSTEM REGISTER ROUTING
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
    
    pio_sm_set_pins_with_mask(pio, sm, (1u << enable_pin), (1u << enable_pin) | (1u << trigger_pin));
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
