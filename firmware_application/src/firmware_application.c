#include "core/system.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/scb.h>

#define BOOTLOADER_SIZE (0x8000U) // 32768 Byte or 32 KByte

static void vector_setup(void) {
    SCB_VTOR = BOOTLOADER_SIZE;
}

static void gpio_setup(void) {
    rcc_periph_clock_enable(RCC_GPIOA);
    gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO5);
}

int main(void) {
    vector_setup();
    system_setup();
    gpio_setup();
    
    uint32_t start_time = system_get_ticks();

    while (1) {
        if (system_get_ticks() - start_time >= SYSTICK_FREQ) {
            gpio_toggle(GPIOA, GPIO5);
            start_time = system_get_ticks();
        }
    }
    
    return 0;
}
