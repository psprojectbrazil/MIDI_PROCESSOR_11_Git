#pragma once
#include <Arduino.h>

enum ArpMode : uint8_t { ARP_OFF, ARP_UP, ARP_DOWN, ARP_UPDOWN, ARP_RANDOM };

struct ArpCfg {
  uint8_t  mode;
  uint8_t  octaves;
  uint8_t  gatePct;
  uint8_t  divTicks;
  uint8_t  src;
};

extern ArpCfg arpCfg;            /* config atual */

void arp_setCfg(const ArpCfg &c);
void arp_clockTick();            /* chamar a cada 0xF8 */
void arp_allNotesOff();          /* opcional (Stop MMC) */
bool arp_isActive();

