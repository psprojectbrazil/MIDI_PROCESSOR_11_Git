#include "midi_parser.h"
#include "midi_fifo.h"
#include "arp.h"
#include "echo.h"
#include "quantizer.h"
#include "midi_realtime.h"

extern MidiFifo txQ;
extern Config   cfg;
extern void     q_put(uint8_t src, uint8_t b);

/* ---------- estado de notas ---------- */
uint16_t noteMask[16][8] = {0};
volatile uint16_t rawMask[16][8] = {0};

static inline void set_note(uint8_t ch, uint8_t n) {
  noteMask[ch][n >> 4] |= 1u << (n & 15);
}

static inline void clr_note(uint8_t ch, uint8_t n) {
  noteMask[ch][n >> 4] &= ~(1u << (n & 15));
}

static inline bool notes_active() {
  for(uint8_t ch = 0; ch < 16; ch++)
    for(uint8_t g = 0; g < 8; g++)
      if(noteMask[ch][g]) return true;
  return false;
}

static inline uint8_t msg_len(uint8_t st) {
  uint8_t h = st & 0xF0;
  return (h == 0xC0 || h == 0xD0) ? 2 : 3;
}

static bool mmc_play(uint8_t v) {
  static MmcBuf buf = {{0}};
  for(uint8_t i = 0;i < 5;i++) buf.b[i] = buf.b[i + 1];
  buf.b[5] = v;
  return buf.b[0] == 0xF0 && buf.b[1] == 0x7F && buf.b[3] == 0x06 && buf.b[4] == 0x02 && buf.b[5] == 0xF7;
}

bool mmc_stop(MmcBuf &buf,uint8_t v) {
  for(uint8_t i = 0;i < 5;i++) buf.b[i] = buf.b[i + 1];
  buf.b[5] = v;
  return buf.b[0] == 0xF0 && buf.b[1] == 0x7F && buf.b[3] == 0x06 && buf.b[4] == 0x01 && buf.b[5] == 0xF7;
}

/* ---------- libera notas ---------- */
void flush_notes() {
  if(!notes_active()) return;
  for(uint8_t ch = 0; ch < 16; ch++) {
    bool sent = false;
    for(uint8_t grp = 0; grp < 8; grp++) {
      uint16_t m = noteMask[ch][grp];
      if(!m) continue;
      for(uint8_t bit = 0; bit < 16; bit++)
        if(m & (1u << bit)) {
          uint8_t n = (grp << 4) | bit;
          q_put(g_src, 0x80 | ch);
          q_put(g_src, n);
          q_put(g_src, 0);
        }
      noteMask[ch][grp] = 0;
      sent = true;
    }
    if(sent) {
      q_put(g_src, 0xB0 | ch);
      q_put(g_src, 0x40);
      q_put(g_src, 0);
    }
  }
}

/* ---------- parser ---------- */
bool parse(Parser *p, uint8_t b, uint8_t *out, uint8_t *n) {
  if(b >= 0xF8) midi_handleRealtime(b);
  if(b == 0xF8) { arp_clockTick(); echo_clockTick(); }
  else if(b == 0xFC) { arp_allNotesOff(); echo_panic(); }
  *n = 0;
  if(b >= 0xF8) { out[0] = b; *n = 1; return true; }
  if(p->syx) {
    mmc_play(b);          /* só detecta; não bloqueia */
    out[0] = b; *n = 1;
    if(b == 0xF7) { p->syx = false; p->st = 0; }
    return true;
  }
  if(b == 0xF0) {
    p->syx = true; p->st = 0;
    out[0] = 0xF0; *n = 1;
    return true;
  }
  if(b & 0x80) { p->st = b; p->cnt = 0; return false; }
  if(!p->st) return false;
  uint8_t need = msg_len(p->st) - 1;
  p->d[p->cnt++] = b;
  if(p->cnt < need) return false;
  out[0] = p->st;
  for(uint8_t i = 0; i < need; i++) out[i + 1] = p->d[i];
  *n = need + 1; p->cnt = 0;
  uint8_t hiT = out[0] & 0xF0;
  if(hiT == 0x90 || hiT == 0x80) {
    // 1. transposição 
    int16_t note = (int16_t)out[1] + (int8_t)cfg.transp[g_src];
    if(note < 0) note = 0;
    if(note > 127) note = 127;
    uint8_t chTmp = out[0] & 0x0F;
    if(hiT == 0x90){
      if(out[2]) set_raw(chTmp, (uint8_t)note);
      else       clr_raw(chTmp, (uint8_t)note);
    }else{
      clr_raw(chTmp, (uint8_t)note);
    }
    // 2. quantização
    out[1] = quant_apply(g_src, (uint8_t)note);
  }
  uint8_t hi = out[0] & 0xF0;
  uint8_t ch = out[0] & 0x0F;
  bool noteOn  = (hi == 0x90 && out[2]);
  bool noteOff = (hi == 0x80) || (hi == 0x90 && !out[2]);
  if(noteOn || noteOff) echo_enqueue(out);
  bool srcOk  = (cfg.arp.src == 3) || (cfg.arp.src == g_src);
  bool useArp = (cfg.arp.src != 2) && srcOk && arp_isActive();
  if(noteOn && useArp)      { set_note(ch, out[1]); *n = 0; }
  else if(noteOff && useArp){ clr_note(ch, out[1]); *n = 0; }
  if(hi == 0xB0 && (out[1]==0x7B || out[1]==0x78) && out[2]==0)
    if(notes_active()) flush_notes();
  return *n;
}
