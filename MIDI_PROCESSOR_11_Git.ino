//  ================================================
//  = Projeto: EMW  -  MIDI PROCESSOR              =
//  = Microcontrolador: RP2040-Zero                =
//  = Frequência: 240MHz (overclocked)             =
//  = Desenvolvedor: Paulo Sergio dos Santos       =
//  = Iniciado em: 25/06/2025                      =
//  = Updated: 03/07/2025                          =
//  ================================================

#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_TinyUSB.h>
#include <pico/multicore.h>
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "midi_rx.pio.h"
#include "midi_tx.pio.h"
#include "config.h"
#include "clock_int.h"
#include "ui_menu.h"
#include "encoder.h"
#include "midi_fifo.h"
#include "midi_parser.h"
#include "filters.h"
#include "routing.h"
#include "echo.h"
#include "quantizer.h"
#include "pulse_out.h"
#include "midi_realtime.h" 
#include "EMWlogo.h"

/* Entradas */
#define MIDI_IN1_PIN 0
#define MIDI_IN2_PIN 1
/* Saídas DIN */
#define MIDI_OUT1_PIN 2  // PIO0 SM2
#define MIDI_OUT2_PIN 3  // PIO0 SM3
#define MIDI_OUT3_PIN 4  // PIO1 SM0
#define MIDI_OUT4_PIN 5  // PIO1 SM1
/* Encoder + Display + LED */
#define ENC_A_PIN 6
#define ENC_B_PIN 7
#define ENC_BTN_PIN 8
#define OLED_SDA 12
#define OLED_SCL 13
#define LED_PIN 14
#define OLED_RST -1
#define DISP_W 128
#define DISP_H 64
/* UI e buffers */
#define LONG_MS 600
#define DEB_MS 40
#define DRAW_INTERVAL_MS 16

/* ponteiro local para PIO0 */
PIO pio = pio0;

uint8_t g_src = 0;
/* State-machines */
const uint sm_rx1  = 0;  /* PIO0 */
const uint sm_rx2  = 1;  /* PIO0 */
extern const uint8_t sm_tx1;
extern const uint8_t sm_tx2;
extern const uint8_t sm_tx3;
extern const uint8_t sm_tx4;
const uint8_t sm_tx1 = 2;   /* PIO0 (OUT1) */
const uint8_t sm_tx2 = 3;   /* PIO0 (OUT2) */
const uint8_t sm_tx3 = 0;   /* PIO1 (OUT3) */
const uint8_t sm_tx4 = 1;   /* PIO1 (OUT4) */

Adafruit_SSD1306 display(DISP_W, DISP_H, &Wire, OLED_RST);
Adafruit_USBD_MIDI usbMIDI;

Parser pA = {0}, pB = {0};
Parser parserUsb = {};
MmcBuf mmcA = {{0}}, mmcB = {{0}};

extern MidiFifo txQ;

volatile bool nv_dirty = false;
volatile bool cfg_dirty = false;
volatile uint8_t lastNote = 0;
volatile uint8_t lastVel  = 0;
volatile uint32_t ledOffAt = 0;

/* ---------- fila paralela para SRC + BYTE ---------- */
struct TxMsg{ uint8_t src; uint8_t data; };

static volatile TxMsg txBuf[256];
static volatile uint16_t qHead = 0;
static volatile uint16_t qTail = 0;

static inline bool q_full (){ return ((qHead + 1u) & 255) == qTail; }
static inline bool q_empty(){ return qHead == qTail; }

void q_put(uint8_t src,uint8_t b){
  if(q_full()) return;
  txBuf[qHead].src  = src;
  txBuf[qHead].data = b;
  qHead = (qHead + 1u) & 255;
}

static inline void q_get(uint8_t *src,uint8_t *b){
  *src = txBuf[qTail].src;
  *b   = txBuf[qTail].data;
  qTail = (qTail + 1u) & 255;
}

/* PIO init */
void midi_rx_init(PIO p, uint sm, uint pin) {
  uint off = pio_add_program(p, &midi_rx_program);
  pio_gpio_init(p, pin);
  pio_sm_set_consecutive_pindirs(p, sm, pin, 1, false);
  auto c = midi_rx_program_get_default_config(off);
  sm_config_set_clkdiv(&c, (float)clock_get_hz(clk_sys) / 31250 / 8);
  sm_config_set_in_pins(&c, pin); sm_config_set_jmp_pin(&c, pin);
  sm_config_set_in_shift(&c, true, true, 8);
  pio_sm_init(p, sm, off, &c); pio_sm_set_enabled(p, sm, true);
}

void midi_tx_init(PIO p, uint sm, uint pin) {
  uint off = pio_add_program(p, &midi_tx_program);
  pio_gpio_init(p, pin);
  pio_sm_set_consecutive_pindirs(p, sm, pin, 1, true);
  auto c = midi_tx_program_get_default_config(off);
  sm_config_set_clkdiv(&c, (float)clock_get_hz(clk_sys) / (31250 * 9));
  sm_config_set_out_pins(&c, pin, 1);
  sm_config_set_sideset_pins(&c, pin);
  sm_config_set_out_shift(&c, true, false, 0);
  pio_sm_init(p, sm, off, &c); pio_sm_set_enabled(p, sm, true);
}

/* ---------- Core 1 ---------- */
void core1_main() {
  bool btnOld = true;
  uint32_t tPress = 0;
  uint32_t tDeb = 0;
  uint32_t lastDraw = millis();
  int32_t prevStep = encCount >> 2;
  constexpr uint8_t ROUTE_USB = 2;

  for(;;) {
    /* ===== MIDI-USB IN =============================================== */
    uint8_t ev[4];
    while(usbMIDI.readPacket(ev)) {                             /* lê 4 bytes de cada vez */
      uint8_t cin = ev[0] & 0x0F;
      uint8_t buf[3] = { ev[1], ev[2], ev[3] };
      uint8_t len = (cin == 0x5 || cin == 0xF) ? 1 : (cin == 0x2 || cin == 0x6 || cin == 0xC || cin == 0xD) ? 2 : 3;
      for(uint8_t i = 0; i < len; ++i) {
        uint8_t n;
        uint8_t pkt[3];
        g_src = 0;                                              /* 0 ou 1 – garante transpose */
        if(parse(&parserUsb, buf[i], pkt, &n)) {
          for(uint8_t k = 0; k < n; ++k) {
            route_sendByte(ROUTE_USB, pkt[k]);
          }
        }
      }
    }

    int32_t step = encCount >> 2;
    int32_t diff = step - prevStep;
    if(diff) {
      prevStep = step;

      if(uiMode == MODE_EDIT) {
        bool changed = false;

        /* ---------- Clock -------------------------------------------- */
        if(curList == menuClock) {
          if(cursor == CLK_IDX_SRC) {
            int8_t v = (int8_t)cfgSrc + diff;
            v = (v + 4) % 4;
            if(v != cfgSrc) { cfgSrc = (ClkSrc)v; changed = true; }
          } else if(cursor == CLK_IDX_BPM) {
            int32_t v = (int32_t)cfgBpm + diff;
            v = constrain(v, 20, 300);
            if(v != cfgBpm) { cfgBpm = v; changed = true; }
          } else if(cursor == CLK_IDX_MULT) {
            int8_t v = (int8_t)cfgMul + diff;
            v = constrain(v, -2, 2);
            if(v != cfgMul) { cfgMul = (ClkMul)v; changed = true; }
          } else if(cursor == CLK_IDX_PULSE) {
            const uint8_t order[4] = { 3, 6, 12, 24 };
            int8_t idx = 0;
            while(idx < 4 && order[idx] != cfg.clkPulseDiv) ++idx;
            idx = (idx + diff + 4) % 4;
            uint8_t v = order[idx];
            if(v != cfg.clkPulseDiv){
              cfg.clkPulseDiv = v;
              clockout_setDivision(cfg.clkPulseDiv);
              changed = true;
            }
          }
        }

        /* ---------- FX : Arpeggiator --------------------------------- */
        else if(curList == fxArp) {
          /* cursor: 0 = Active on, 1 = Mode, 2 = Oct, 3 = Gate, 4 = Sync */
          if(cursor == 0) {
            const uint8_t order[4] = { 2, 0, 1, 3 };            /* OFF, DIN1, DIN2, DIN1+2 */
            int8_t idx = 0;
            while(idx < 4 && order[idx] != arpCfg.src) { ++idx; }
            idx = (idx + diff + 4) % 4;
            uint8_t v = order[idx];
            if(v != arpCfg.src) { arpCfg.src = v; changed = true; }
          } else if(cursor == 1) {
            const uint8_t order[4] = { ARP_UP, ARP_DOWN, ARP_UPDOWN, ARP_RANDOM };
            int8_t idx = 0;
            while(idx < 4 && order[idx] != arpCfg.mode) { ++idx; }
            idx = (idx + diff + 4) % 4;
            uint8_t v = order[idx];
            if(v != arpCfg.mode) { arpCfg.mode = (ArpMode)v; changed = true; }
          } else if(cursor == 2) {
            int8_t v = (int8_t)arpCfg.octaves + diff;
            v = constrain(v, 1, 4);
            if(v != arpCfg.octaves) { arpCfg.octaves = v; changed = true; }
          } else if(cursor == 3) {
            int16_t v = (int16_t)arpCfg.gatePct + diff * 5;
            v = constrain(v, 10, 100);
            if(v != arpCfg.gatePct) { arpCfg.gatePct = v; changed = true; }
          } else if(cursor == 4) {
            uint8_t order[3] = { 24, 12, 6 };
            int8_t idx = 0;
            while(idx < 3 && order[idx] != arpCfg.divTicks) { ++idx; }
            idx = (idx + diff + 3) % 3;
            uint8_t v = order[idx];
            if(v != arpCfg.divTicks) { arpCfg.divTicks = v; changed = true; }
          }
        }

        /* ---------- FX : Echo ---------------------------------------- */
        else if(curList == fxEcho) {
          bool changed = false;
          /* cursor = 0 → Active on */
          if(cursor == 0) {
            const uint8_t order[4] = { 2, 0, 1, 3 };          /* OFF, DIN1, DIN2, DIN1+2 */
            int8_t idx = 0;
            while(idx < 4 && order[idx] != cfg.echo.src) { ++idx; }
            idx = (idx + diff + 4) % 4;
            uint8_t v = order[idx];
            if(v != cfg.echo.src) { cfg.echo.src = v; changed = true; }
          /* cursor = 1 → Time */
          } else if(cursor == 1) {
            const uint8_t order[5] = { 48, 24, 12, 6, 3 };    /* 1/2, 1/4, 1/8, 1/16, 1/32 */
            int8_t idx = 0;
            while(idx < 5 && order[idx] != cfg.echo.divTicks) { ++idx; }
            idx = (idx + diff + 5) % 5;
            uint8_t v = order[idx];
            if(v != cfg.echo.divTicks) { cfg.echo.divTicks = v; changed = true; }
          /* cursor = 2 → Repeats */
          } else if(cursor == 2) {
            int8_t v = (int8_t)cfg.echo.repeats + diff;
            v = constrain(v, 1, 8);
            if(v != cfg.echo.repeats) { cfg.echo.repeats = v; changed = true; }
          /* cursor = 3 → Vel Decay */
          } else if(cursor == 3) {
            int8_t v = (int8_t)cfg.echo.velDecay + diff * 5;
            v = constrain(v, 0, 50);
            if(v != cfg.echo.velDecay) { cfg.echo.velDecay = v; changed = true; }
          }
          if(changed) {
            echo_setCfg(cfg.echo);      /* aplica imediatamente */
            pageDirty = true;           /* força redesenho      */
          }
          continue;
        }

        /* ---------- FX : Scale Quantizer ---------------------------- */
        else if(curList == fxScale) {
            bool changed = false;
            if(cursor == Q_IDX_ACTIVE) {  /* cursor = 0 → Active on */
                const uint8_t order[4] = { 2, 0, 1, 3 };      /* OFF, DIN1, DIN2, DIN1+2 */
                int8_t idx = 0;
                while(idx < 4 && order[idx] != cfg.quant.active) ++idx;
                idx = (idx + diff + 4) % 4;
                uint8_t v = order[idx];
                if(v != cfg.quant.active) { cfg.quant.active = v; changed = true; }
            } else if(cursor == Q_IDX_ROOT) { /* cursor = 1 → Root note (0-11) */
                int8_t v = (int8_t)cfg.quant.root + diff;
                v = (v + 12) % 12;                          /* wrap-around */
                if(v != cfg.quant.root) { cfg.quant.root = v; changed = true; }
            } else if(cursor == Q_IDX_SCALE) {  /* cursor = 2 → Scale (0-7) */
                int8_t v = (int8_t)cfg.quant.scale + diff;
                v = (v + 8) % 8;
                if(v != cfg.quant.scale) { cfg.quant.scale = v; changed = true; }
            }
            if(changed) {
                quant_setCfg(cfg.quant);   /* aplica LUT imediatamente */
                pageDirty = true;
            }
            continue;
        }

        /* ---------- FX : Transpose ----------------------------------- */
        else if(curList == fxTranspose) {
          if(cursor == 0) {
            int16_t v = (int16_t)cfg.transp[0] + diff;
            v = constrain(v, -36, 36);
            if(v != cfg.transp[0]) { cfg.transp[0] = v; changed = true; }
          } else if(cursor == 1) {
            int16_t v = (int16_t)cfg.transp[1] + diff;
            v = constrain(v, -36, 36);
            if(v != cfg.transp[1]) { cfg.transp[1] = v; changed = true; }
          }
          if(changed) {
            pageDirty = true;
          }
          continue;
        }

        /* ------------- Input Routing --------------------------------- */
        else if(curList == menuInRouting) {
          /* cursor sempre 0 → USB Source */
          const uint8_t order[4] = { 2, 0, 1, 3 };      /* OFF, DIN1, DIN2, DIN1+2 */
          int8_t idx = 0;
          while(idx < 4 && order[idx] != cfg.usbInSel) { ++idx; }
          idx = (idx + diff + 4) % 4;
          uint8_t v = order[idx];
          if(v != cfg.usbInSel) {
            cfg.usbInSel = v;
            pageDirty = true;
          }
        }

        /* ------------- Main Routing ---------------------------------- */
        else if(curList == menuMain && cursor == 0) {
          changeRouting(diff);
          continue;
        }

        if(changed) {
          clk_config(cfgSrc, cfgBpm, cfgMul);
          arp_setCfg(arpCfg);
          cfg.clkSrc = cfgSrc;
          cfg.clkBpm = cfgBpm;
          cfg.clkMul = cfgMul;
          cfg.arp = arpCfg;
          pageDirty = true;
        }
        continue;
      } else {
        if(curPage == PAGE_MENU) {
          int32_t cur = (int32_t)cursor + diff;
          cur = constrain(cur, 0, (int32_t)curCnt - 1);
          if(cur != cursor) { cursor = cur; pageDirty = true; }
        }
      }
    }

    bool btn = digitalRead(ENC_BTN_PIN);
    uint32_t now = millis();

    if(btnOld && !btn && now - tDeb > DEB_MS) { tPress = now; tDeb = now; }
    if(!btnOld && btn && now - tDeb > DEB_MS) {
      uint32_t dur = now - tPress;
      if(dur >= LONG_MS) {  // Rotina: Clique LONGO
        backMenu();
        cfg_dirty = true;
      } else {  // Rotina: Clique CURTO
        if(uiMode == MODE_EDIT) {
          if(curList == menuMain && cursor == 0) {
            routingEnter();
          } else {
            uiMode = MODE_NAV;
            clk_config(cfgSrc, cfgBpm, cfgMul);
            clk_setOutMask(cfgMask);
            arp_setCfg(arpCfg);
            pageDirty = true;
          }
        } else {
          if(curPage == PAGE_HOME) { curPage = PAGE_MENU; pageDirty = true; }
          else if(curPage == PAGE_MENU) { enterMenu(); }
        }
      }
      tDeb = now;
    }
    btnOld = btn;

    bool need = pageDirty;
    if(curPage == PAGE_HOME && nv_dirty) { need = true; nv_dirty = false; }
    if(need && now - lastDraw >= DRAW_INTERVAL_MS) {
      drawPage();
      lastDraw = now;
    }
  }
}

/* ---------- Setup ---------- */
void setup() {
  enc_begin(ENC_A_PIN, ENC_B_PIN);
  pinMode(ENC_BTN_PIN, INPUT_PULLUP);
  pinMode(MIDI_IN1_PIN, INPUT);
  pinMode(MIDI_IN2_PIN, INPUT);
  pinMode(MIDI_OUT1_PIN, OUTPUT);
  pinMode(MIDI_OUT2_PIN, OUTPUT);
  pinMode(MIDI_OUT3_PIN, OUTPUT);
  pinMode(MIDI_OUT4_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  fifo_init(&txQ);
  cfg_load(); 
  arp_setCfg(arpCfg);
  /* ---------- OLED ---------- */
  Wire.setSDA(OLED_SDA);
  Wire.setSCL(OLED_SCL);
  Wire.begin();
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.drawBitmap(0, 0, LogoEMW, 128, 64, WHITE);
  display.display();

  TinyUSBDevice.setManufacturerDescriptor("EMW");
  TinyUSBDevice.setProductDescriptor("EMW - MIDI PROCESSOR");
  usbMIDI.begin();

  display.setTextColor(SSD1306_WHITE);
  delay(800);
  drawPage();
  pageDirty = false;

  /* ---------- MIDI PIO ---------- */
  midi_rx_init(pio0, sm_rx1, MIDI_IN1_PIN);
  midi_rx_init(pio0, sm_rx2, MIDI_IN2_PIN);
  midi_tx_init(pio0, sm_tx1, MIDI_OUT1_PIN);
  midi_tx_init(pio0, sm_tx2, MIDI_OUT2_PIN);
  midi_tx_init(pio1, sm_tx3, MIDI_OUT3_PIN);
  midi_tx_init(pio1, sm_tx4, MIDI_OUT4_PIN);
  /* ---------- configurações persistentes ---------- */
                               // lê da flash
  memcpy(routingMask, cfg.route, sizeof(cfg.route));
  memcpy(filtMask   , cfg.filt , sizeof(cfg.filt));
  cfgSrc = (ClkSrc)cfg.clkSrc;
  cfgBpm = cfg.clkBpm;
  cfgMul = (ClkMul)cfg.clkMul;
  cfgMask = cfg.clkMask; 
  arpCfg = cfg.arp;
  /* se nenhuma rota estava gravada, aplica o padrão DIN1/DIN2 → OUT1-4 */
  if (routingMask[0] == 0 && routingMask[1] == 0 && routingMask[2] == 0) {
    routingMask[0] = 0x0F;          // DIN1
    routingMask[1] = 0x0F;          // DIN2
    routingMask[2] = 0x00;          // USB (mantenha 0x0F se quiser habilitar)
    memcpy(cfg.route, routingMask, sizeof(cfg.route));
    cfg_dirty = true;               // grava ao final do setup()
  }
  /* ---------- aplica aos módulos em tempo-real ---------- */
  clk_config(cfgSrc, cfgBpm, cfgMul);
  clockout_setDivision(cfg.clkPulseDiv);
  clk_setOutMask(cfgMask);
  arp_setCfg(arpCfg);
  /* grava defaults na EEPROM apenas uma vez */
  if (cfg_dirty) {
    cfg_save();
    cfg_dirty = false;
  }
  clk_start();
  /* ---------- UI no Core 1 ---------- */
  multicore_launch_core1(core1_main);
  pulse_init();
}

/* ---------- Loop (Core 0) ---------- */
void loop(){
  uint8_t pkt[3],n;
  auto rx=[&](Parser &pr,uint sm,MmcBuf &mmc,uint8_t src){
    while(!pio_sm_is_rx_fifo_empty(pio0,sm)){
      uint8_t b=(uint8_t)(pio_sm_get(pio0,sm)>>24);
      if(mmc_stop(mmc,b)){ flush_notes(); arp_allNotesOff(); }
      g_src = src;
      if(parse(&pr,b,pkt,&n) && filt_allow(src,pkt,n)){
        for(uint8_t i=0;i<n;i++){
          uint8_t v=pkt[i];
          q_put(src,v);                      /* <<=== grava SRC + BYTE */
          route_sendByte(src,v);
        }
        if((pkt[0]&0xF0)==0x90 && pkt[2]){
          lastNote=pkt[1]; lastVel=pkt[2];
          nv_dirty=true;
          gpio_put(LED_PIN,1);
          ledOffAt = millis()+40;
        }
      }
    }
  };
  rx(pA,sm_rx1,mmcA,ROUTE_DIN1);
  rx(pB,sm_rx2,mmcB,ROUTE_DIN2);
  while(!q_empty()){
    uint8_t src,b;
    q_get(&src,&b);                          /* recupera SRC + BYTE   */
    if(b==0xF8){ arp_clockTick(); echo_clockTick(); clk_extTick(); }
    route_sendByte(src,b);                   /* SRC correto preservado */
  }
  clk_timeoutCheck();
  if(ledOffAt && (int32_t)(ledOffAt-millis())<=0){ gpio_put(LED_PIN,0); ledOffAt=0; }
  if(cfg_dirty) {
    cfg_save();
    cfg_dirty=false;
  }
}
