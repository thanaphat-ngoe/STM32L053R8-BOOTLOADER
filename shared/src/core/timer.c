#include "core/timer.h"
#include "core/system.h"

void TIMER_Init(timer_t* timer, uint64_t wait_time, bool auto_reset) {
    timer->wait_time = wait_time;
    timer->auto_reset = auto_reset;
    timer->target_time = SYSTEM_Get_Ticks() + wait_time;
    timer->has_elapsed = false;
}

bool TIMER_Is_Elapsed(timer_t* timer) {
    uint64_t now = SYSTEM_Get_Ticks();
    bool has_elapsed = now >= timer->target_time;

    if (timer->has_elapsed) return false;

    if (has_elapsed) {
        if (timer->auto_reset) {
            uint64_t drift = now - timer->target_time;
            timer->target_time = (now + timer->wait_time) - drift;
        } else {
            timer->has_elapsed = true;
        }
    }
    
    return has_elapsed;
}
 
void TIMER_Reset(timer_t* timer) {
    TIMER_Init(timer, timer->wait_time, timer->auto_reset);
}
