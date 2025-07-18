#pragma once
#include <stdint.h>

void midi_handleRealtime(uint8_t b);
void clockout_setDivision(uint8_t clocksPerPulse);