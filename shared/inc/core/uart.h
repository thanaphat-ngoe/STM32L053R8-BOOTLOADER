#ifndef INC_UART_H
#define INC_UART_H

#include "common-defines.h"

void UART_Init(void);
void uart_write(uint8_t* data, const uint32_t length);
void uart_write_byte(uint8_t data);
uint32_t uart_read(uint8_t* data, const uint32_t length);
uint8_t uart_read_byte(void);
bool uart_data_available(void);
void UART_Init_Reset(void);

#endif
