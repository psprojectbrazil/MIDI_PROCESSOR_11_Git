#include <pico/time.h>
#include "clock_int.h"
#include "midi_fifo.h"
#include "arp.h"
#include "echo.h"
#include "pulse_out.h"
#include "midi_realtime.h"

extern MidiFifo txQ;

/* -------------- estado -------------- */
static volatile bool clkRun  = false;
static volatile ClkSrc clkSrc  = SRC_INT;
static volatile ClkMul clkMul  = MUL_X1;
static volatile uint16_t clkBpm  = 120;
static volatile uint8_t clkMask = 0x0F;     /* OUT1..4 */
static repeating_timer_t clkTmr;
static volatile bool clkRequested = false;  /* deseja rodar? */
static volatile uint32_t extLastUs  = 0;    /* micros do último 0xF8 recebido */
static volatile uint32_t extPeriodUs = 0;   /* intervalo médio entre ticks    */

/* -------------- helpers -------------- */
static inline uint32_t us_per_tick(uint16_t bpm, ClkMul m){
  uint32_t base = 60000000UL / (bpm * 24UL);    /* 24 PPQN */
  if(m < 0)           return base << (-m);      /* ÷2, ÷4  */
  else if(m > 0)      return base >>   m;       /* ×2, ×4  */
  else                return base;              /* ×1      */
}
static inline void put_clk(uint8_t b){ fifo_put(&txQ, b); }

/* -------------- timer ISR -------------- */
static bool clk_cb(repeating_timer_t *){
  arp_clockTick();
  echo_clockTick();
  put_clk(0xF8);
  midi_handleRealtime(0xF8);    /* gera pulso GP9 (24 PPQN ÷24 → 1 BPM) */
  return clkRun;
}

/* -------------- API -------------------- */
void clk_setOutMask(uint8_t mask){ clkMask = mask & 0x0F; }
uint8_t clk_getOutMask(){ return clkMask; }
bool clk_isInternal(){ return (clkSrc == SRC_INT) && clkRun; }

void clk_config(ClkSrc src, uint16_t bpm, ClkMul mul) {
  bool wasInt = (clkSrc == SRC_INT);
  clkSrc = src;
  clkBpm = constrain(bpm, 20, 300);
  clkMul = mul;
  bool nowInt = (clkSrc == SRC_INT);
  if(wasInt && !nowInt) {                 /* INT → externo: pausa timer */
    if(clkRun) {
      cancel_repeating_timer(&clkTmr);
      clkRun = false;
    }
  }else if(!wasInt && nowInt) {           /* externo → INT */
    if(clkRequested && !clkRun) {
      clkRun = true;
      add_repeating_timer_us(-(int32_t)us_per_tick(clkBpm, clkMul), clk_cb, nullptr, &clkTmr);
    }
  }else if(nowInt && clkRun) {            /* continua INT: só ajusta período */
    cancel_repeating_timer(&clkTmr);
    add_repeating_timer_us(-(int32_t)us_per_tick(clkBpm, clkMul), clk_cb, nullptr, &clkTmr);
  }
}

void clk_start(){
  clkRequested = true;        /* <-- grava a intenção */
  if(clkSrc != SRC_INT || clkRun) return;
  clkRun = true;
  add_repeating_timer_us(-(int32_t)us_per_tick(clkBpm, clkMul), clk_cb, nullptr, &clkTmr);
  put_clk(0xFA);              /* Start para as saídas MIDI */
  midi_handleRealtime(0xFA);  /* pulso START (GP10) */
}

void clk_stop(){
  clkRequested = false;         /* <-- limpa intenção */
  if(!clkRun) return;
  cancel_repeating_timer(&clkTmr);
  clkRun = false;
  put_clk(0xFC);                /* Stop para as saídas MIDI */
  midi_handleRealtime(0xFC);    /* pulso STOP (P11) */
}

void clk_extTick() {
  uint32_t now = micros();
  extPeriodUs  = now - extLastUs;       /* atualiza período estimado */
  extLastUs    = now;
}

void clk_timeoutCheck() {
  if(clkSrc == SRC_INT) return;         /* só vale para fontes externas */
  if(!clkRun)        return;
  uint32_t now   = micros();
  uint32_t limit = extPeriodUs ? extPeriodUs * 2 : 500000;  /* ~2 ticks ou 0,5 s */
  if(now - extLastUs > limit) {
    clk_stop();                         /* já envia 0xFC para todas as saídas */
  }
}
