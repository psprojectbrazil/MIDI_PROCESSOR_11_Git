#include <pico/time.h>   /* alarm_pool_add_alarm_in_us, alarm_id_t */
#include <Arduino.h>     /* garante gpio_put, etc.                 */
#include "pulse_out.h"
#include "hardware/gpio.h"

#define PULSE_US 15000

static int64_t pulseLowCb(alarm_id_t id, void *user_data){
  uint32_t gpio = (uint32_t)user_data;
  gpio_put(gpio,0);
  return 0;
}

static inline void firePulse(uint32_t gpio){
  gpio_put(gpio,1);
  alarm_pool_add_alarm_in_us(alarm_pool_get_default(),PULSE_US,pulseLowCb,(void*)gpio,true);
}

void pulse_init(){
  gpio_init(PIN_CLK_OUT);
  gpio_set_dir(PIN_CLK_OUT,GPIO_OUT);
  gpio_put(PIN_CLK_OUT,0);
  gpio_init(PIN_START_OUT);
  gpio_set_dir(PIN_START_OUT,GPIO_OUT);
  gpio_put(PIN_START_OUT,0);
  gpio_init(PIN_STOP_OUT);
  gpio_set_dir(PIN_STOP_OUT,GPIO_OUT);
  gpio_put(PIN_STOP_OUT,0);
}

void pulse_onClockTick(){
  firePulse(PIN_CLK_OUT);
}

void pulse_onStart(){
  firePulse(PIN_START_OUT);
}

void pulse_onStop(){
  firePulse(PIN_STOP_OUT);
}
