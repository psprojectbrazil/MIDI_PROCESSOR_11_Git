#pragma once
#include <stdint.h>

enum FiltBit : uint8_t {
  /* ordem = ordem de exibição no menu */
  FILT_AFTER      = 1u << 0,   /* Channel- & Poly-Aftertouch  */
  FILT_PROG_BANK  = 1u << 1,   /* Program-Change + Bank-Select */
  FILT_MODWHEEL   = 1u << 2,   /* CC 1                         */
  FILT_SUSTAIN    = 1u << 3,   /* CC 64                        */
  FILT_VOL_EXPR   = 1u << 4,   /* CC 7  + CC 11                */
  FILT_OTHER_CC   = 1u << 5,   /* restantes CCs                */
};

extern uint8_t filtMask[2];              /* [0] DIN1  [1] DIN2 */

/* true → encaminha, false → descarta */
bool filt_allow(uint8_t src, const uint8_t *msg, uint8_t len);
