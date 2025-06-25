/*
 * Copyright 2025 Core Devices LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "os/mutex.h"
#include "os/tick.h"
#include "drivers/dma.h"
#include "drivers/gpio.h"
#include "drivers/periph_config.h"
#include "system/passert.h"
#include "drivers/sf32lb52/i2c_hal_definitions.h"
#include "drivers/i2c_definitions.h"
#include "bf0_pin_const.h"

#define I2C_HAL_DEBUG 1
enum
{
    #ifdef I2C1
    I2C1_INDEX,
    #endif
    #ifdef I2C2
    I2C2_INDEX,
    #endif
    #ifdef I2C3
    I2C3_INDEX,
    #endif
    #ifdef I2C4
    I2C4_INDEX,
    #endif
    #ifdef I2C5
    I2C5_INDEX,
    #endif
    #ifdef I2C6
    I2C6_INDEX,
    #endif
    I2C_MAX,
};

#if defined(I2C1)
#define BF0_I2C1_CFG                      \
    {                                     \
        .device_name = "i2c1",            \
        .Instance = I2C1,                 \
        .irq_type = I2C1_IRQn,            \
        .core     = I2C1_CORE,            \
        .open_flag= RT_DEVICE_FLAG_RDWR,    \
    }
#define I2C1_CFG_DEFAULT                \
    {                                    \
        0,                              \
        0,                              \
        5000,                           \
        400000,                          \
    }
#endif

#if defined(I2C2)
#define BF0_I2C2_CFG                      \
    {                                     \
        .device_name = "i2c2",            \
        .Instance = I2C2,                 \
        .irq_type = I2C2_IRQn,            \
        .core     = I2C2_CORE,            \
        .open_flag= RT_DEVICE_FLAG_RDWR,    \
    }
#define I2C2_CFG_DEFAULT                \
    {                                     \
        0,                              \
        0,                              \
        5000,                           \
        400000,                          \
    }
#endif

#if defined(I2C3)
#define BF0_I2C3_CFG                      \
    {                                     \
        .device_name = "i2c3",            \
        .Instance = I2C3,                 \
        .irq_type = I2C3_IRQn,            \
        .core     = I2C3_CORE,            \
        .open_flag= RT_DEVICE_FLAG_RDWR,    \
    }
#define I2C3_CFG_DEFAULT                \
    {                                     \
        0,                              \
        0,                              \
        5000,                           \
        400000,                          \
    }
#endif

#if defined(I2C4)
#define BF0_I2C4_CFG                      \
    {                                     \
        .device_name = "i2c4",            \
        .Instance = I2C4,                 \
        .irq_type = I2C4_IRQn,            \
        .core     = I2C4_CORE,            \
        .open_flag= RT_DEVICE_FLAG_RDWR,    \
    }
#define I2C4_CFG_DEFAULT                \
    {                                     \
        0,                              \
        0,                              \
        5000,                           \
        400000,                          \
    }
#endif


#if defined(I2C5)
#define BF0_I2C5_CFG                      \
    {                                     \
        .device_name = "i2c5",            \
        .Instance = I2C5,                 \
        .irq_type = I2C5_IRQn,            \
        .core     = I2C5_CORE,            \
        .open_flag= RT_DEVICE_FLAG_RDWR,    \
    }
#define I2C5_CFG_DEFAULT                \
    {                                    \
        0,                              \
        0,                              \
        5000,                           \
        400000,                          \
    } 
#endif

#if defined(I2C6)
#define BF0_I2C6_CFG                      \
    {                                     \
        .device_name = "i2c6",            \
        .Instance = I2C6,                 \
        .irq_type = I2C6_IRQn,            \
        .core     = I2C6_CORE,            \
        .open_flag= RT_DEVICE_FLAG_RDWR,    \
    }
#define I2C6_CFG_DEFAULT                \
    {                                    \
        0,                              \
        0,                              \
        5000,                           \
        400000,                          \
    } 
#endif


static bf0_i2c_config_t bf0_i2c_cfg[] =
{
    #if defined(I2C1)
    BF0_I2C1_CFG,
    #endif
    
    #if defined(I2C2)
    BF0_I2C2_CFG,
    #endif

    #if defined(I2C3)
    BF0_I2C3_CFG,
    #endif

    #if defined(I2C4)
    BF0_I2C4_CFG,
    #endif
    
    #if defined(I2C5)
    BF0_I2C5_CFG,
    #endif

    #if defined(I2C6)
    BF0_I2C6_CFG,
    #endif


};

#define I2C_NUM  (sizeof(bf0_i2c_cfg) / sizeof(bf0_i2c_cfg[0]))

static struct rt_i2c_configuration rt_i2c_cfg_default[I2C_NUM] =
{
    #ifdef I2C1
    I2C1_CFG_DEFAULT,
    #endif

    #ifdef I2C2
    I2C2_CFG_DEFAULT,
    #endif

    #ifdef I2C3
    I2C3_CFG_DEFAULT,
    #endif
    
    #ifdef I2C4
    I2C4_CFG_DEFAULT,
    #endif
    
    #ifdef I2C5
    I2C5_CFG_DEFAULT,
    #endif
        
    #ifdef I2C6
    I2C6_CFG_DEFAULT,
    #endif
};

struct I2CBusHal i2c_hal_obj[I2C_NUM];


static void hal_semaphore_give(I2CBusState *bus_state) 
{
    // If this fails, something is very wrong
    xSemaphoreGive(bus_state->event_semaphore);
}
static portBASE_TYPE hal_semaphore_give_from_isr(I2CBusState *bus) {
  portBASE_TYPE should_context_switch = pdFALSE;
  (void)xSemaphoreGiveFromISR(bus->event_semaphore,  &should_context_switch);
  return should_context_switch;
}

static void I2Cx_IRQHandler(uint16_t index)
{
    I2C_HandleTypeDef *handle ;
    I2CBus *bus = i2c_hal_obj[index].bus;
    handle = &( i2c_hal_obj[index].handle);

    if (handle->XferISR != NULL)
    {
        handle->XferISR(handle, 0, 0);
    }

    if ((HAL_I2C_STATE_BUSY_TX != handle->State) && (HAL_I2C_STATE_BUSY_RX != handle->State))
    {
        hal_semaphore_give_from_isr(bus->state); 
        HAL_I2C_StateTypeDef i2c_state = HAL_I2C_GetState(handle);
        if(i2c_state == HAL_I2C_STATE_READY)
            bus->state->transfer_event = I2CTransferEvent_TransferComplete;
        else if(i2c_state == HAL_I2C_STATE_TIMEOUT)
            bus->state->transfer_event = I2CTransferEvent_Timeout;
        else
            bus->state->transfer_event = I2CTransferEvent_Error;
        __HAL_I2C_DISABLE(handle);
    }

}

#if defined(I2C1)
void I2C1_IRQHandler(void)
{
    I2Cx_IRQHandler(I2C1_INDEX); 
}
#endif


#if defined(I2C2)
void I2C2_IRQHandler(void)
{
    I2Cx_IRQHandler(I2C2_INDEX); 
}
#endif


#if defined(I2C3)
void I2C3_IRQHandler(void)
{
    I2Cx_IRQHandler(I2C3_INDEX); 
}
#endif


#if defined(I2C4)
void I2C4_IRQHandler(void)
{
    I2Cx_IRQHandler(I2C4_INDEX); 
}
#endif

#if defined(I2C5)
void I2C5_IRQHandler(void)
{
    I2Cx_IRQHandler(I2C5_INDEX); 
}
#endif

#if defined(I2C6)
void I2C6_IRQHandler(void)
{
    I2Cx_IRQHandler(I2C6_INDEX); 
}
#endif

#if defined(I2C1_DMA_IRQHandler) || defined(I2C2_DMA_IRQHandler) || defined(I2C3_DMA_IRQHandler)  || defined(I2C4_DMA_IRQHandler)  || defined(I2C5_DMA_IRQHandler)  || defined(I2C6_DMA_IRQHandler)

static void I2Cx_DMA_IRQHandler(uint16_t index)
{
    I2C_HandleTypeDef *handle;
    handle = &(i2c_hal_obj[index].handle);
    if (handle->State == HAL_I2C_STATE_BUSY_TX)
    {
        HAL_DMA_IRQHandler(handle->hdmatx);
    }
    else if (handle->State == HAL_I2C_STATE_BUSY_RX)
    {
        HAL_DMA_IRQHandler(handle->hdmarx);
    }
    else
    {
        /*The I2C_handle->State is READY before this DMA IRQ sometimes.*/
        if (handle->hdmatx != NULL)
            if (HAL_DMA_STATE_BUSY == handle->hdmatx->State)
                HAL_DMA_IRQHandler(handle->hdmatx);

        if (handle->hdmarx != NULL)
            if (HAL_DMA_STATE_BUSY == handle->hdmarx->State)
                HAL_DMA_IRQHandler(handle->hdmarx);
    }


}
#endif
#if defined(I2C1_DMA_INSTANCE)
struct dma_config i2c1_trx_dma = 
{ 
    .dma_irq_prio = I2C1_DMA_IRQ_PRIO,
    .Instance = I2C1_DMA_INSTANCE,
    .dma_irq = I2C1_DMA_IRQ, 
    .request = I2C1_DMA_REQUEST, 
};

#endif

#if defined(I2C1_DMA_IRQHandler)
void I2C1_DMA_IRQHandler(void)
{
    I2Cx_DMA_IRQHandler(I2C1_INDEX);
}
#endif

#if defined(I2C2_DMA_INSTANCE)
struct dma_config i2c2_trx_dma = 
{ 
    .dma_irq_prio = I2C2_DMA_IRQ_PRIO,
    .Instance = I2C2_DMA_INSTANCE,
    .dma_irq = I2C2_DMA_IRQ, 
    .request = I2C2_DMA_REQUEST, 
};
#endif
#if defined(I2C2_DMA_IRQHandler)
void I2C2_DMA_IRQHandler(void)
{
    I2Cx_DMA_IRQHandler(I2C2_INDEX);
}
#endif
#if defined(I2C3_DMA_INSTANCE)
struct dma_config i2c3_trx_dma = 
{ 
    .dma_irq_prio = I2C3_DMA_IRQ_PRIO,
    .Instance = I2C3_DMA_INSTANCE,
    .dma_irq = I2C3_DMA_IRQ, 
    .request = I2C3_DMA_REQUEST, 
};
#endif
#if defined(I2C3_DMA_IRQHandler)
void I2C3_DMA_IRQHandler(void)
{
    I2Cx_DMA_IRQHandler(I2C3_INDEX);
}
#endif
#if defined(I2C4_DMA_INSTANCE)
struct dma_config i2c4_trx_dma = 
{ 
    .dma_irq_prio = I2C4_DMA_IRQ_PRIO,
    .Instance = I2C4_DMA_INSTANCE,
    .dma_irq = I2C4_DMA_IRQ, 
    .request = I2C4_DMA_REQUEST, 
};
#endif
#if defined(I2C4_DMA_IRQHandler)
void I2C4_DMA_IRQHandler(void)
{
    I2Cx_DMA_IRQHandler(I2C4_INDEX);
}
#endif
#if defined(I2C5_DMA_INSTANCE)
struct dma_config i2c5_trx_dma =
{ 
    .dma_irq_prio = I2C5_DMA_IRQ_PRIO,
    .Instance = I2C5_DMA_INSTANCE,
    .dma_irq = I2C5_DMA_IRQ, 
    .request = I2C5_DMA_REQUEST, 
};
#endif
#if defined(I2C5_DMA_IRQHandler)
void I2C5_DMA_IRQHandler(void)
{
    I2Cx_DMA_IRQHandler(I2C5_INDEX);
}
#endif
#if defined(I2C6_DMA_INSTANCE)
struct dma_config i2c6_trx_dma = 
{ 
    .dma_irq_prio = I2C6_DMA_IRQ_PRIO,
    .Instance = I2C6_DMA_INSTANCE,
    .dma_irq = I2C6_DMA_IRQ, 
    .request = I2C6_DMA_REQUEST, 
};
#endif
#if defined(I2C6_DMA_IRQHandler)
void I2C6_DMA_IRQHandler(void)
{
    I2Cx_DMA_IRQHandler(I2C6_INDEX);
}
#endif
struct rt_i2c_msg msgs[2];
uint32_t msgs_num;
static HAL_StatusTypeDef master_xfer(struct I2CBusHal * i2c_hal, struct rt_i2c_msg msgs[], uint32_t num)
{
    uint32_t index = 0;
    struct I2CBusHal *bf0_i2c = NULL;
    struct rt_i2c_msg *msg = NULL;
    HAL_StatusTypeDef status = HAL_ERROR;
    uint16_t mem_addr_type;

    PBL_ASSERTN(i2c_hal != NULL);
    bf0_i2c = i2c_hal;
    __HAL_I2C_ENABLE(&bf0_i2c->handle);
    for (index = 0; index < num; index++)
    {
        msg = &msgs[index];
        if (msg->flags & RT_I2C_MEM_ACCESS)
        {
            if (8 >= msg->mem_addr_size)
            {
                mem_addr_type = I2C_MEMADD_SIZE_8BIT;
            }
            else
            {
                mem_addr_type = I2C_MEMADD_SIZE_16BIT;
            }
            PBL_LOG(LOG_LEVEL_ALWAYS, "msg->addr = 0x%x, msg->mem_addr = 0x%x, type = %d, buf = 0x%x, len = 0x%d, timeout = %d;", 
            (unsigned int)msg->addr, (unsigned int)msg->mem_addr, (int)mem_addr_type, (unsigned int)msg->buf, (int)msg->len, (int)bf0_i2c->bf0_i2c_cfg->timeout);
            if (msg->flags & RT_I2C_RD)
            {
            
                if ((bf0_i2c->i2c_dma_flag) && (bf0_i2c->bf0_i2c_cfg->open_flag & RT_DEVICE_FLAG_DMA_RX))
                {
                    HAL_DMA_Init(&bf0_i2c->dma.dma_rx);
                    mpu_dcache_invalidate(msg->buf, msg->len);
                    status = HAL_I2C_Mem_Read_DMA(&bf0_i2c->handle, msg->addr, msg->mem_addr, mem_addr_type, msg->buf, msg->len);
                }
                else if (bf0_i2c->bf0_i2c_cfg->open_flag & RT_DEVICE_FLAG_INT_RX)
                {
                    status = HAL_I2C_Mem_Read_IT(&bf0_i2c->handle, msg->addr, msg->mem_addr, mem_addr_type, msg->buf, msg->len);
                }
                //else if (msg->flags & RT_I2C_NORMAL_MODE)
                else
                {
                    status = HAL_I2C_Mem_Read(&bf0_i2c->handle, msg->addr, msg->mem_addr, mem_addr_type, msg->buf, msg->len, bf0_i2c->bf0_i2c_cfg->timeout);
                }
            }
            else
            {
                if ((bf0_i2c->i2c_dma_flag) && (bf0_i2c->bf0_i2c_cfg->open_flag & RT_DEVICE_FLAG_DMA_TX))
                {
                    HAL_DMA_Init(&bf0_i2c->dma.dma_tx);
                    status = HAL_I2C_Mem_Write_DMA(&bf0_i2c->handle, msg->addr, msg->mem_addr, mem_addr_type, msg->buf, msg->len);
                }
                else if (bf0_i2c->bf0_i2c_cfg->open_flag & RT_DEVICE_FLAG_INT_TX)
                {
                    status = HAL_I2C_Mem_Write_IT(&bf0_i2c->handle, msg->addr, msg->mem_addr, mem_addr_type, msg->buf, msg->len);
                }
                //else if (msg->flags & RT_I2C_NORMAL_MODE)
                else
                {
                    status = HAL_I2C_Mem_Write(&bf0_i2c->handle, msg->addr, msg->mem_addr, mem_addr_type, msg->buf, msg->len, bf0_i2c->bf0_i2c_cfg->timeout);
                }

            }
        }
        else
        {
            if (msg->flags & RT_I2C_RD)
            {
                if ((bf0_i2c->i2c_dma_flag) && (bf0_i2c->bf0_i2c_cfg->open_flag & RT_DEVICE_FLAG_DMA_RX))
                {
                    HAL_DMA_Init(&bf0_i2c->dma.dma_rx);
                    mpu_dcache_invalidate(msg->buf, msg->len);
                    status = HAL_I2C_Master_Receive_DMA(&bf0_i2c->handle, msg->addr, msg->buf, msg->len);
                }
                else if (bf0_i2c->bf0_i2c_cfg->open_flag & RT_DEVICE_FLAG_INT_RX)
                {
                    status = HAL_I2C_Master_Receive_IT(&bf0_i2c->handle, msg->addr, msg->buf, msg->len);
                }
                //else if (msg->flags & RT_I2C_NORMAL_MODE)
                else
                {
                    status = HAL_I2C_Master_Receive(&bf0_i2c->handle, msg->addr, msg->buf, msg->len, bf0_i2c->bf0_i2c_cfg->timeout);
                }
            }
            else
            {
                if ((bf0_i2c->i2c_dma_flag) && (bf0_i2c->bf0_i2c_cfg->open_flag & RT_DEVICE_FLAG_DMA_TX))
                {
                    HAL_DMA_Init(&bf0_i2c->dma.dma_tx);
                    status = HAL_I2C_Master_Transmit_DMA(&bf0_i2c->handle, msg->addr, msg->buf, msg->len);
                }
                else if (bf0_i2c->bf0_i2c_cfg->open_flag & RT_DEVICE_FLAG_INT_TX)
                {
                    status = HAL_I2C_Master_Transmit_IT(&bf0_i2c->handle, msg->addr, msg->buf, msg->len);
                }
                else
                {
                    status = HAL_I2C_Master_Transmit(&bf0_i2c->handle, msg->addr, msg->buf, msg->len, bf0_i2c->bf0_i2c_cfg->timeout);
                }
            }
        }
        PBL_LOG(LOG_LEVEL_ALWAYS, "I2C hal transmit result = %d", (int)status);

        if (HAL_OK != status) goto exit;
#if 1
        while (1)
        {
            HAL_I2C_StateTypeDef i2c_state = HAL_I2C_GetState(&bf0_i2c->handle);

            if (HAL_I2C_STATE_READY == i2c_state)
            {
                status = HAL_OK;
            }
            else if (HAL_I2C_STATE_TIMEOUT == i2c_state)
            {
                status = HAL_TIMEOUT;
            }
            else if ((HAL_I2C_STATE_BUSY_TX == i2c_state) || (HAL_I2C_STATE_BUSY_RX == i2c_state)) //Interrupt or DMA mode, wait semaphore
            {
                //if(prv_semaphore_wait(objs) == pdTRUE)
                status = HAL_BUSY;                
            }
            else
            {
                status = HAL_ERROR;
            }

            break;
        }

        if (bf0_i2c->handle.ErrorCode) goto exit;

        if (HAL_OK != status) goto exit;

#if 1
        //Wordaround: I2C Wait TE fail in 'I2C_MasterRequestWrite' after ISR mode read(After 58x)  - Fixed
        bf0_i2c->handle.Instance->CR |= I2C_CR_UR;
        HAL_Delay_us(1);        // Delay at least 9 cycle.
        bf0_i2c->handle.Instance->CR &= ~I2C_CR_UR;
#endif
#endif
 
    }


exit:
#if  0
    if ((ret != num) || (HAL_OK != status))
    {
        PBL_LOG(LOG_LEVEL_ALWAYS,"bus err:%d, xfer:%d/%d, i2c_stat:%x, i2c_errcode=%x", status, (int)index, (int)num, HAL_I2C_GetState(&bf0_i2c->handle), (int)bf0_i2c->handle.ErrorCode);
        HAL_I2C_Reset(&bf0_i2c->handle);
        PBL_LOG(LOG_LEVEL_ALWAYS,"reset and send 9 clks");
    }
#endif     

    if(status != HAL_BUSY)
        __HAL_I2C_DISABLE(&bf0_i2c->handle);

    PBL_LOG(LOG_LEVEL_ALWAYS,"master_xfer");

    return status;
}

void i2c_hal_init_transfer(I2CBus *bus)
{
    
    if(I2CTransferType_SendRegisterAddress == bus->state->transfer.type)
    {
        if(bus->state->transfer.direction == I2CTransferDirection_Write)
        {
            msgs[0].addr   = bus->state->transfer.device_address;
            msgs[0].mem_addr = bus->state->transfer.register_address;
            msgs[0].mem_addr_size = 8;      // 8bit address
            msgs[0].flags  = RT_I2C_WR | RT_I2C_MEM_ACCESS;
            msgs[0].len    = bus->state->transfer.size;
            msgs[0].buf    = bus->state->transfer.data;
            msgs_num = 1;
         }
        else 
        {
            msgs[0].addr   = bus->state->transfer.device_address;
            msgs[0].mem_addr = bus->state->transfer.register_address;
            msgs[0].mem_addr_size = 8;      // 8bit address
            msgs[0].flags  = RT_I2C_RD | RT_I2C_MEM_ACCESS;
            msgs[0].len    = bus->state->transfer.size;
            msgs[0].buf    = bus->state->transfer.data;
            msgs_num = 1;
        }


    }
    else
    {
        if(bus->state->transfer.direction == I2CTransferDirection_Write)
        {
            msgs[0].addr   = bus->state->transfer.device_address;
            msgs[0].flags  = RT_I2C_WR;
            msgs[0].len    = bus->state->transfer.size;
            msgs[0].buf    = bus->state->transfer.data;
            msgs_num = 1;
         }
        else 
        {
            msgs[0].addr   = bus->state->transfer.device_address;
            msgs[0].flags  = RT_I2C_RD ;
            msgs[0].len    = bus->state->transfer.size;
            msgs[0].buf    = bus->state->transfer.data;
            msgs_num = 1;
        } 

    }  
}

void i2c_hal_abort_transfer(I2CBus *bus)
{
    struct I2CBusHal * hal = (struct I2CBusHal * )bus->hal;
    HAL_I2C_Reset(&(hal->handle));
    //I2C_HandleTypeDef
    PBL_LOG(LOG_LEVEL_ALWAYS,"reset and send 9 clks");
    __HAL_I2C_DISABLE(&(hal->handle));
}
void i2c_hal_start_transfer(I2CBus *bus)
{
    struct I2CBusHal * hal = (struct I2CBusHal * )bus->hal;
    PBL_LOG(LOG_LEVEL_ALWAYS,"i2c_hal_start_transfer, 0x%p", hal);
    HAL_StatusTypeDef status = master_xfer(hal, &msgs[0], msgs_num);
    if(status ==HAL_BUSY)
        return;
    if(status == HAL_OK)
    {
        bus->state->transfer_event = I2CTransferEvent_TransferComplete;
        if((bus->hal->i2c_dma_flag != 1) && (bus->hal->i2c_int_flag != 1))
            hal_semaphore_give(bus->state);
    }
    else if(status == HAL_TIMEOUT)
        bus->state->transfer_event = I2CTransferEvent_Timeout;
    else
        bus->state->transfer_event = I2CTransferEvent_Error;
    return;
}

int i2c_bus_configure(struct I2CBusHal * i2c_hal, struct rt_i2c_configuration *configuration)
{
    HAL_StatusTypeDef ret = HAL_OK;
    PBL_ASSERTN(i2c_hal != NULL);
    PBL_ASSERTN(configuration != NULL);
    i2c_hal->bf0_i2c_cfg->open_flag = RT_DEVICE_FLAG_RDWR | configuration->open_flag ;
    i2c_hal->bf0_i2c_cfg->timeout = configuration->timeout;

    if (configuration->mode & RT_I2C_ADDR_10BIT)
    {
        i2c_hal->handle.Init.AddressingMode = I2C_ADDRESSINGMODE_10BIT;
        i2c_hal->handle.Init.OwnAddress1 = (configuration->addr & 0x7fff);
    }
    else
    {
        i2c_hal->handle.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
        i2c_hal->handle.Init.OwnAddress1 = (configuration->addr & 0x7fff) << 1;
    }
    
    i2c_hal->handle.Init.ClockSpeed = configuration->max_hz;
    i2c_hal->handle.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    i2c_hal->handle.core = i2c_hal->bf0_i2c_cfg->core;
    i2c_hal->handle.Mode = HAL_I2C_MODE_MASTER;


    #if defined(I2C1)               
    if (i2c_hal->handle.Instance == hwp_i2c1)
        HAL_RCC_EnableModule(RCC_MOD_I2C1);
    #endif
    #if defined(I2C2)  
    if (i2c_hal->handle.Instance == hwp_i2c2)
        HAL_RCC_EnableModule(RCC_MOD_I2C2);
    #endif
    #if defined(I2C3)  
    if (i2c_hal->handle.Instance == hwp_i2c3)
        HAL_RCC_EnableModule(RCC_MOD_I2C3);
    #endif
    #if defined(I2C4)      
    if (i2c_hal->handle.Instance == hwp_i2c4)
        HAL_RCC_EnableModule(RCC_MOD_I2C4);
    #endif
    #if defined(I2C5)  
    if (i2c_hal->handle.Instance == hwp_i2c5)
        HAL_RCC_EnableModule(RCC_MOD_I2C5);
    #endif    
    #if defined(I2C6)  
    if (i2c_hal->handle.Instance == hwp_i2c6)
        HAL_RCC_EnableModule(RCC_MOD_I2C6);
    #endif

    ret = HAL_I2C_Init(&(i2c_hal->handle));
    
    if(ret != HAL_OK)
    {
        PBL_LOG(LOG_LEVEL_ERROR, "I2C [%s] bus_configure fail!", i2c_hal->bf0_i2c_cfg->device_name);
        return -1;
    }
    #ifdef I2C_HAL_DEBUG
    PBL_LOG(LOG_LEVEL_INFO, "I2C [%s] bus_configure ok!", i2c_hal->bf0_i2c_cfg->device_name);
    #endif
    return 0;
}




void i2c_hal_enable(I2CBus *bus)
{
    #if defined(I2C1)
    if(bus->hal->handle.Instance == I2C1)
    {
        HAL_RCC_EnableModule(RCC_MOD_I2C1);        
    }
    #endif
    #if defined(I2C2)
    else if(bus->hal->handle.Instance == I2C2)
    {        
        HAL_RCC_EnableModule(RCC_MOD_I2C2);        
    }
    #endif
    #if defined(I2C3)
    else if(bus->hal->handle.Instance == I2C3)
    {        
        HAL_RCC_EnableModule(RCC_MOD_I2C3);    
    }
    #endif
    #if defined(I2C4)
    else if(bus->hal->handle.Instance == I2C4)
    {        
        HAL_RCC_EnableModule(RCC_MOD_I2C4);        
    }
    #endif
    #if defined(I2C5)
    else if(bus->hal->handle.Instance == I2C5)
    {
         HAL_RCC_EnableModule(RCC_MOD_I2C5);        
    }
    #endif
    #if defined(I2C6)
    else if(bus->hal->handle.Instance == I2C6)
    {        
        HAL_RCC_EnableModule(RCC_MOD_I2C6);        
    }
    #endif
    PBL_LOG(LOG_LEVEL_INFO, "I2C [%s] enable!", bus->name);
}

void i2c_hal_disable(I2CBus *bus)
{
    #if defined(I2C1)
    if(bus->hal->handle.Instance == I2C1)
    {
        HAL_RCC_DisableModule(RCC_MOD_I2C1);        
    }
    #endif
    #if defined(I2C2)
    else if(bus->hal->handle.Instance == I2C2)
    {        
        HAL_RCC_DisableModule(RCC_MOD_I2C2);        
    }
    #endif
    #if defined(I2C3)
    else if(bus->hal->handle.Instance == I2C3)
    {        
        HAL_RCC_DisableModule(RCC_MOD_I2C3);    
    }
    #endif
    #if defined(I2C4)
    else if(bus->hal->handle.Instance == I2C4)
    {        
        HAL_RCC_DisableModule(RCC_MOD_I2C4);        
    }
    #endif
    #if defined(I2C5)
    else if(bus->hal->handle.Instance == I2C5)
    {
        HAL_RCC_DisableModule(RCC_MOD_I2C5);        
    }
    #endif
    #if defined(I2C6)
    else if(bus->hal->handle.Instance == I2C6)
    {        
        HAL_RCC_DisableModule(RCC_MOD_I2C6);        
    }
    #endif    
    PBL_LOG(LOG_LEVEL_INFO, "I2C [%s] disable!", bus->name);
}

bool i2c_hal_is_busy(I2CBus *bus)
{
    bool ret = false;
    struct I2CBusHal * hal = (struct I2CBusHal * )bus->hal;
    if(HAL_I2C_GetState(&(hal->handle)) != HAL_I2C_STATE_READY)
        ret = true;
    return ret;
}       


static void i2c_get_dma_info(uint16_t index)
{
    struct dma_config *i2c_dma = NULL;
    if(index > I2C_NUM)
        return;
    

    if(index == 0)
    {
        #if defined(I2C1_DMA_INSTANCE)
        i2c_dma = &i2c1_trx_dma;
        #endif
    }
    else if(index == 1)
    {
        #if defined(I2C2_DMA_INSTANCE)
        i2c_dma = &i2c2_trx_dma;
        #endif
    }
    else if(index == 2)
    {
        #if defined(I2C3_DMA_INSTANCE)
        i2c_dma = &i2c3_trx_dma;
        #endif
    }
    else if(index == 3)
    {   
        #if defined(I2C4_DMA_INSTANCE)
        i2c_dma = &i2c4_trx_dma;
        #endif
    }
    else if(index == 4)
    {
        #if defined(I2C5_DMA_INSTANCE)
        i2c_dma = &i2c5_trx_dma;
        #endif
    }
    else if(index == 5)
    {
        #if defined(I2C6_DMA_INSTANCE)
        i2c_dma = &i2c6_trx_dma;
        #endif
    }
    if(i2c_dma)
    {
        i2c_hal_obj[index].i2c_dma_flag = 1;
        bf0_i2c_cfg[index].dma_rx = i2c_dma;
        bf0_i2c_cfg[index].dma_tx = i2c_dma;
        PBL_LOG(LOG_LEVEL_INFO, "I2C [%s] has config DMA!", bf0_i2c_cfg[index].device_name);
    }
    else
        PBL_LOG(LOG_LEVEL_INFO, "I2C [%s] hasn't config DMA!", bf0_i2c_cfg[index].device_name);

}

int rt_hw_i2c_init(struct I2CBusHal * i2c_hal, bf0_i2c_config_t *cfg, struct rt_i2c_configuration *cfg_default)
{
    int ret = 0;

    i2c_hal->bf0_i2c_cfg = cfg;
    i2c_hal->i2c_configuration = cfg_default;
    i2c_hal->handle.Instance = cfg->Instance;

    if (i2c_hal->i2c_dma_flag)
    {
        __HAL_LINKDMA(&(i2c_hal->handle), hdmarx, i2c_hal->dma.dma_rx);
        __HAL_LINKDMA(&(i2c_hal->handle), hdmatx, i2c_hal->dma.dma_tx);
        HAL_I2C_DMA_Init(&(i2c_hal->handle), cfg->dma_rx, cfg->dma_tx);

        HAL_NVIC_SetPriority(i2c_hal->bf0_i2c_cfg->dma_rx->dma_irq, 5, 0); 
        NVIC_EnableIRQ(i2c_hal->bf0_i2c_cfg->dma_rx->dma_irq);
        HAL_NVIC_SetPriority(i2c_hal->bf0_i2c_cfg->dma_rx->dma_irq, 5, 0);
        NVIC_EnableIRQ(i2c_hal->bf0_i2c_cfg->dma_rx->dma_irq);
    }
    else if(i2c_hal->i2c_int_flag)
    {
        HAL_NVIC_SetPriority(i2c_hal->bf0_i2c_cfg->irq_type, 5, 0);
        NVIC_EnableIRQ(i2c_hal->bf0_i2c_cfg->irq_type);
    }
    ret = i2c_bus_configure(i2c_hal, cfg_default);
    if(ret <  0)
    {
        return ret;
    }
    return ret;
}

uint16_t find_i2c_bus(I2CBus *bus)
{
    uint16_t i;
    uint16_t index = 0xff;
    struct I2CBusHal *obj = NULL;
    for(i = 0; i < I2C_NUM; i++)
    {
        //if(strcmp(bf0_i2c_cfg[i].device_name, (char *)bus->name) == 0)
        obj = &(i2c_hal_obj[i]);
        if(obj == bus->hal)
        {
          index = i;
          break;
        }

    }
    if(index != 0xff)
    {
        PBL_LOG(LOG_LEVEL_INFO, "I2C find [%s] index = [%d] ok!", (char *)bus->name, index);
    }
    else 
    {
        PBL_LOG(LOG_LEVEL_INFO, "I2C find [%s] fail!", (char *)bus->name);
    }
    
    return index;
}

void i2c_hal_init(I2CBus *bus)
{
    int ret = 0;
    PBL_ASSERTN(bus != NULL);
    uint16_t index = 0xff;
    index = find_i2c_bus(bus);
    if(index > I2C_NUM)
    {
        ret = -1;
        goto exit;
    }
    i2c_get_dma_info(index);
    ret = rt_hw_i2c_init(&i2c_hal_obj[index], &bf0_i2c_cfg[index], &rt_i2c_cfg_default[index]);
    if(ret < 0)
    {
        PBL_LOG(LOG_LEVEL_ERROR, "I2C [%s] hw init fail!", bf0_i2c_cfg[index].device_name);
    }
    else
    {
        PBL_LOG(LOG_LEVEL_INFO, "I2C [%s] hw init ok!", bf0_i2c_cfg[index].device_name);
        i2c_hal_obj[index].bus = bus;
    }
    
  
exit:
    if(ret <  0)
        PBL_LOG(LOG_LEVEL_ERROR, "I2C [%s] hal init fail!", bus->name);
        
    else
        PBL_LOG(LOG_LEVEL_INFO, "I2C [%s] hal init ok!", bus->name);
    return;
}


void i2c_hal_pins_set_gpio(I2CBus *bus)
{
    
} 
void i2c_hal_pins_set_i2c(I2CBus *bus)
{
    #if 1
    int pad_sda = 0, pad_scl = 0;
    int hcpu_sda = 1, hcpu_scl = 1;
    uint32_t pin_sda = 0xff, pin_scl = 0xff;
    pin_function func_sda = PIN_FUNC_UNDEF, func_scl = PIN_FUNC_UNDEF;
    if(bus->hal->handle.Instance == I2C1)
    {
        func_sda = I2C1_SDA;
        func_scl = I2C1_SCL;
    }
    #if defined(I2C2)
    else if(bus->hal->handle.Instance == I2C2)
    {
        func_sda = I2C2_SDA;
        func_scl = I2C2_SCL;
    }
    #endif
    #if defined(I2C3)
    else if(bus->hal->handle.Instance == I2C3)
    {
        func_sda = I2C3_SDA;
        func_scl = I2C3_SCL;
    }
    #endif
    #if defined(I2C4)
    else if(bus->hal->handle.Instance == I2C4)
    {
        func_sda = I2C4_SDA;
        func_scl = I2C4_SCL;
    }
    #endif
    #if defined(I2C5)
    else if(bus->hal->handle.Instance == I2C5)
    {
        func_sda = I2C5_SDA;
        func_scl = I2C5_SCL;
    }
    #endif
    #if defined(I2C6)
    else if(bus->hal->handle.Instance == I2C6)
    {
        func_sda = I2C6_SDA;
        func_scl = I2C6_SCL;
    }
    #endif
    else
    {
        func_sda = I2C1_SDA;
        func_scl = I2C1_SCL;
    }

    pin_sda = bus->sda_gpio.gpio_pin;
    hcpu_sda = (pin_sda > 96) ? 0 : 1;
    if(hcpu_sda)
    {
        pad_sda = pin_sda + PAD_PA00;
    }
    else
    {
        pad_sda = pin_sda - 96 + PAD_PB00;
    }
    if(pad_sda > 0)
    {
        HAL_PIN_Set(pad_sda, func_sda, PIN_NOPULL, hcpu_sda);
        PBL_LOG(LOG_LEVEL_INFO, "set pin[%d],as [%s] sda pin;", (int)pin_sda, bus->name);
    }
    

    pin_scl = bus->scl_gpio.gpio_pin;
    hcpu_scl = (pin_scl > 96) ? 0 : 1;
    if(hcpu_scl)
    {
        pad_scl = pin_scl + PAD_PA00;
    }
    else
    {
        pad_scl = pin_scl - 96 + PAD_PB00;
    }
    if(pad_scl > 0)
    {
        HAL_PIN_Set(pad_scl, func_scl, PIN_NOPULL, hcpu_scl);
        PBL_LOG(LOG_LEVEL_INFO, "set pin[%d],as [%s] scl pin;", (int)pin_scl, bus->name);
    }
        
    #endif
}

