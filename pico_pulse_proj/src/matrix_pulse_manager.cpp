#include "matrix_pulse_manager.h"
#include "pulse_generator.pio.h"
#include "hardware/clocks.h"

MatrixPulseManager::MatrixPulseManager() : 
    _pio_hw(pio0), _pio_sm(0), _pio_offset(0), _is_ready(false),
    _mask_w0(0xFFFFFFFF), _mask_w1(0xFFFFFFFF), _mask_w2(0xFFFFFFFF), _mask_w3(0xFFFFFFFF) {}

bool MatrixPulseManager::begin(PIO pio_block, uint trigger_pin, uint enable_pin) {
    _pio_hw = pio_block;

    if (!pio_can_add_program(_pio_hw, &pulse_generator_program)) {
        return false;
    }

    _pio_offset = pio_add_program(_pio_hw, &pulse_generator_program);
    _pio_sm = pio_claim_unused_sm(_pio_hw, true);

    pio_gpio_init(_pio_hw, trigger_pin);
    pio_gpio_init(_pio_hw, enable_pin);
    
    pio_sm_set_consecutive_pindirs(_pio_hw, _pio_sm, trigger_pin, 1, true);
    pio_sm_set_consecutive_pindirs(_pio_hw, _pio_sm, enable_pin, 1, true);

    pio_sm_config c = pulse_generator_program_get_default_config(_pio_offset);
    
    // HARDWARE TRACK SEPARATION: Separates the buses completely
    sm_config_set_out_pins(&c, enable_pin, 1);     // OUT bus strictly affects 1 pin (Enable 15)
    sm_config_set_out_shift(&c, true, true, 32);  // Enable hardware Autopull at 32 bits
    sm_config_set_sideset_pins(&c, trigger_pin);   // SIDESET bus strictly affects Trigger 14
    
    sm_config_set_clkdiv(&c, 1.0f); // Default factory clock speed (125MHz / 8ns per cycle)
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

    pio_sm_init(_pio_hw, _pio_sm, _pio_offset, &c);
    
    // Force clean resting hardware matrix layout states BEFORE enabling
    pio_sm_set_pins_with_mask(_pio_hw, _pio_sm, (1u << enable_pin), (1u << enable_pin) | (1u << trigger_pin));
    pio_sm_set_enabled(_pio_hw, _pio_sm, true);

    _is_ready = true;
    return true;
}

void MatrixPulseManager::set_width_percent(uint8_t percent) {
    if (percent > 100) percent = 100;
    uint32_t low_bits = percent;

    // Default payload registers start entirely HIGH (Inactive)
    uint32_t w0 = 0xFFFFFFFF;
    uint32_t w1 = 0xFFFFFFFF;
    uint32_t w2 = 0xFFFFFFFF;
    uint32_t w3 = 0xFFFFFFFF;

    // Inject active-LOW zeros directly matching your percentage count
    for (uint32_t i = 0; i < low_bits; i++) {
        if (i < 32)       w0 &= ~(1u << i);
        else if (i < 64)  w1 &= ~(1u << (i - 32));
        else if (i < 96)  w2 &= ~(1u << (i - 64));
        else              w3 &= ~(1u << (i - 96));
    }
    
    // Cross core boundaries atomically
    _mask_w0 = w0;
    _mask_w1 = w1;
    _mask_w2 = w2;
    _mask_w3 = w3;
}

bool MatrixPulseManager::is_hardware_ready() const {
    return _is_ready;
}

void __not_in_flash_func(MatrixPulseManager::fire_blocking_pulse)() {
    uint32_t w0 = _mask_w0;
    uint32_t w1 = _mask_w1;
    uint32_t w2 = _mask_w2;
    uint32_t w3 = _mask_w3;

    _pio_hw->irq = 1u << 0; 

    // First, push the loop bit length indicator (100 total iterations - 1 for overhead = 99)
    pio_sm_put_blocking(_pio_hw, _pio_sm, 99);

    // Then, pump the 4 data stream registers down the FIFO consecutively using _pio_sm
    pio_sm_put_blocking(_pio_hw, _pio_sm, w0);
    pio_sm_put_blocking(_pio_hw, _pio_sm, w1); // FIXED: Changed pio_sm to _pio_sm
    pio_sm_put_blocking(_pio_hw, _pio_sm, w2);
    pio_sm_put_blocking(_pio_hw, _pio_sm, w3);
    
    // Busy wait: Blocks Core 1 cleanly for the exact 800ns stream window length
    while (!(_pio_hw->irq & (1u << 0))) {
        __compiler_memory_barrier(); 
    }
    _pio_hw->irq = 1u << 0; 
}
