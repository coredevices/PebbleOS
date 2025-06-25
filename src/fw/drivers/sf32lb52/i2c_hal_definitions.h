#pragma once

#include "os/mutex.h"
#include "kernel/util/stop.h"
#include "freertos_types.h"
#include <stdbool.h>
#include <stdint.h>
#include "board/board.h"
#include "drivers/i2c_definitions.h"
#include "drivers/dma.h"
#include "bf0_hal.h"
#include "register.h"

typedef struct bf0_i2c_config
{
    const char *device_name;
    I2C_TypeDef *Instance;
    IRQn_Type irq_type;
    uint16_t open_flag;
    uint8_t core;
    struct dma_config *dma_rx;
    struct dma_config *dma_tx;
    uint32_t timeout;
} bf0_i2c_config_t;
struct rt_i2c_configuration
{
    uint16_t mode;
    uint16_t addr;
    uint32_t timeout;
    uint32_t max_hz;
    uint16_t open_flag;
    ;
};

typedef struct I2CBusHal
{
    I2C_HandleTypeDef handle;
    bf0_i2c_config_t *bf0_i2c_cfg;
    const struct I2CBus *bus; 
    struct rt_i2c_configuration *i2c_configuration;
    struct
    {
        DMA_HandleTypeDef dma_rx;
        DMA_HandleTypeDef dma_tx;
    } dma;
    uint8_t i2c_dma_flag;
    uint8_t i2c_int_flag;
} I2CBusHal;

struct rt_i2c_msg
{
    uint16_t addr;
    uint16_t mem_addr;
    uint16_t mem_addr_size;
    uint16_t flags;
    uint16_t len;
    uint8_t  *buf;
};


extern struct I2CBusHal i2c_hal_obj[];


#define RT_I2C_WR                0x0000
#define RT_I2C_RD               (1u << 0)
#define RT_I2C_ADDR_10BIT       (1u << 2)  /* this is a ten bit chip address */
#define RT_I2C_NO_START         (1u << 4)
#define RT_I2C_IGNORE_NACK      (1u << 5)
#define RT_I2C_NO_READ_ACK      (1u << 6)  /* when I2C reading, we do not ACK */
/* read/write specified memory address,
   in this mode, no STOP condition is inserted between memory address and data */
#define RT_I2C_MEM_ACCESS       (1u << 7)

#define RT_DEVICE_FLAG_DEACTIVATE       0x000           /**< device is not not initialized */

#define RT_DEVICE_FLAG_RDONLY           0x001           /**< read only */
#define RT_DEVICE_FLAG_WRONLY           0x002           /**< write only */
#define RT_DEVICE_FLAG_RDWR             0x003           /**< read and write */

#define RT_DEVICE_FLAG_REMOVABLE        0x004           /**< removable device */
#define RT_DEVICE_FLAG_STANDALONE       0x008           /**< standalone device */
#define RT_DEVICE_FLAG_ACTIVATED        0x010           /**< device is activated */
#define RT_DEVICE_FLAG_SUSPENDED        0x020           /**< device is suspended */
#define RT_DEVICE_FLAG_STREAM           0x040           /**< stream mode */

#define RT_DEVICE_FLAG_INT_RX           0x100           /**< INT mode on Rx */
#define RT_DEVICE_FLAG_DMA_RX           0x200           /**< DMA mode on Rx */
#define RT_DEVICE_FLAG_INT_TX           0x400           /**< INT mode on Tx */
#define RT_DEVICE_FLAG_DMA_TX           0x800           /**< DMA mode on Tx */

#define RT_DEVICE_OFLAG_CLOSE           0x000           /**< device is closed */
#define RT_DEVICE_OFLAG_RDONLY          0x001           /**< read only access */
#define RT_DEVICE_OFLAG_WRONLY          0x002           /**< write only access */
#define RT_DEVICE_OFLAG_RDWR            0x003           /**< read and write */
#define RT_DEVICE_OFLAG_OPEN            0x008           /**< device is opened */
#define RT_DEVICE_OFLAG_MASK            0xf0f           /**< mask of open flag */

#define I2C1_CORE   CORE_ID_HCPU
#define I2C2_CORE   CORE_ID_HCPU
#define I2C3_CORE   CORE_ID_HCPU
#define I2C4_CORE   CORE_ID_LCPU
#define I2C5_CORE   CORE_ID_LCPU
#define I2C6_CORE   CORE_ID_LCPU
#if 0
#define I2C1_DMA_IRQHandler              DMAC1_CH3_IRQHandler
#define I2C1_DMA_IRQ_PRIO                1
#define I2C1_DMA_INSTANCE                DMA1_Channel5
#define I2C1_DMA_REQUEST                 DMA_REQUEST_22
#define I2C1_DMA_IRQ                     DMAC1_CH5_IRQn
#endif

