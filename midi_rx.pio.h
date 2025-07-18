#pragma once

#if !PICO_NO_HARDWARE
#include "hardware/pio.h"
#endif

// ------- //
// midi_rx //
// ------- //

#define midi_rx_wrap_target 0
#define midi_rx_wrap 6

static const uint16_t midi_rx_program_instructions[] = {
            //     .wrap_target
    0x20a0, //  0: wait   1 pin, 0                   
    0x2020, //  1: wait   0 pin, 0                   
    0xe527, //  2: set    x, 7                   [5] 
    0xa542, //  3: nop                           [5] 
    0x4601, //  4: in     pins, 1                [6] 
    0x0044, //  5: jmp    x--, 4                     
    0x0000, //  6: jmp    0                          
            //     .wrap
};

#if !PICO_NO_HARDWARE
static const struct pio_program midi_rx_program = {
    .instructions = midi_rx_program_instructions,
    .length = 7,
    .origin = -1,
};

static inline pio_sm_config midi_rx_program_get_default_config(uint offset) {
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + midi_rx_wrap_target, offset + midi_rx_wrap);
    return c;
}
#endif

