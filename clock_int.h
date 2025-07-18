#pragma once
#include <Arduino.h>

enum ClkSrc : uint8_t { SRC_INT, SRC_DIN1, SRC_DIN2, SRC_USB };
enum ClkMul : int8_t  { MUL_D4 = -2, MUL_D2 = -1, MUL_X1 = 0, MUL_X2 = 1, MUL_X4 = 2 };

void clk_config(ClkSrc src, uint16_t bpm, ClkMul mul);
void clk_start();
void clk_stop();
void clk_setOutMask(uint8_t mask);  /* bits 0-3 → OUT1-4 */
bool clk_isInternal();  
uint8_t clk_getOutMask();

void clk_extTick();
void clk_timeoutCheck();
void clockout_setDivision(uint8_t div);
