#include "core/uart.h"

#include "libopencm3/stm32/rcc.h"
#include "libopencm3/stm32/usart.h"
#include "libopencm3/cm3/nvic.h"

#define BAUD_RATE (115200)

void uart_setup(void) {
    rcc_periph_clock_enable(RCC_USART2);
    usart_set_flow_control(USART2, USART_FLOWCONTROL_NONE);
    usart_set_databits(USART2, 8);
    usart_set_baudrate(USART2, BAUD_RATE);
    usart_set_parity(USART2, 0);
    usart_set_stopbits(USART2, 1);
    usart_set_mode(USART2, USART_MODE_TX_RX);
    usart_enable_rx_interrupt(USART2);
    nvic_enable_irq(NVIC_USART2_IRQ);
    usart_enable(USART2);
}

void uart_write(uint8_t* data, const uint32_t length) {

}

void uart_write_byte(uint8_t data) {

}

uint32_t uart_read(uint8_t* data, const uint32_t length) {

}

uint8_t uart_read_byte(void) {

}

bool uart_data_available(void) {

}
