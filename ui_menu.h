#pragma once
#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include "clock_int.h" 
#include "arp.h" 

extern Adafruit_SSD1306 display;

struct MenuItem { const char *label; const MenuItem *child; uint8_t cnt; };

extern const MenuItem menuMain[];

extern const MenuItem *curList;
extern uint8_t curCnt;
extern uint8_t cursor;
extern uint8_t depth;

extern volatile uint8_t lastNote;
extern volatile uint8_t lastVel;
extern volatile bool pageDirty;

enum Page : uint8_t { PAGE_HOME, PAGE_MENU };
extern volatile Page curPage;

enum UiMode : uint8_t { MODE_NAV, MODE_EDIT };
extern volatile UiMode uiMode;

/* acesso a partir de core1_main() */
extern const MenuItem menuClock[];
extern const MenuItem fxArp[];
extern const MenuItem fxEcho[];
extern const MenuItem fxScale[];
extern const MenuItem fxTranspose[];
extern const MenuItem menuInRouting[];

extern ClkSrc cfgSrc;
extern uint16_t cfgBpm;
extern ClkMul cfgMul;
extern uint8_t cfgMask;
extern ArpCfg arpCfg; 

void drawRouting();
void changeRouting(int8_t delta);
void routingEnter();
void drawPage();
void enterMenu();
void backMenu();

constexpr uint8_t CLK_IDX_SS    = 0;
constexpr uint8_t CLK_IDX_SRC   = 1;
constexpr uint8_t CLK_IDX_BPM   = 2;
constexpr uint8_t CLK_IDX_MULT  = 3;
constexpr uint8_t CLK_IDX_SPACE = 4;   /* linha em branco    */
constexpr uint8_t CLK_IDX_PULSE = 5;   /* novo “Pulse Clock” */

constexpr uint8_t ARP_IDX_MODE = 0;
constexpr uint8_t ARP_IDX_OCT  = 1;
constexpr uint8_t ARP_IDX_GATE = 2;
constexpr uint8_t ARP_IDX_SYNC = 3;

constexpr uint8_t Q_IDX_ACTIVE = 0;
constexpr uint8_t Q_IDX_ROOT   = 1;
constexpr uint8_t Q_IDX_SCALE  = 2;

