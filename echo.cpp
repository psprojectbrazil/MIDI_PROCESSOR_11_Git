#include "echo.h"
#include "config.h"
#include "routing.h"           /* route_sendByte() */

#define ECHO_QSZ   32
#define ECHO_QMASK (ECHO_QSZ - 1)

struct EchoEvent {
  uint8_t st, d1, d2, vel;     /* status, nota, vel original/actual */
  uint16_t t;                  /* contagem até a próxima batida      */
  uint8_t left;                /* repetições restantes               */
};

static EchoEvent q[ECHO_QSZ];
static uint8_t head = 0, tail = 0;

static inline bool     q_empty() { return head == tail; }
static inline bool      q_full() { return ((tail + 1) & ECHO_QMASK) == head; }
static inline uint8_t  q_next(uint8_t i) { return (uint8_t)((i + 1) & ECHO_QMASK); }

extern Config  cfg;     /* estrutura global */
extern uint8_t g_src;   /* origem do último byte (DIN1/2) */

/* ---------- envio de 3 bytes pelo roteador ---------- */
static inline void send3(uint8_t src, uint8_t st, uint8_t d1, uint8_t d2) {
  route_sendByte(src, st);
  route_sendByte(src, d1);
  route_sendByte(src, d2);
}

/* envia a mensagem (Note-On ou Note-Off) para as portas activas */
static void echo_send(uint8_t st, uint8_t d1, uint8_t vel) {
  if (cfg.echo.src == 0 || cfg.echo.src == 3)          /* DIN 1        */
    send3(ROUTE_DIN1, st, d1, vel);
  if (cfg.echo.src == 1 || cfg.echo.src == 3)          /* DIN 2        */
    send3(ROUTE_DIN2, st, d1, vel);
}

/* ---------- interface ---------- */
void echo_setCfg(const EchoCfg &c) {
  cfg.echo = c;
  if (cfg.echo.src == 2 || !cfg.echo.repeats)   /* OFF / 0 rep. */
    head = tail = 0;                            /* limpa fila   */
}

bool echo_isActive() {
  bool match = (cfg.echo.src == 3) || (cfg.echo.src == g_src);  /* 3 = DIN1+2 */
  return (cfg.echo.src != 2) && match && cfg.echo.repeats;
}

/* ---------- enfileira a mensagem original ---------- */
void echo_enqueue(const uint8_t *m) {
  if (!echo_isActive() || q_full()) return;
  EchoEvent &e = q[tail];
  uint8_t st = m[0];
  uint8_t d1 = m[1];
  uint8_t d2 = m[2];
  /* normaliza Note-Off codificado como Note-On vel 0 */
  if ((st & 0xF0) == 0x90 && d2 == 0) {
    st = (st & 0x0F) | 0x80;     /* vira 0x8n */
    d2 = 0;
  }
  e.st   = st;
  e.d1   = d1;
  e.d2   = d2;
  e.vel  = d2;                   /* valor que sofrerá decaimento      */
  e.t    = cfg.echo.divTicks;
  e.left = cfg.echo.repeats;
  tail = q_next(tail);
}

/* ---------- avança um tick ---------- */
void echo_clockTick() {
  if (q_empty()) return;
  uint8_t i = head;
  do {
    EchoEvent &e = q[i];
    if (e.left) {
      if (e.t) --e.t;
      if (!e.t) {
        /* envia esta repetição */
        uint8_t hi = e.st & 0xF0;
        uint8_t vel = (hi == 0x90) ? e.vel : e.d2;     /* off mantém 0 */
        echo_send(e.st, e.d1, vel);
        /* prepara próxima, se ainda houver */
        if (--e.left) {
          e.t = cfg.echo.divTicks;
          if (hi == 0x90 && e.vel) {                   /* decai só Note-On */
            uint8_t dec = cfg.echo.velDecay;
            e.vel = (e.vel > dec) ? (uint8_t)(e.vel - dec) : 1;
          }
        }
      }
    }
    i = q_next(i);
  } while (i != tail);
  /* descarta eventos que terminaram */
  while (head != tail && q[head].left == 0)
    head = q_next(head);
}

void echo_panic() {
  /* Envia All-Notes-Off em todos os canais nas portas activas */
  for (uint8_t ch = 0; ch < 16; ch++) {
    if (cfg.echo.src == 0 || cfg.echo.src == 3)      /* DIN 1      */
      send3(ROUTE_DIN1, 0xB0 | ch, 0x7B, 0);
    if (cfg.echo.src == 1 || cfg.echo.src == 3)      /* DIN 2      */
      send3(ROUTE_DIN2, 0xB0 | ch, 0x7B, 0);
  }
  head = tail = 0;  /* limpa a fila — evita repetições órfãs */
}
