#pragma once
#include <stdint.h>

bool parse(Parser *p,uint8_t byte,uint8_t *out,uint8_t *n);
void flush_notes();
bool mmc_stop(MmcBuf &buf,uint8_t b);

extern uint16_t noteMask[16][8];
extern volatile uint16_t rawMask[16][8];
extern uint8_t g_src;

static inline void set_raw(uint8_t ch, uint8_t n){
  rawMask[ch][n >> 4] |= 1u << (n & 15);
}

static inline void clr_raw(uint8_t ch, uint8_t n){
  rawMask[ch][n >> 4] &= ~(1u << (n & 15);
}

struct Parser {
  uint8_t st;
  uint8_t d[2];
  uint8_t cnt;
  bool    syx;
};

struct MmcBuf {
  uint8_t b[6];
};
