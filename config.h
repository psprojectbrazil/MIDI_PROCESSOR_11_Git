#pragma once
#include <Arduino.h>
#include <EEPROM.h>
#include <string.h>
#include "midi_parser.h"
#include "arp.h"

struct EchoCfg{
  uint8_t divTicks;
  uint8_t repeats;
  uint8_t velDecay;
  uint8_t src;
};

struct QuantCfg {
  uint8_t active;    /* 0-OFF 1-DIN1 2-DIN2 3-DIN1+2 */
  uint8_t root;      /* 0-11  (C=0, C#=1, … B=11)   */
  uint8_t scale;     /* 0-Chrom 1-Major … 7-Blues   */
};

struct Config{
  uint16_t magic;
  uint8_t version;
  uint8_t route[3];
  uint8_t filt[2];
  uint8_t clkSrc;
  uint16_t clkBpm;
  int8_t clkMul;
  uint8_t clkPulseDiv;
  uint8_t clkMask;
  ArpCfg arp;
  EchoCfg echo;
  QuantCfg quant; 
  int8_t transp[2];
  uint8_t  usbInSel;
  uint16_t crc;
};

typedef struct {
  uint16_t bpm;
  int8_t   mul;
  uint8_t  pulseDiv;   /* 3, 6, 12 ou 24 – padrão 6 */
  uint8_t  src;
} ClockCfg;

extern Config cfg;

void cfg_factoryDefaults();
void cfg_load();
void cfg_save();

