#ifndef MIDI_FIFO_H
#define MIDI_FIFO_H

#include <Arduino.h> 

#define FIFO_SIZE 256          // potencias de 2 facilitam masking

typedef struct {
  volatile uint8_t buf[FIFO_SIZE];
  volatile uint16_t head;
  volatile uint16_t tail;
} MidiFifo;

static inline void fifo_init(MidiFifo *f) {
  f->head = 0;
  f->tail = 0;
}

static inline bool fifo_full(const MidiFifo *f) {
  return ((f->head + 1) & (FIFO_SIZE - 1)) == f->tail;
}

static inline bool fifo_empty(const MidiFifo *f) {
  return f->head == f->tail;
}

static inline void fifo_put(MidiFifo *f, uint8_t b) {
  if (!fifo_full(f)) {
    f->buf[f->head] = b;
    f->head = (f->head + 1) & (FIFO_SIZE - 1);
  }
}

static inline uint8_t fifo_get(MidiFifo *f) {
  uint8_t b = f->buf[f->tail];
  f->tail = (f->tail + 1) & (FIFO_SIZE - 1);
  return b;
}

#endif
