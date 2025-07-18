#pragma once

#if !PICO_NO_HARDWARE
#include "hardware/pio.h"
#endif

// ------- //
// midi_tx //
// ------- //

#define midi_tx_wrap_target 0
#define midi_tx_wrap 5

static const uint16_t midi_tx_program_instructions[] = {
            //     .wrap_target
    0x98a0, //  0: pull   block           side 1     
    0xb742, //  1: nop                    side 0 [7] 
    0xe027, //  2: set    x, 7                       
    0x6701, //  3: out    pins, 1                [7] 
    0x0043, //  4: jmp    x--, 3                     
    0xbf42, //  5: nop                    side 1 [7] 
            //     .wrap
};

#if !PICO_NO_HARDWARE
static const struct pio_program midi_tx_program = {
    .instructions = midi_tx_program_instructions,
    .length = 6,
    .origin = -1,
};

static inline pio_sm_config midi_tx_program_get_default_config(uint offset) {
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + midi_tx_wrap_target, offset + midi_tx_wrap);
    sm_config_set_sideset(&c, 2, true, false);
    return c;
}
#endif

