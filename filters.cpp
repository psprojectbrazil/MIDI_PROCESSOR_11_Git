#include "filters.h"

uint8_t filtMask[2] = { 0, 0 };           /* tudo liberado por padrão */

bool filt_allow(uint8_t src,const uint8_t *m,uint8_t n){
  if(src > 1) return true;                /* só DIN1/2 filtram */
  uint8_t f  = filtMask[src];
  uint8_t st = m[0], hi = st & 0xF0;

  /* ------------------ Aftertouch (poly & chan) --------------------- */
  if ( (f & FILT_AFTER) && (hi == 0xA0 || hi == 0xD0) )
    return false;
  /* ------------------ Program Change / Bank Select ----------------- */
  if ( (f & FILT_PROG_BANK) && (hi == 0xC0 || (hi == 0xB0 && (m[1] == 0x00 || m[1] == 0x20))) )
    return false;
  /* ------------------ Mod-Wheel CC1 -------------------------------- */
  if ( (f & FILT_MODWHEEL) && hi == 0xB0 && m[1] == 0x01 )
    return false;
  /* ------------------ Sustain Pedal CC64 --------------------------- */
  if ( (f & FILT_SUSTAIN) && hi == 0xB0 && m[1] == 0x40 )
    return false;
  /* ------------------ Volume / Expression CC7 / CC11 -------------- */
  if ( (f & FILT_VOL_EXPR) && hi == 0xB0 && (m[1] == 0x07 || m[1] == 0x0B) )
    return false;
  /* ------------------ Outros CCs ----------------------------------- */
  if ( (f & FILT_OTHER_CC) && hi == 0xB0 )
    return false;

  return true;
}
