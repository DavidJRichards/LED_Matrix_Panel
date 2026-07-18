#ifndef MATRIX_PULSE_MANAGER_H
#define MATRIX_PULSE_MANAGER_H

#include <Arduino.h>
#include "hardware/pio.h"

class MatrixPulseManager {
public:
    MatrixPulseManager();
    bool begin(PIO pio_block, uint trigger_pin, uint enable_pin);
    void set_width_percent(uint8_t percent);
    bool is_hardware_ready() const;
    void fire_blocking_pulse();

private:
    PIO _pio_hw;
    uint _pio_sm;
    uint _pio_offset;
    volatile bool _is_ready;

    volatile uint32_t _mask_w0;
    volatile uint32_t _mask_w1;
    volatile uint32_t _mask_w2;
    volatile uint32_t _mask_w3;
};

#endif // MATRIX_PULSE_MANAGER_H
