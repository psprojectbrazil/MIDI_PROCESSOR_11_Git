#include <stdint.h>
#include <Arduino.h>
#include "hardware/gpio.h"
#include "pulse_out.h"
#include "midi_realtime.h"
#include "ui_menu.h"

struct ClockOut {
  bool running;
  uint8_t cnt;
};

static ClockOut clkOut = { false, 0 };
static uint8_t clkDiv  = 6;

void clockout_setDivision(uint8_t clocksPerPulse){
  if(clocksPerPulse) clkDiv = clocksPerPulse;
}

void midi_handleRealtime(uint8_t b){
  switch(b){
    case 0xFA: {
      clkOut.running = true;
      clkOut.cnt = (clkDiv ? clkDiv - 1 : 0);
      gpio_put(PIN_CLK_OUT, 0);
      pulse_onStart();
      break;
    }
    case 0xFB: {
      clkOut.running = true;
      pulse_onStart();
      break;
    }
    case 0xFC: {
      clkOut.running = false;
      gpio_put(PIN_CLK_OUT, 0);
      pulse_onStop();
      break;
    }
    case 0xF8: {
      if(!clkOut.running){
        clkOut.running = true;
      }
      clkOut.cnt++;
      if(clkOut.cnt >= clkDiv){
        clkOut.cnt = 0;
        pulse_onClockTick();
      }
      break;
    }
  }
}
