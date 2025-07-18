#include "quantizer.h"
#include "routing.h"
#include "arp.h"
#include "midi_parser.h"
#include "hardware/pio.h"

extern const uint8_t sm_tx1;  /* PIO0 – OUT 1 */
extern const uint8_t sm_tx3;  /* PIO1 – OUT 3 */

static QuantCfg qCfg;
static uint8_t  lut[128];

static inline bool tx_ready() {
  return (pio0->ctrl & (1u << sm_tx1)) || (pio1->ctrl & (1u << sm_tx3));
}

static const uint16_t scaleMask[8] = {
  0x0FFF,   /* Chromatic            */
  0x0AD5,   /* Major – C D E F G A B */
  0x0B5A,   /* Natural Minor        */
  0x0B6A,   /* Harmonic Minor       */
  0x0ADA,   /* Melodic Minor (asc.) */
  0x08D5,   /* Pentatonic Major     */
  0x095A,   /* Pentatonic Minor     */
  0x095B    /* Blues                */
};

static void rebuildLUT() {
  uint16_t m = scaleMask[qCfg.scale & 7];
  for(uint8_t n = 0; n < 128; n++){
    int8_t note = n - qCfg.root;
    int8_t d    = (note % 12 + 12) % 12;
    int8_t q    = d;
    while(!(m & (1 << q))){
      q = (q + 11) % 12;
    }
    lut[n] = uint8_t(n - d + q);
  }
}

void quant_setCfg(const QuantCfg &c) {
  qCfg = c;
  rebuildLUT();
  if(!tx_ready()) return;
  flush_notes();          /* libera NOTE ON pendentes */
  arp_allNotesOff();      /* solta a nota atual do Arp */
  if(c.active & 1) {      /* DIN 1 */
    for(uint8_t ch = 0; ch < 16; ++ch) {
      route_sendByte(ROUTE_DIN1, 0xB0 | ch);
      route_sendByte(ROUTE_DIN1, 0x78);
      route_sendByte(ROUTE_DIN1, 0x00);
      route_sendByte(ROUTE_DIN1, 0xB0 | ch);
      route_sendByte(ROUTE_DIN1, 0x7B);
      route_sendByte(ROUTE_DIN1, 0x00);
    }
  }
  if(c.active & 2) {      /* DIN 2 */
    for(uint8_t ch = 0; ch < 16; ++ch) {
      route_sendByte(ROUTE_DIN2, 0xB0 | ch);
      route_sendByte(ROUTE_DIN2, 0x78);
      route_sendByte(ROUTE_DIN2, 0x00);
      route_sendByte(ROUTE_DIN2, 0xB0 | ch);
      route_sendByte(ROUTE_DIN2, 0x7B);
      route_sendByte(ROUTE_DIN2, 0x00);
    }
  }
}

uint8_t quant_apply(uint8_t src, uint8_t note) {
  if(qCfg.active == 0) return note;
  if(qCfg.active == 1 && src != ROUTE_DIN1) return note;
  if(qCfg.active == 2 && src != ROUTE_DIN2) return note;
  return lut[note];
}
