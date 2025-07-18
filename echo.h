#pragma once
#include <stdint.h>
#include "config.h"
#include "midi_fifo.h"

void echo_setCfg(const EchoCfg &c);
void echo_enqueue(const uint8_t *msg);
void echo_clockTick();
bool echo_isActive();
void echo_panic();