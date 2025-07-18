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
  /* --- NOTE OFF + CC 78/7B para notas que estavam soando --- */
  auto send_offs = [&](uint8_t route){
    for(uint8_t ch = 0; ch < 16; ++ch){
      for(uint8_t grp = 0; grp < 8; ++grp){
        uint16_t m = noteMask[ch][grp];
        if(!m) continue;
        for(uint8_t bit = 0; bit < 16; ++bit){
          if(m & (1u << bit)){
            uint8_t n = (grp << 4) | bit;
            route_sendByte(route, 0x80 | ch);
            route_sendByte(route, n);
            route_sendByte(route, 0);
          }
        }
      }
      route_sendByte(route, 0xB0 | ch);
      route_sendByte(route, 0x78);
      route_sendByte(route, 0x00);
      route_sendByte(route, 0xB0 | ch);
      route_sendByte(route, 0x7B);
      route_sendByte(route, 0x00);
    }
  };
  if(c.active & 1) send_offs(ROUTE_DIN1);
  if(c.active & 2) send_offs(ROUTE_DIN2);
  for(uint8_t ch = 0; ch < 16; ch++){   /* --- limpa apenas o mapa já quantizado --- */
    for(uint8_t grp = 0; grp < 8; grp++){
      noteMask[ch][grp] = 0;
    }
  }
  arp_allNotesOff();                    /* reinicia o ciclo do Arp */
}


uint8_t quant_apply(uint8_t src, uint8_t note) {
  if(qCfg.active == 0) return note;
  if(qCfg.active == 1 && src != ROUTE_DIN1) return note;
  if(qCfg.active == 2 && src != ROUTE_DIN2) return note;
  return lut[note];
}
