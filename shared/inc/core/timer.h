#ifndef INC_TIMER_H
#define INC_TIMER_H

#include "common-defines.h"

typedef struct timer_t {
    uint64_t wait_time;
    uint64_t target_time;
    bool auto_reset;
    bool has_elapsed;
} timer_t;

void TIMER_Init(timer_t* timer, uint64_t wait_time, bool auto_reset);
bool TIMER_Is_Elapsed(timer_t* timer);
void TIMER_Reset(timer_t* timer);

#endif
