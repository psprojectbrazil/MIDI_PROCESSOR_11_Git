#include "arp.h"
#include "config.h"
#include "clock_int.h"
#include "quantizer.h"
#include "pico/multicore.h"

#define CFG_MAGIC        0xABCD
#define CFG_VERSION      5
#define CFG_EEPROM_ADDR  0

extern void core1_main();
extern volatile bool cfg_dirty;
extern void clockout_setDivision(uint8_t); 

static uint16_t crc16_update(uint16_t crc, uint8_t data) {
  crc ^= data;
  for(uint8_t i = 0; i < 8; i++) {
    if(crc & 1) crc = (crc >> 1) ^ 0xA001;
    else        crc >>= 1;
  }
  return crc;
}

static uint16_t calc_crc(const Config &c) {
  const uint8_t *p = (const uint8_t *)&c;
  size_t len = sizeof(Config) - sizeof(uint16_t);
  uint16_t crc = 0xFFFF;
  for(size_t i = 0; i < len; i++) crc = crc16_update(crc, p[i]);
  return crc;
}

Config cfg;

ClockCfg clk = {
  .bpm       = 120,
  .mul       = 0,
  .pulseDiv  = 6,
  .src       = SRC_INT
};

void cfg_factoryDefaults(){
  memset(&cfg,0,sizeof(cfg));
  cfg.magic         = CFG_MAGIC;
  cfg.version       = CFG_VERSION;
  cfg.route[0]      = 0x0F;
  cfg.route[1]      = 0x0F;
  cfg.route[2]      = 0x00;
  cfg.clkSrc        = 0;
  cfg.clkBpm        = 120;
  cfg.clkMul        = 0;
  cfg.clkPulseDiv   = 6;
  cfg.clkMask       = 0x0F;
  cfg.arp.mode      = ARP_OFF;
  cfg.arp.octaves   = 1;
  cfg.arp.gatePct   = 50;
  cfg.arp.divTicks  = 6;
  cfg.arp.src       = 2;
  cfg.echo.divTicks = 6;
  cfg.echo.repeats  = 3;
  cfg.echo.velDecay = 20;
  cfg.echo.src      = 2;
  cfg.quant.active  = 0;
  cfg.quant.root    = 0;
  cfg.quant.scale   = 0;
  cfg.transp[0]     = 0;
  cfg.transp[1]     = 0;
  cfg.usbInSel      = 2;
  cfg.crc           = calc_crc(cfg);
}

void cfg_load(){
  EEPROM.begin(sizeof(Config));
  EEPROM.get(CFG_EEPROM_ADDR,cfg);
  bool ok = (cfg.magic == CFG_MAGIC) && (cfg.version <= CFG_VERSION);
  if(ok){
    uint16_t saved = cfg.crc;
    cfg.crc = 0;
    ok = (saved == calc_crc(cfg));
    cfg.crc = saved;
  }
  if(!ok){
    cfg_factoryDefaults();
    cfg_save();
    quant_setCfg(cfg.quant);
    return;
  }
  if(cfg.version < 5){
    cfg.quant.active = 0;
    cfg.quant.root   = 0;
    cfg.quant.scale  = 0;
    cfg.version      = CFG_VERSION;
    cfg_dirty        = true;
  }
  clk.bpm       = cfg.clkBpm;
  clk.mul       = cfg.clkMul;
  clk.pulseDiv  = cfg.clkPulseDiv;
  clk.src       = (ClkSrc)cfg.clkSrc;
  clockout_setDivision(clk.pulseDiv);
  quant_setCfg(cfg.quant);
}

void cfg_save(){
  multicore_reset_core1();
  cfg.magic = CFG_MAGIC;
  cfg.version = CFG_VERSION;
  cfg.crc = calc_crc(cfg);
  EEPROM.put(CFG_EEPROM_ADDR, cfg);
  EEPROM.commit();
  multicore_launch_core1(core1_main);
}



