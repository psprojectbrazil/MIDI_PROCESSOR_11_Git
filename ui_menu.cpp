#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include "ui_menu.h"
#include "clock_int.h"
#include "routing.h"
#include "filters.h"
#include "config.h"

const MenuItem menuRouting[] = { { "", nullptr, 0 } };
const MenuItem menuInRouting[] = { { "USB Input", nullptr, 0 } };

/* ----------- filtros ----------- */
const MenuItem filtTypes[] = {
  { "Aftertouch",     nullptr, 0 },   /* índice 0 */
  { "Program/Bank",   nullptr, 0 },   /* índice 1 */
  { "Mod-Wheel",      nullptr, 0 },   /* índice 2 */
  { "Sustain Pedal",  nullptr, 0 },   /* índice 3 */
  { "Vol/Expression", nullptr, 0 },   /* índice 4 */
  { "Other CCs",      nullptr, 0 },   /* índice 5 */
};

const MenuItem menuFilters[] = {
  { "DIN 1", filtTypes, 6 },
  { "DIN 2", filtTypes, 6 },
};

/* clock subtree */
const MenuItem clkStartStop[] = {
  { "OUT1", nullptr, 0 },
  { "OUT2", nullptr, 0 },
  { "OUT3", nullptr, 0 },
  { "OUT4", nullptr, 0 },
};

const MenuItem menuClock[] = {
  { "CLOCK ROUTE", clkStartStop, 4 },   /* idx 0 */
  { "Source",      nullptr,      0 },   /* idx 1 */
  { "BPM",         nullptr,      0 },   /* idx 2 */
  { "Div/Mult",    nullptr,      0 },   /* idx 3 */
  { "",            nullptr,      0 },   /* idx 4 (linha vazia) */
  { "Pulse Clock", nullptr,      0 },   /* idx 5 – novo item    */
};

const MenuItem fxArp[] = {
  { "Active on", nullptr, 0 },
  { "Mode",      nullptr, 0 },
  { "Octaves",   nullptr, 0 },
  { "Gate%",     nullptr, 0 },
  { "Sync",      nullptr, 0 },
};

const MenuItem fxEcho[] = {
  { "Active on", nullptr, 0 },
  { "Time",      nullptr, 0 },
  { "Repeat",    nullptr, 0 },
  { "VelDec%",   nullptr, 0 },
};

const MenuItem fxTranspose[] = {
  { "DIN 1", nullptr, 0 },
  { "DIN 2", nullptr, 0 },
};

const MenuItem fxScale[] = {
  { "Active on", nullptr, 0 },   /* idx 0 */
  { "Root",      nullptr, 0 },   /* idx 1 */
  { "Scale",     nullptr, 0 },   /* idx 2 */
};

const MenuItem fxVelCurve[] = { { "Shape", nullptr, 0 } };

const MenuItem fxHumanize[] = {
  { "Time ±", nullptr, 0 },
  { "Vel ±",  nullptr, 0 },
};

const MenuItem menuFX[] = {
  { "ARPEGGIATOR",     fxArp,       5 },
  { "ECHO",            fxEcho,      4 },
  { "TRANSPOSE",       fxTranspose, 2 },
  { "SCALE QUANTIZER", fxScale,     3 },
};

const MenuItem menuDiag[] = {
  { "FIFO Usage %", nullptr, 0 },
  { "Active Notes", nullptr, 0 },
  { "Latency (ms)", nullptr, 0 },
};

const MenuItem menuMain[] = {
  { "OUTPUT ROUTING", menuRouting,   1 },  /* idx 0 – grade 3×4    */
  { "INPUT ROUTING",  menuInRouting, 1 },  /* idx 1 – input routng */
  { "FILTERS",        menuFilters,   2 },  /* idx 1 */
  { "EFFECTS",        menuFX,        4 },  /* idx 2 */
  { "CLOCK",          menuClock,     6 },  /* idx 3 */
};

/* ----------- estado de navegacao ----------- */
const MenuItem *curList = menuMain;
uint8_t curCnt = sizeof(menuMain) / sizeof(MenuItem);
uint8_t cursor = 0;
const MenuItem *stackList[8];
uint8_t stackCnt[8];
uint8_t stackCursor[8];
uint8_t depth = 0;

volatile Page curPage = PAGE_HOME;
volatile bool pageDirty = true;
volatile UiMode uiMode = MODE_NAV;

/* ----------- variaveis Clock ----------- */
ClkSrc cfgSrc = SRC_INT;
uint16_t cfgBpm = 120;
ClkMul cfgMul = MUL_X1;
uint8_t cfgMask = 0x0F;     /* OUT1-4 habilitados */

static const char *arpModeLbl[] = { "Off","Up","Down","UpDn","Rand" };
static const char *srcLbl[] = { "INT","DIN1","DIN2","USB" };
static const char *mulLbl[] = { "/4","/2","x1","x2","x4" };

/* ---- Scale Quantizer ---- */
static const char *qActiveLbl[] = { "OFF", "DIN 1", "DIN 2", "DIN1+2" };
static const char *qRootLbl[]   = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
static const char *qScaleLbl[]  = { "Chrom", "Major", "NatMin", "HarMin", "MelMin", "PentMj", "PentMn", "Blues" };

static uint8_t routePos = 0;
static uint8_t curFiltSrc = 0;     /* 0 = DIN1  1 = DIN2 */

extern volatile bool cfg_dirty;

/* ----------- externs vindos do sketch ----------- */
extern Adafruit_SSD1306 display;
extern volatile uint8_t lastNote, lastVel;

/* ----------- desenhar paginas ----------- */
void drawHome() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0,0);
  display.print("EMW MIDI PROCESSOR");
  display.drawLine(0,12,128,12,WHITE);
  display.setCursor(0,16);
  display.print("NOTE:");
  display.print(lastNote);
  display.setCursor(68,16);
  display.print("VEL:");
  display.print(lastVel);
  display.drawLine(0,26,128,26,WHITE);

  display.setCursor(0,34);
  display.print("CLOCK:");
  display.print(srcLbl[cfgSrc]);
  if(cfgSrc == 0) {
    display.print("  -  BPM:");
    display.print(cfgBpm);
  }

  display.setCursor(0,45);
  display.print("ARPEG:");
  if(arpCfg.src == 2) {
    display.print("OFF");
  } else {
    display.print("ON   -  ");
    if(arpCfg.src == 0) {
      display.print("DIN 1");
    } else if(arpCfg.src == 1) {
      display.print("DIN 2");
    } else {
      display.print("DIN 1+2");
    }
  }

  display.setCursor(0,56);
  display.print("ECHO:");
  if(cfg.echo.src == 2) {
    display.print("OFF");
  } else {
    display.print("ON - ");
    if(cfg.echo.src == 0) {
      display.print("DIN 1");
    } else if(cfg.echo.src == 1) {
      display.print("DIN 2");
    } else {
      display.print("DIN 1+2");
    }
  }
  display.display();
}

void drawRouting() {
  display.clearDisplay();
  display.setCursor(0,0);
  display.print("OUTPUT ROUTING");
  const char *rowLbl[3] = { " DIN1 "," DIN2 "," USB  " };
  uint8_t curRow = routePos >> 2;
  for(uint8_t r = 0; r < 3; r++) {
    if(r == curRow) display.setTextColor(BLACK, WHITE);
    else display.setTextColor(WHITE);
    display.setCursor(0,16 + r*12);
    display.print(rowLbl[r]);
    display.setTextColor(WHITE);
    for(uint8_t c = 0; c < 4; c++) {
      uint8_t x = 46 + c*18;
      uint8_t y = 16 + r*12;
      bool on = routingMask[r] & (1u << c);
      if(routePos == r*4 + c) display.setTextColor(BLACK, WHITE);
      else display.setTextColor(WHITE);
      display.setCursor(x,y);
      display.print(on ? '*' : '-');
    }
  }
  display.setTextColor(WHITE);
  display.display();
}

void drawMenu(){
  if(curList == menuMain && cursor == 0 && uiMode == MODE_EDIT){
    drawRouting();
    return;
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0,0);
  if(uiMode == MODE_EDIT) display.print("EDIT MODE");
  else if(curList == menuMain) display.print("MAIN MENU");
  else if(curList == menuClock) display.print("CLOCK MENU");
  else if(curList == menuInRouting) display.print("INPUT ROUTING");
  else if(curList == menuFilters) display.print("FILTERS MENU");
  else if(curList == menuFX) display.print("EFFECTS MENU");
  else if(curList == fxArp) display.print("ARPEGGIATOR");
  else if(curList == fxEcho) display.print("ECHO");
  else if(curList == fxTranspose) display.print("TRANSPOSE");
  else if(curList == filtTypes) display.print("FILTERS ON / OFF");
  else if(curList == clkStartStop) display.print("CLOCK ROUTE");
  else display.print("MENU");
  for(uint8_t i = 0; i < curCnt; ++i){
    display.setCursor(0,12 + i*9);
    display.print(i == cursor ? '>' : ' ');
    display.print(' ');
    if(curList == filtTypes){
      bool on = filtMask[curFiltSrc] & (1u << i);
      display.print('[');
      display.print(on ? '*' : '-');
      display.print("] ");
    }
    display.print(curList[i].label);
    display.setCursor(84,12 + i*9);
    if(curList == menuClock){
      if(i == CLK_IDX_SRC ) display.print(srcLbl[cfgSrc]);
      if(i == CLK_IDX_BPM ) display.print(cfgBpm);
      if(i == CLK_IDX_MULT) display.print(mulLbl[cfgMul + 2]);
      if(i == CLK_IDX_PULSE){
        const char *lbl = (cfg.clkPulseDiv == 3) ? "/3"
                        : (cfg.clkPulseDiv == 6) ? "/6"
                        : (cfg.clkPulseDiv == 12) ? "/12" : "/24";
        display.print(lbl);
      }

    }else if(curList == clkStartStop){
      display.print((cfgMask & (1 << i)) ? " ON" : "OFF");

    }else if(curList == fxArp){
      if(i == 0){
        if(arpCfg.src == 2) display.print("OFF");
        else if(arpCfg.src == 0) display.print("DIN 1");
        else if(arpCfg.src == 1) display.print("DIN 2");
        else display.print("DIN 1+2");
      }
      if(i == 1) display.print(arpModeLbl[arpCfg.mode]);
      if(i == 2) display.print(arpCfg.octaves);
      if(i == 3) display.print(arpCfg.gatePct);
      if(i == 4) display.print((arpCfg.divTicks == 24) ? "1/4"
                          : (arpCfg.divTicks == 12) ? "1/8"
                          : (arpCfg.divTicks == 6)  ? "1/16" : "1/32");

    }else if(curList == fxEcho){
      if(i == 0){
        if(cfg.echo.src == 2) display.print("OFF");
        else if(cfg.echo.src == 0) display.print("DIN 1");
        else if(cfg.echo.src == 1) display.print("DIN 2");
        else display.print("DIN 1+2");
      }
      if(i == 1) display.print((cfg.echo.divTicks == 48) ? "1/2"
                        : (cfg.echo.divTicks == 24) ? "1/4"
                        : (cfg.echo.divTicks == 12) ? "1/8"
                        : (cfg.echo.divTicks == 6)  ? "1/16" : "1/32");
      if(i == 2) display.print(cfg.echo.repeats);
      if(i == 3) display.print(cfg.echo.velDecay);

    }else if(curList == fxScale){
      if(i == 0) display.print(qActiveLbl[cfg.quant.active]);
      if(i == 1) display.print(qRootLbl  [cfg.quant.root  ]);
      if(i == 2) display.print(qScaleLbl [cfg.quant.scale ]);

    }else if(curList == fxTranspose){
      display.print(cfg.transp[i]);
    }else if(curList == menuInRouting){
      if(cfg.usbInSel == 2) display.print("OFF");
      else if(cfg.usbInSel == 0) display.print("DIN 1");
      else if(cfg.usbInSel == 1) display.print("DIN 2");
      else display.print("DIN 1+2");
    }
  }
  display.display();
}

void drawPage() {
  switch(curPage) {
    case PAGE_HOME: drawHome(); break;
    case PAGE_MENU: drawMenu(); break;
  }
  pageDirty = false;
}

/* ----------- navegaçao ----------- */
void enterMenu() {
  if(curList == menuMain && cursor == 0) {
    routePos = 0;
    uiMode = MODE_EDIT;
    pageDirty = true;
    return;
  }
  const MenuItem *child = curList[cursor].child;
  uint8_t cnt = curList[cursor].cnt;
  if(child && cnt){
    stackList [depth] = curList;
    stackCnt  [depth] = curCnt;
    stackCursor[depth]= cursor;
    ++depth;
    if(child == filtTypes) curFiltSrc = cursor;
    curList = child;
    curCnt  = cnt;
    cursor  = 0;
    pageDirty = true;
    return;
  }
  if(curList == menuClock && (cursor == CLK_IDX_SRC || cursor == CLK_IDX_BPM || cursor == CLK_IDX_MULT || cursor == CLK_IDX_PULSE)) {
    uiMode = MODE_EDIT;
    pageDirty = true;
    return;
  }
  if(curList == fxArp || curList == fxEcho || curList == fxTranspose ||  curList == fxScale || curList == menuInRouting) {
    uiMode = MODE_EDIT;
    pageDirty = true;
    return;
  }
  if(curList == clkStartStop) {
    cfgMask ^= 1 << cursor;
    clk_setOutMask(cfgMask);
    cfg.clkMask = cfgMask;
    cfg_dirty = true;
    pageDirty = true;
    return;
  }
  if(curList == filtTypes) {
    filtMask[curFiltSrc] ^= (1u << cursor);
    memcpy(cfg.filt, filtMask, sizeof(cfg.filt));
    cfg_dirty = true;
    pageDirty = true;
    return;
  }
}

void backMenu() {
  if(curList == menuMain && cursor == 0 && uiMode == MODE_EDIT) {
    uiMode   = MODE_NAV;
    pageDirty= true;
    return;
  }
  if(depth) {
    --depth;
    curList = stackList [depth];
    curCnt = stackCnt  [depth];
    cursor = stackCursor[depth];
  } else {
    curPage  = PAGE_HOME;
  }
  uiMode   = MODE_NAV;
  pageDirty= true;
}

void changeRouting(int8_t delta) {
  routePos = (routePos + delta + 12) % 12;
  pageDirty = true;
}

void routingEnter() {
  uint8_t r = routePos / 4;
  uint8_t c = routePos % 4;
  route_toggle(r,c);
  memcpy(cfg.route, routingMask, sizeof(cfg.route));
  cfg_dirty = true;
  pageDirty = true;
}
