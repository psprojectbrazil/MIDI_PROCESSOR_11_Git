#include "routing.h"
#include "clock_int.h"
#include "hardware/pio.h"
#include "config.h"
#include <Adafruit_TinyUSB.h>

#define DST_USB  0x10

extern Adafruit_USBD_MIDI usbMIDI;

extern const uint8_t sm_tx1, sm_tx2, sm_tx3, sm_tx4;

uint8_t routingMask[3] = { 0x0F, 0x0F, 0x0F };

uint8_t route_getMask(uint8_t src) {
  return routingMask[src & 3];
}

void route_toggle(uint8_t src, uint8_t out) {
  routingMask[src & 3] ^= (1u << out);
}

void route_sendByte(uint8_t src, uint8_t b){
  bool isClk = (b == 0xF8) || (b == 0xFA) || (b == 0xFC);
  uint8_t mask = routingMask[src & 3];
  if(src == ROUTE_DIN1) {
    if(cfg.usbInSel == 0 || cfg.usbInSel == 3){ mask |= DST_USB; }
  } else if(src == ROUTE_DIN2) {
    if(cfg.usbInSel == 1 || cfg.usbInSel == 3){ mask |= DST_USB; }
  }
  if(isClk){ mask &= clk_getOutMask(); }
  if(!mask){ return; }
  if(mask & 1){ pio_sm_put_blocking(pio0, sm_tx1, b); }
  if(mask & 2){ pio_sm_put_blocking(pio0, sm_tx2, b); }
  if(mask & 4){ pio_sm_put_blocking(pio1, sm_tx3, b); }
  if(mask & 8){ pio_sm_put_blocking(pio1, sm_tx4, b); }
  if(mask & DST_USB){ usbMIDI.write(&b, 1); }
}
