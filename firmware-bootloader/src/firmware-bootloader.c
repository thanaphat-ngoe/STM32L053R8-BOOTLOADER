#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/memorymap.h>
#include <libopencm3/cm3/vector.h>

#include "core/system.h"
#include "core/uart.h"
#include "core/timer.h"
#include "transport-layer.h"
#include "bl-flash.h"

#define BOOTLOADER_SIZE (0x4000U) // 16 KByte (16384 Byte)
#define MAIN_APPLICATION_START_ADDRESS (FLASH_BASE + BOOTLOADER_SIZE) // 0x08000000 + 0x4000 (0x08004000)

#define FLASH_SIZE (0x10000U) // 64 Kbyte (65536 Byte)

#define MAX_FIRMWARE_SIZE (FLASH_SIZE - BOOTLOADER_SIZE) // 48 Kbyte (49152 Byte)

#define UART_PORT (GPIOA)
#define TX_PIN    (GPIO2)
#define RX_PIN    (GPIO3)

#define DEVICE_ID (0x01)

#define SYNC_SEQ_0 (0x01)
#define SYNC_SEQ_1 (0x02)
#define SYNC_SEQ_2 (0x03)
#define SYNC_SEQ_3 (0x04)

#define DEFAULT_TIMEOUT (5000)

typedef enum bl_al_state_t {
    BL_AL_STATE_Sync,
    BL_AL_STATE_WaitForUpdateReq,
    BL_AL_STATE_DeviceIDReq,
    BL_AL_STATE_DeviceIDRes,
    BL_AL_STATE_FirmwareLengthReq,
    BL_AL_STATE_FirmwareLengthRes,
    BL_AL_STATE_EraseApplication,
    BL_AL_STATE_ReceiveFirmware,
    BL_AL_STATE_Done
} bl_al_state_t;

static bl_al_state_t state = BL_AL_STATE_Sync;
static uint32_t firmware_size = 0;
static uint32_t bytes_written = 0;
static uint8_t sync_seq[4] = {0};
static tl_segment_t temp_segment;

static timer_t timer;

static void GPIO_Init(void) {
    rcc_periph_clock_enable(RCC_GPIOA);
    gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, TX_PIN | RX_PIN);
    gpio_set_af(GPIOA, GPIO_AF4, TX_PIN | RX_PIN);
}

static void GPIO_Init_Reset(void) {
    gpio_mode_setup(GPIOA, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, TX_PIN | RX_PIN);
    rcc_periph_clock_disable(RCC_GPIOA);
}

static void Jump_To_Main_Application(void) {
    vector_table_t* main_vector_table = (vector_table_t*)(MAIN_APPLICATION_START_ADDRESS);
    main_vector_table->reset();
}

static bool IS_MESSAGE_Device_ID(const tl_segment_t* segment) {
    if (segment->segment_data_size != 2) {
        return false;
    }

    if (segment->segment_type == SEGMENT_ACK || segment->segment_type == SEGMENT_RETX || segment->segment_type != 0) {
        return false;
    }

    if (segment->data[0] != BL_AL_MESSAGE_DEVICE_ID_RES) {
        return false;
    }

    for (uint8_t i = 2; i < SEGMENT_DATA_SIZE; i++) {
        if (segment->data[i] != 0xff) {
            return false;
        }
    }

    return true;
}

static bool IS_MESSAGE_Firmware_Size(const tl_segment_t* segment) {
    if (segment->segment_data_size != 5) {
        return false;
    }

    if (segment->segment_type == SEGMENT_ACK || segment->segment_type == SEGMENT_RETX || segment->segment_type != 0) {
        return false;
    }

    if (segment->data[0] != BL_AL_MESSAGE_FW_LENGTH_RES) {
        return false;
    }

    for (uint8_t i = 5; i < SEGMENT_DATA_SIZE; i++) {
        if (segment->data[i] != 0xff) {
            return false;
        }
    }

    return true;
}

int main(void) {
    SYSTEM_Init();
    GPIO_Init();
    UART_Init();
    TL_Init();
    TIMER_Init(&timer, DEFAULT_TIMEOUT, false);

    while (state != BL_AL_STATE_Done) {
        if (state == BL_AL_STATE_Sync) {
            if (uart_data_available()) {
                sync_seq[0] = sync_seq[1];
                sync_seq[1] = sync_seq[2];
                sync_seq[2] = sync_seq[3];
                sync_seq[3] = uart_read_byte();

                bool is_match = sync_seq[0] == SYNC_SEQ_0;
                is_match = is_match && (sync_seq[1] == SYNC_SEQ_1);
                is_match = is_match && (sync_seq[2] == SYNC_SEQ_2);
                is_match = is_match && (sync_seq[3] == SYNC_SEQ_3);
            
                if (is_match) {
                    tl_create_single_byte_segment(&temp_segment, BL_AL_MESSAGE_SEQ_OBSERVED);
                    tl_write(&temp_segment);
                    state = BL_AL_STATE_WaitForUpdateReq;
                } else {
                    if (TIMER_Is_Elapsed(&timer)) {
                        state = BL_AL_STATE_Done;
                        continue;
                    } else {
                        continue;
                    }
                }
            } else {
                if (TIMER_Is_Elapsed(&timer)) {
                    state = BL_AL_STATE_Done;
                    continue;
                } else {
                    continue;
                }
            }
        }

        TL_Update();

        switch (state) {
            case BL_AL_STATE_WaitForUpdateReq: {
                if (tl_segment_available()) {
                    tl_read(&temp_segment);

                    if (tl_is_single_byte_segment(&temp_segment, BL_AL_MESSAGE_FW_UPDATE_REQ)) {
                        tl_create_single_byte_segment(&temp_segment,  BL_AL_MESSAGE_FW_UPDATE_RES);
                        tl_write(&temp_segment);
                        state = BL_AL_STATE_DeviceIDReq;
                    } else {
                        continue;
                    }
                } else {
                    continue;
                }
            } break;
            
            case BL_AL_STATE_DeviceIDReq: {
                tl_create_single_byte_segment(&temp_segment, BL_AL_MESSAGE_DEVICE_ID_REQ);
                tl_write(&temp_segment);
                state = BL_AL_STATE_DeviceIDRes;
            } break;
            
            case BL_AL_STATE_DeviceIDRes: {
                if (tl_segment_available()) {
                    tl_read(&temp_segment);

                    if (IS_MESSAGE_Device_ID(&temp_segment) && temp_segment.data[1] == DEVICE_ID) {
                        state = BL_AL_STATE_FirmwareLengthReq;
                    } else {
                        continue;
                    }
                } else {
                    continue;
                }
            } break;
            
            case BL_AL_STATE_FirmwareLengthReq: {
                tl_create_single_byte_segment(&temp_segment, BL_AL_MESSAGE_FW_LENGTH_REQ);
                tl_write(&temp_segment);
                state = BL_AL_STATE_FirmwareLengthRes;
            } break;
            
            case BL_AL_STATE_FirmwareLengthRes: {
                if (tl_segment_available()) {
                    tl_read(&temp_segment);
                    firmware_size = (
                        (temp_segment.data[1])       |
                        (temp_segment.data[2] << 8)  |
                        (temp_segment.data[3] << 16) |
                        (temp_segment.data[4] << 24) 
                    );

                    if (IS_MESSAGE_Firmware_Size(&temp_segment) && (firmware_size <= MAX_FIRMWARE_SIZE) && (firmware_size % 4 == 0)) {
                        state = BL_AL_STATE_EraseApplication;
                    } else {
                        continue;
                    }
                } else {
                    continue;
                }
            } break;
            
            case BL_AL_STATE_EraseApplication: {
                BL_FLASH_ERASE_Main_Application();
                tl_create_single_byte_segment(&temp_segment, BL_AL_MESSAGE_READY_FOR_DATA);
                tl_write(&temp_segment);
                state = BL_AL_STATE_ReceiveFirmware; 
            } break;
            
            case BL_AL_STATE_ReceiveFirmware: {
                if (tl_segment_available()) {
                    tl_read(&temp_segment);
                    
                    for (uint8_t i = 0; i < temp_segment.segment_data_size; i = i + 4) {
                        uint32_t firmware_data = (
                            (temp_segment.data[i])           |
                            (temp_segment.data[i + 1] << 8)  |
                            (temp_segment.data[i + 2] << 16) |
                            (temp_segment.data[i + 3] << 24) 
                        );
                        HAL_FLASH_Unlock();
                        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, MAIN_APPLICATION_START_ADDRESS + bytes_written, firmware_data);
                        HAL_FLASH_Lock();
                        bytes_written += 4;
                    }
                    
                    if (bytes_written >= MAX_FIRMWARE_SIZE) {
                        tl_create_single_byte_segment(&temp_segment, BL_AL_MESSAGE_UPDATE_SUCCESSFUL);
                        tl_write(&temp_segment);
                        state = BL_AL_STATE_Done;
                    } else {
                        tl_create_single_byte_segment(&temp_segment, BL_AL_MESSAGE_READY_FOR_DATA);
                        tl_write(&temp_segment);
                    }
                } else {
                    continue;
                }
            } break;

            default: {
                state = BL_AL_STATE_Sync;
            }
        }
    }

    // TODO: Reset all system before passing control over to the main application
    SYSTEM_Delay(500);
    UART_Init_Reset();
    GPIO_Init_Reset();
    SYSTEM_Init_Reset(); 
    Jump_To_Main_Application();

    // Must never return;
    return 0; 
}
