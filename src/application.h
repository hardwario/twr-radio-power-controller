#ifndef _APPLICATION_H
#define _APPLICATION_H

#ifndef LED_STRIP_COUNT
#define LED_STRIP_COUNT 144
#endif

#ifndef LED_STRIP_TYPE
#define LED_STRIP_TYPE 4
#endif

#ifndef LED_STRIP_SWAP_RG
#define LED_STRIP_SWAP_RG 0
#endif

#include <bcl.h>
#include <twr.h>

typedef struct
{
    uint8_t channel;
    float value;
    twr_tick_t next_pub;

} event_param_t;

#endif // _APPLICATION_H
