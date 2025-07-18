#include "arp.h"
#include "midi_parser.h"
#include "routing.h"
#include "quantizer.h" 

/* ---------- configuração (global) ---------- */
/* src: 0 = DIN 1 | 1 = DIN 2 | 3 = DIN 1+2 */
ArpCfg arpCfg = { ARP_OFF, 1, 50, 12 };   /* Off, 1-oct, 50 %, 1/8 */

/* ---------- estado interno ---------- */
static uint8_t pool[16];
static uint8_t poolCnt = 0;
static uint8_t idx = 0;
static uint8_t oct = 0;
static uint16_t seqPos = 0;
static uint16_t seqMax = 0;
static int8_t seqDir = 1;
static uint8_t tickCnt = 0;
static uint8_t gateCnt = 0;
static uint8_t gateTicks = 0;
static uint8_t curNote = 0xFF;

/* ---------- funções auxiliares ---------- */
static inline void send3(uint8_t src, uint8_t st, uint8_t d1, uint8_t d2)
{
  route_sendByte(src, st);
  route_sendByte(src, d1);
  route_sendByte(src, d2);
}

static void sendNoteOff()
{
  if (curNote == 0xFF) return;

  if (arpCfg.src == 0 || arpCfg.src == 3)
    send3(ROUTE_DIN1, 0x80, curNote, 0);

  if (arpCfg.src == 1 || arpCfg.src == 3)
    send3(ROUTE_DIN2, 0x80, curNote, 0);

  curNote = 0xFF;
}

static void sendNoteOn(uint8_t n)
{
  if (arpCfg.src == 0 || arpCfg.src == 3)
    send3(ROUTE_DIN1, 0x90, n, 100);

  if (arpCfg.src == 1 || arpCfg.src == 3)
    send3(ROUTE_DIN2, 0x90, n, 100);

  curNote = n;
}

/* reconstrói a lista de notas mantidas */
static void rebuildPool() {
  uint8_t old = poolCnt;
  poolCnt = 0;
  uint8_t srcRoute = (arpCfg.src == 1) ? ROUTE_DIN2 : ROUTE_DIN1;
  for(uint8_t n = 0; n < 128 && poolCnt < 16; n++) {
    for(uint8_t ch = 0; ch < 16; ch++){
      if(rawMask[ch][n >> 4] & (1u << (n & 15))) {
        uint8_t q = quant_apply(srcRoute, n);
        bool dup = false;
        for(uint8_t i = 0; i < poolCnt; i++) {
          if(pool[i] == q) {
            dup = true;
            break;
          }
        }
        if(!dup) {
          pool[poolCnt++] = q;
        }
        break;
      }
    }
  }
  seqMax = poolCnt * arpCfg.octaves;
  if((old == 0 && poolCnt) || poolCnt == 0) {
    idx = 0;
    oct = 0;
    seqPos = 0;
    seqDir = 1;
    sendNoteOff();
  }
}

/* ---------- API ---------- */
void arp_setCfg(const ArpCfg &c) { arpCfg = c; }

void arp_clockTick()
{
  if (arpCfg.mode == ARP_OFF) return;

  if (curNote != 0xFF)
  {
    if (++gateCnt >= gateTicks)
      sendNoteOff();
  }

  if (++tickCnt < arpCfg.divTicks) return;
  tickCnt = 0;

  rebuildPool();
  if (poolCnt == 0) return;

  uint8_t note = 0;

  switch (arpCfg.mode)
  {
    case ARP_UP:
      note = pool[idx] + 12 * oct;
      if (++idx >= poolCnt)
      {
        idx = 0;
        if (++oct >= arpCfg.octaves) oct = 0;
      }
      break;

    case ARP_DOWN:
      note = pool[idx] + 12 * oct;
      if (idx == 0)
      {
        idx = poolCnt - 1;
        oct = (oct == 0) ? arpCfg.octaves - 1 : oct - 1;
      }
      else
        --idx;
      break;

    case ARP_UPDOWN:
      if (seqMax == 0) return;
      note = pool[seqPos % poolCnt] + 12 * (seqPos / poolCnt);
      if (seqDir > 0)
      {
        if (++seqPos >= seqMax)
        {
          seqDir = -1;
          seqPos = seqMax - 2;
        }
      }
      else
      {
        if (seqPos == 0)
        {
          seqDir = 1;
          seqPos = 1;
        }
        else
          --seqPos;
      }
      break;

    case ARP_RANDOM:
      idx = random(poolCnt);
      oct = random(arpCfg.octaves);
      note = pool[idx] + 12 * oct;
      break;

    default:
      return;
  }

  sendNoteOn(note);
  gateCnt = 0;

  uint16_t gt = (uint32_t)arpCfg.divTicks * arpCfg.gatePct / 100;
  if (gt == 0) gt = 1;
  if (gt >= arpCfg.divTicks) gt = arpCfg.divTicks - 1;
  gateTicks = gt;
}

void arp_allNotesOff() { sendNoteOff(); }

bool arp_isActive() { return arpCfg.mode != ARP_OFF; }


