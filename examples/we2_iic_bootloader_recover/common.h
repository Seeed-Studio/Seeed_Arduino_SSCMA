#ifndef __COMMON__
#define __COMMON__

#define COLOR_NONE "\033[m"
#define COLOR_RED  "\033[0;32;31m"

#include <stdint.h>

#define DEBUG_I2C_COMMAND           (0x00)
#define EPII_VERC_PPDONE            (0x01)
#define EPII_VERB_PPDONE_WORKAROUND (0x01)

#define CTRL_SLVID                  (0x28)
#define DATA_SLVID                  (0x28)

#define ISP_MEM_IN_ADDR             (0x3A000000)
#define ISP_MEM_OUT_ADDR            (0x38000000)
#define ISP_CONTROL_ADDR            (0x51010000)
#define ISP_DATA_I2C_SLVID          (0x28)
#define ISP_CONTROL_I2C_SLVID       (0x79)
#define SCU_ADDR                    (0x56000000)
#define SCU_ISP_XIP_SPICACHE_OFFSET (0xB00)
#define SCU_ISP_XIP_SPICACHE_ADDR   (0x53070B00)
#define SCU_UART1_PINMUX_ADDR       (0x52001118)
#define SCU_FPGA_VERSION1_ADDR      (0x56100FF0)
#define SCU_FPGA_VERSION2_ADDR      (0x56100FF4)
#define AHB_DEBUG_I2C_SLVID         (0x79)
#define ISP_ENABLE_REG_ADDR         (0xD8)
#define ISP_DISABLE_PASSWD          (0x5AFF00A5)
#define ISP_STATUS_CLR_OFFEST       (0x30)
#define ISP_PPDONE_OFFSET           (0x50)
#define ISP_PPDONE_COUNTER_OFFSET   (0x40)
#define ISP_CRC_ADDR_OFFSET         (0x0C)
#define ISP_REG_DEFAULT_VAL         (0x0288208F)
#define ISP_DATA_PORT_ADDR          (0x38000000)

#define PROGRESS_STEP               (2048)

typedef enum {
    REG_OFF           = 0x00,
    REG_BYPASS_CACHE  = 0x00,
    REG_ISP_WRITE_EN  = 0x01,
    REG_XIP_EN        = 0x02,
    REG_ISP_TEST_MODE = 0x04,
    REG_D8_ISP_EN     = 0x01,
    REG_D8_TEST_MODE  = 0x02,
    REG_D8_SPI_DO_EN  = 0x04,
} FlashSubMod_t;

#define SUCCESS 0
#define FAILURE -1

#define true 1
#define false 0

#define ARRAY_COUNT(x) (sizeof(x) / sizeof(x[0]))

void clear_screen();

void printProgress(double percentage);

uint32_t get_dec_selection();
uint32_t get_hex_selection();

extern uint32_t i2cspeed_setting;

#endif
