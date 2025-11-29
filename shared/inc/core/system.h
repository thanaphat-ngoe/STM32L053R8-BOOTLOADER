#ifndef INC_SYSTEM_H
#define INC_SYSTEM_H

#include "common-defines.h"

#define CPU_FREQ (32000000)
#define SYSTICK_FREQ (1000)

void SYSTEM_Init(void);
void SYSTEM_Init_Reset(void);
uint64_t SYSTEM_Get_Ticks(void);
void SYSTEM_Delay(uint64_t millisecond);

#endif
