#pragma once
#include <Arduino.h>

/* contador global em detents (1 detent = 4 transicoes) */
extern volatile int32_t encCount;

/* chama uma unica vez no setup  */
void enc_begin(uint8_t pinA, uint8_t pinB);
