#pragma once
#include <stdint.h>
#include "config.h"

void quant_setCfg(const QuantCfg &c);

uint8_t quant_apply(uint8_t src, uint8_t note);