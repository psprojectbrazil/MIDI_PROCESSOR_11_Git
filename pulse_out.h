#pragma once
#include <stdint.h>

#define PIN_CLK_OUT    9
#define PIN_START_OUT 10
#define PIN_STOP_OUT  11

void pulse_init();
void pulse_onClockTick();
void pulse_onStart();
void pulse_onStop();
