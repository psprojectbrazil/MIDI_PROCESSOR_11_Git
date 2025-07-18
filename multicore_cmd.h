#pragma once
#include <stdint.h>

enum CmdTag : uint8_t {
  CMD_CLK_UPDATE = 1,   /* data16 = bpm, data32 = mul (-2..2)  */
  CMD_CLK_START  = 2,
  CMD_CLK_STOP   = 3,
  CMD_ARP_CFG    = 4    /* data32 = ptr p/ struct ArpCfg       */
};

struct CmdMsg {
  uint8_t  tag;
  uint8_t  pad;
  uint16_t data16;
  uint32_t data32;
};
