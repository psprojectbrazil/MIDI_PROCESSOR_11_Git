#include "encoder.h"
#include "hardware/gpio.h"

/* ------- tabela de transicao Gray (4x4 = 16 entradas) ------- */
static const int8_t qtab[16] = {
   0 , -1,  1,  0,
   1 ,  0,  0, -1,
  -1 ,  0,  0,  1,
   0 ,  1, -1,  0
};

static uint8_t pinA_, pinB_;
volatile int32_t encCount = 0;

/* ISR compartilhada pelos dois pinos */
static void enc_isr(uint gpio, uint32_t events){
  static uint8_t prevAB = 0;
  uint8_t a = gpio_get(pinA_);
  uint8_t b = gpio_get(pinB_);
  uint8_t curAB = (a << 1) | b;
  uint8_t idx   = (prevAB << 2) | curAB;
  encCount += qtab[idx & 15];
  prevAB    = curAB;
}

/* chamada no setup */
void enc_begin(uint8_t pinA, uint8_t pinB){
  pinA_ = pinA;
  pinB_ = pinB;
  pinMode(pinA_, INPUT_PULLUP);
  pinMode(pinB_, INPUT_PULLUP);
  gpio_set_irq_enabled_with_callback(pinA_, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, enc_isr);
  gpio_set_irq_enabled(pinB_, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
}