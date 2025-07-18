#ifndef ROUTING_H
#define ROUTING_H

#include <stdint.h>

enum RouteSrc { ROUTE_DIN1 = 0, ROUTE_DIN2 = 1, ROUTE_USB = 2 };

extern uint8_t routingMask[3];
extern const uint8_t sm_tx1, sm_tx2, sm_tx3, sm_tx4;

uint8_t route_getMask(uint8_t src);
void route_toggle(uint8_t src, uint8_t out);
void route_sendByte(uint8_t src,uint8_t b);

#endif
