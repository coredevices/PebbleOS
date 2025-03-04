/*
 * Copyright (c) 2015 - 2024, Nordic Semiconductor ASA
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <nrfx.h>

#if NRFX_CHECK(NRFX_UARTE_ENABLED)

#include <nrfx_uarte.h>
//#include "prs/nrfx_prs.h"
#include <haly/nrfy_gpio.h>
#include <string.h>
#include <soc/nrfx_coredep.h>

#define NRFX_LOG_MODULE UARTE
#include <nrfx_log.h>

#define NRFX_IS_ENABLED(config_macro) _NRFX_IS_ENABLED1(config_macro)
#define _NRFX_IS_ENABLED1(config_macro) _NRFX_IS_ENABLED2(_XXXX##config_macro)
#define _NRFX_IS_ENABLED2(one_or_two_args) _NRFX_IS_ENABLED3(one_or_two_args 1, 0)
#define _NRFX_IS_ENABLED3(ignore_this, val, ...) val
#define NRFX_MIN(a, b) ((a) < (b) ? (a) : (b))
#define NRFX_BIT_MASK(x) (((x) == 32) ? UINT32_MAX : ((1UL << (x)) - 1))

NRF_STATIC_INLINE bool nrf_dma_accessible_check(void const * p_reg, void const * p_object)
{
    (void) p_reg;
    return ((((uint32_t)p_object) & 0xE0000000u) == 0x20000000u);
}



#if !defined(NRFX_UARTE_RX_FIFO_FLUSH_WORKAROUND_MAGIC_BYTE)
#define NRFX_UARTE_RX_FIFO_FLUSH_WORKAROUND_MAGIC_BYTE 171
#endif

#define UARTEX_LENGTH_VALIDATE(periph_name, prefix, i, drv_inst_idx, len1, len2) \
    (((drv_inst_idx) == NRFX_CONCAT(NRFX_, periph_name, prefix, i, _INST_IDX)) && \
     NRFX_EASYDMA_LENGTH_VALIDATE(NRFX_CONCAT(periph_name, prefix, i), len1, len2))

#define UARTE_LENGTH_VALIDATE(drv_inst_idx, len)    \
        (NRFX_FOREACH_ENABLED(UARTE, UARTEX_LENGTH_VALIDATE, (||), (0), drv_inst_idx, len, 0))

#if NRFX_CHECK(NRFX_UARTE_CONFIG_RX_CACHE_ENABLED)
// Internal cache buffer is used if buffers provided by a user cannot be used in DMA. This is a
// HW limitation on some platforms but for testing purposes it can be emulated on any platform.
#define RX_CACHE_SUPPORTED 1
#else
#define RX_CACHE_SUPPORTED 0
#endif

#define MIN_RX_CACHE_SIZE 8
// There is a HW bug which results in RX amount value not being updated when FIFO was empty.
// It is then hard to determine if FIFO contained anything or not.
#define USE_WORKAROUND_FOR_FLUSHRX_ANOMALY 1

// Size of the RX HW FIFO
#define UARTE_HW_RX_FIFO_SIZE 5

// Set of flags which if needed are atomically controlled. Flags maintain current state and
// configuration.
#define UARTE_FLAG_TX_CNT      12 // Max amount of TX flags
#define UARTE_FLAG_RX_CNT      12 // Max amount of RX flags
#define UARTE_FLAG_MISC_CNT    8  // Max amount of miscellaneous flags

#define UARTE_FLAG_TX_OFFSET   0
#define UARTE_FLAG_RX_OFFSET   UARTE_FLAG_TX_CNT
#define UARTE_FLAG_MISC_OFFSET (UARTE_FLAG_RX_OFFSET + UARTE_FLAG_RX_CNT)

#define UARTE_FLAG(type, i) \
    NRFX_BIT(NRFX_CONCAT(UARTE_FLAG_,type,_OFFSET) + i)

// Mask with all RX flags
#define UARTE_RX_FLAGS (NRFX_BIT_MASK(UARTE_FLAG_RX_CNT) << UARTE_FLAG_RX_OFFSET)

// Flag set when there is a PPI connection set up between ENDTX event and TXSTOP task.
#define UARTE_FLAG_TX_STOP_ON_END          UARTE_FLAG(TX, 0)
//
// Flag is set when internal TX buffer is used as a cache/proxy buffer. It is used when user buffer
// is in the memory that cannot be used by the DMA.
#define UARTE_FLAG_TX_USE_CACHE            UARTE_FLAG(TX, 1)

// Flag is used to indicate that asynchronous TX transfer request occured during blocking transfer.
// Asynchronous pending transfer is started immediately after the current blocking transfer is
// completed.
#define UARTE_FLAG_TX_PENDING              UARTE_FLAG(TX, 2)

// Flag indicates that TX abort is in progress.
#define UARTE_FLAG_TX_ABORTED              UARTE_FLAG(TX, 3)

// Flag indicates that TX transfers are linked (by ENDTX-STARTTX PPI connection set up by the user).
#define UARTE_FLAG_TX_LINKED               UARTE_FLAG(TX, 4)

// Flag is set when receiver is active.
#define UARTE_FLAG_RX_ENABLED              UARTE_FLAG(RX, 0)

// Flag is set if receiver is enabled with flag NRFX_UARTE_RX_ENABLE_STOP_ON_END.
#define UARTE_FLAG_RX_STOP_ON_END          UARTE_FLAG(RX, 1)

// Flag is set if receiver is enabled with flag NRFX_UARTE_RX_ENABLE_CONT.
#define UARTE_FLAG_RX_CONT                 UARTE_FLAG(RX, 2)

// Flag is set if receiver is enabled with flag NRFX_UARTE_RX_ENABLE_KEEP_FIFO_CONTENT.
#define UARTE_FLAG_RX_KEEP_FIFO_CONTENT    UARTE_FLAG(RX, 3)

// Flag indicates that RX abort was done to seamlessly switch the DMA buffer and not to abort the
// transfer.
#define UARTE_FLAG_RX_RESTARTED            UARTE_FLAG(RX, 4)

// Flag is set when internal RX buffer is used as a cache/proxy buffer. It is used when user buffer
// is in the memory that cannot be used by the DMA.
#define UARTE_FLAG_RX_USE_CACHE            UARTE_FLAG(RX, 5)

// Flag indicates that RX was aborted.
#define UARTE_FLAG_RX_ABORTED              UARTE_FLAG(RX, 6)

// Flag indicates that there are new bytes from flushed buffer copied to the user buffer.
#define UARTE_FLAG_RX_FROM_FLUSH           UARTE_FLAG(RX, 7)

// Flag indicates user explicitly aborted RX.
#define UARTE_FLAG_RX_FORCED_ABORT         UARTE_FLAG(RX, 8)

// Flag is set if instance was configured to control PSEL pins during the initialization.
#define UARTE_FLAG_PSEL_UNINIT             UARTE_FLAG(MISC, 0)

// Flag is set if instance was configured to control GPIO pins during the initialization.
#define UARTE_FLAG_GPIO_UNINIT             UARTE_FLAG(MISC, 1)

// Flag is atomically set when nrfx_uarte_int_trigger is called.
#define UARTE_FLAG_TRIGGER                 UARTE_FLAG(MISC, 2)

// Flag indicates that HWFC pins are being configured.
#define UARTE_FLAG_HWFC_PINS               UARTE_FLAG(MISC, 3)

typedef struct
{
    /* User provided buffers. */
    nrfy_uarte_buffer_t     curr;
    nrfy_uarte_buffer_t     next;
    nrfy_uarte_buffer_t     flush;
    nrfx_uarte_rx_cache_t * p_cache;
    size_t                  off;
} uarte_rx_data_t;

typedef struct
{
    nrfy_uarte_buffer_t curr;
    nrfy_uarte_buffer_t next;
    nrfy_uarte_buffer_t cache;
    size_t              off;
    int                 amount;
} uarte_tx_data_t;

typedef struct
{
    void                     * p_context;
    nrfx_uarte_event_handler_t handler;
    uarte_rx_data_t            rx;
    uarte_tx_data_t            tx;
    nrfx_drv_state_t           state;
    nrfx_atomic_t              flags;
} uarte_control_block_t;

static uarte_control_block_t m_cb[NRFX_UARTE_ENABLED_COUNT];

static const uint32_t rx_int_mask = NRF_UARTE_INT_ERROR_MASK |
                                    NRF_UARTE_INT_ENDRX_MASK |
                                    NRF_UARTE_INT_RXTO_MASK |
                                    NRF_UARTE_INT_RXSTARTED_MASK;

static void apply_workaround_for_enable_anomaly(nrfx_uarte_t const * p_instance);

static void uarte_configure(nrfx_uarte_t        const * p_instance,
                            nrfx_uarte_config_t const * p_config)
{
    uarte_control_block_t * p_cb = &m_cb[p_instance->drv_inst_idx];

    if (/*!NRFX_IS_ENABLED(NRFX_UARTE_CONFIG_SKIP_GPIO_CONFIG) &&*/
        (p_config->skip_gpio_cfg == false))
    {
        p_cb->flags |= UARTE_FLAG_GPIO_UNINIT;
        if (p_config->txd_pin != NRF_UARTE_PSEL_DISCONNECTED)
        {
            nrfy_gpio_pin_set(p_config->txd_pin);
            nrfy_gpio_cfg_output(p_config->txd_pin);
#if NRF_GPIO_HAS_CLOCKPIN && defined(NRF_UARTE_CLOCKPIN_TXD_NEEDED)
            nrfy_gpio_pin_clock_set(p_config->txd_pin, true);
#endif
        }
        if (p_config->rxd_pin != NRF_UARTE_PSEL_DISCONNECTED)
        {
            nrfy_gpio_cfg_input(p_config->rxd_pin, NRF_GPIO_PIN_NOPULL);
        }
    }

    if (/*!NRFX_IS_ENABLED(NRFX_UARTE_CONFIG_SKIP_GPIO_CONFIG) &&*/
        (p_config->config.hwfc == NRF_UARTE_HWFC_ENABLED) && (!p_config->skip_gpio_cfg))
    {
        p_cb->flags |= UARTE_FLAG_HWFC_PINS;
        if (p_config->cts_pin != NRF_UARTE_PSEL_DISCONNECTED)
        {
            nrfy_gpio_cfg_input(p_config->cts_pin, NRF_GPIO_PIN_NOPULL);
        }
        if (p_config->rts_pin != NRF_UARTE_PSEL_DISCONNECTED)
        {
            nrfy_gpio_pin_set(p_config->rts_pin);
            nrfy_gpio_cfg_output(p_config->rts_pin);
#if NRF_GPIO_HAS_CLOCKPIN && defined(NRF_UARTE_CLOCKPIN_RTS_NEEDED)
                nrfy_gpio_pin_clock_set(p_config->rts_pin, true);
#endif
        }
    }

    if (/*!NRFX_IS_ENABLED(NRFX_UARTE_CONFIG_SKIP_PSEL_CONFIG) && */!p_config->skip_psel_cfg)
    {
        p_cb->flags |= UARTE_FLAG_PSEL_UNINIT;
    }

    nrfy_uarte_config_t nrfy_config =
    {
        .pins =
        {
            .txd_pin = p_config->txd_pin,
            .rxd_pin = p_config->rxd_pin,
            .rts_pin = p_config->rts_pin,
            .cts_pin = p_config->cts_pin
        },
        .skip_psel_cfg = /*NRFX_IS_ENABLED(NRFX_UARTE_CONFIG_SKIP_PSEL_CONFIG) ?
            true :*/ p_config->skip_psel_cfg
    };
    nrfy_config.config = p_config->config;

#if defined(LUMOS_XXAA)
    uint32_t base_frequency = NRF_UARTE_BASE_FREQUENCY_GET(p_instance->p_reg);
    if (base_frequency != NRF_UARTE_BASE_FREQUENCY_16MHZ)
    {
        uint32_t baudrate_factor = base_frequency / NRF_UARTE_BASE_FREQUENCY_16MHZ;
        nrfy_config.baudrate =
            (nrf_uarte_baudrate_t)((uint32_t)p_config->baudrate / baudrate_factor);
    }
    else
#endif
    {
        nrfy_config.baudrate = p_config->baudrate;
    }

    nrfy_uarte_periph_configure(p_instance->p_reg, &nrfy_config);

    apply_workaround_for_enable_anomaly(p_instance);

    nrfy_uarte_int_init(p_instance->p_reg,
                        NRFY_EVENT_TO_INT_BITMASK(NRF_UARTE_EVENT_ENDRX) |
                        NRFY_EVENT_TO_INT_BITMASK(NRF_UARTE_EVENT_ENDTX) |
                        NRFY_EVENT_TO_INT_BITMASK(NRF_UARTE_EVENT_ERROR) |
                        NRFY_EVENT_TO_INT_BITMASK(NRF_UARTE_EVENT_RXTO)  |
                        NRFY_EVENT_TO_INT_BITMASK(NRF_UARTE_EVENT_TXSTOPPED),
                        p_config->interrupt_priority,
                        false);
}

static void pins_to_default(nrfx_uarte_t const * p_instance)
{
    uarte_control_block_t const * p_cb = &m_cb[p_instance->drv_inst_idx];
    nrfy_uarte_pins_t pins;

    // Need to read pins before they are reset.
    //if (!NRFX_IS_ENABLED(NRFX_UARTE_CONFIG_SKIP_GPIO_CONFIG))
    {
        nrfy_uarte_pins_get(p_instance->p_reg, &pins);
    }

    // Reset pins to default states.
    if (/*!NRFX_IS_ENABLED(NRFX_UARTE_CONFIG_SKIP_PSEL_CONFIG) &&*/
        (p_cb->flags & UARTE_FLAG_PSEL_UNINIT))
    {
        nrfy_uarte_pins_disconnect(p_instance->p_reg);
    }

    if (1/*!NRFX_IS_ENABLED(NRFX_UARTE_CONFIG_SKIP_GPIO_CONFIG)*/)
    {
        if (p_cb->flags & UARTE_FLAG_GPIO_UNINIT)
        {
            if (pins.txd_pin != NRF_UARTE_PSEL_DISCONNECTED)
            {
                nrfy_gpio_cfg_default(pins.txd_pin);
            }
            if (pins.rxd_pin != NRF_UARTE_PSEL_DISCONNECTED)
            {
                nrfy_gpio_cfg_default(pins.rxd_pin);
            }

            if (p_cb->flags & UARTE_FLAG_HWFC_PINS)
            {
                if (pins.cts_pin != NRF_UARTE_PSEL_DISCONNECTED)
                {
                    nrfy_gpio_cfg_default(pins.cts_pin);
                }
                if (pins.rts_pin != NRF_UARTE_PSEL_DISCONNECTED)
                {
                    nrfy_gpio_cfg_default(pins.rts_pin);
                }
            }
        }
    }
}

static void apply_workaround_for_enable_anomaly(nrfx_uarte_t const * p_instance)
{
#if defined(NRF53_SERIES) || defined(NRF91_SERIES)
    // Apply workaround for anomalies:
    // - nRF91 - anomaly 23
    // - nRF53 - anomaly 44
    volatile uint32_t const * rxenable_reg =
        (volatile uint32_t *)(((uint32_t)p_instance->p_reg) + 0x564);
    volatile uint32_t const * txenable_reg =
        (volatile uint32_t *)(((uint32_t)p_instance->p_reg) + 0x568);

    if (*txenable_reg == 1)
    {
        nrfy_uarte_task_trigger(p_instance->p_reg, NRF_UARTE_TASK_STOPTX);
    }

    if (*rxenable_reg == 1)
    {
        nrfy_uarte_enable(p_instance->p_reg);
        nrfy_uarte_task_trigger(p_instance->p_reg, NRF_UARTE_TASK_STOPRX);

        bool workaround_succeded;
        // The UARTE is able to receive up to four bytes after the STOPRX task has been triggered.
        // On lowest supported baud rate (1200 baud), with parity bit and two stop bits configured
        // (resulting in 12 bits per data byte sent), this may take up to 40 ms.
        NRFX_WAIT_FOR(*rxenable_reg == 0, 40000, 1, workaround_succeded);
        if (!workaround_succeded)
        {
            NRFX_LOG_ERROR("Failed to apply workaround for instance with base address: %p.",
                           (void *)p_instance->p_reg);
        }

        (void)nrfy_uarte_errorsrc_get_and_clear(p_instance->p_reg);
        nrfy_uarte_disable(p_instance->p_reg);
    }
#else
    (void)(p_instance);
#endif // defined(NRF53_SERIES) || defined(NRF91_SERIES)
}

static uint32_t uarte_int_lock(NRF_UARTE_Type * p_uarte)
{
    uint32_t int_enabled = nrfy_uarte_int_enable_check(p_uarte, UINT32_MAX);

    nrfy_uarte_int_disable(p_uarte, int_enabled);

    return int_enabled;
}

static void uarte_int_unlock(NRF_UARTE_Type * p_uarte, uint32_t int_mask)
{
    nrfy_uarte_int_enable(p_uarte, int_mask);
}

/* Function returns true if new transfer can be started. Since TXSTOPPED
 * (and ENDTX) is cleared before triggering new transfer, TX is ready for new
 * transfer if any event is set.
 *
 * @param stop_on_end TXSTOP is PPIed with ENDTX. Check only TXSTOPPED.
 */
static bool is_tx_ready(NRF_UARTE_Type * p_uarte, bool stop_on_end)
{
    return nrfy_uarte_event_check(p_uarte, NRF_UARTE_EVENT_TXSTOPPED) ||
        (!stop_on_end && nrfy_uarte_event_check(p_uarte, NRF_UARTE_EVENT_ENDTX));
}

static bool prepare_rx(NRF_UARTE_Type * p_uarte)
{
    /**
     * Stop any currently running RX operations. This can occur when a
     * bootloader sets up the UART hardware and does not clean it up
     * before jumping to the next application.
     */
    if (nrfy_uarte_event_check(p_uarte, NRF_UARTE_EVENT_RXSTARTED))
    {
        bool res;

        nrfy_uarte_enable(p_uarte);
        nrfy_uarte_task_trigger(p_uarte, NRF_UARTE_TASK_STOPRX);

        NRFX_WAIT_FOR(nrfy_uarte_event_check(p_uarte, NRF_UARTE_EVENT_RXTO) ||
                      nrfy_uarte_event_check(p_uarte, NRF_UARTE_EVENT_ERROR), 100, 10, res);

        nrfy_uarte_event_clear(p_uarte, NRF_UARTE_EVENT_RXSTARTED);
        nrfy_uarte_event_clear(p_uarte, NRF_UARTE_EVENT_ENDRX);
        nrfy_uarte_event_clear(p_uarte, NRF_UARTE_EVENT_RXTO);
        nrfy_uarte_disable(p_uarte);

        return res;
    }
    return true;
}

static bool prepare_tx(NRF_UARTE_Type * p_uarte, bool stop_on_end)
{
    (void)stop_on_end;
    uint8_t dummy = 0;

    nrfy_uarte_enable(p_uarte);

    /* Set TXSTOPPED event by requesting fake (zero-length) transfer.
     * Pointer to RAM variable (data->tx_buffer) is set because otherwise
     * such operation may result in HardFault or RAM corruption.
     */
    nrfy_uarte_tx_buffer_set(p_uarte, &dummy, 0);
    nrfy_uarte_task_trigger(p_uarte, NRF_UARTE_TASK_STARTTX);

    /* switch off transmitter to save an energy */
    nrfy_uarte_task_trigger(p_uarte, NRF_UARTE_TASK_STOPTX);

    bool res;

    NRFX_WAIT_FOR(is_tx_ready(p_uarte, true), 10, 1, res);

    if (!res)
    {
        return false;
    }

    nrfy_uarte_event_clear(p_uarte, NRF_UARTE_EVENT_ENDTX);
    nrfy_uarte_disable(p_uarte);
    return true;
}

#define NRFX_ERROR_ALREADY NRFX_ERROR_INVALID_STATE

nrfx_err_t nrfx_uarte_init(nrfx_uarte_t const *        p_instance,
                           nrfx_uarte_config_t const * p_config,
                           nrfx_uarte_event_handler_t  event_handler)
{
    NRFX_ASSERT(p_config);
    uint32_t inst_idx = p_instance->drv_inst_idx;
    uarte_control_block_t * p_cb = &m_cb[inst_idx];
    nrfx_err_t err_code;

    if (p_cb->state != NRFX_DRV_STATE_UNINITIALIZED)
    {
/*#if NRFX_API_VER_AT_LEAST(3, 2, 0)
        err_code = NRFX_ERROR_ALREADY;
#else*/
        err_code = NRFX_ERROR_INVALID_STATE;
/*#endif*/
        NRFX_LOG_WARNING("Function: %s, error code: %s.",
                         __func__,
                         NRFX_LOG_ERROR_STRING_GET(err_code));
        return err_code;
    }

    memset(p_cb, 0, sizeof(uarte_control_block_t));

    p_cb->p_context = p_config->p_context;
    p_cb->tx.cache.p_buffer = p_config->tx_cache.p_buffer;
    if (p_config->tx_cache.length == 0)
    {
        p_cb->tx.cache.length = 0;
    }
    else
    {
        p_cb->tx.cache.length = p_config->tx_cache.length - 1;
    }
    if (p_config->rx_cache.length >= UARTE_HW_RX_FIFO_SIZE)
    {
        p_cb->rx.flush.p_buffer = p_config->rx_cache.p_buffer;
        p_cb->rx.flush.length = 0;
        if (RX_CACHE_SUPPORTED && p_config->p_rx_cache_scratch)
        {
            if (p_config->rx_cache.length < (UARTE_HW_RX_FIFO_SIZE + MIN_RX_CACHE_SIZE))
            {
                return NRFX_ERROR_INVALID_PARAM;
            }
            size_t cache_len = p_config->rx_cache.length - UARTE_HW_RX_FIFO_SIZE;
            size_t buf_len = cache_len / 2;

            p_cb->rx.p_cache = p_config->p_rx_cache_scratch;

            memset(p_cb->rx.p_cache, 0, sizeof(*p_cb->rx.p_cache));
            // Split provided cache space into two equal buffers. Cache buffers can overlap with
            // flush buffer as they are not used simultaneously.
            p_cb->rx.p_cache->cache_len = buf_len;
            p_cb->rx.p_cache->cache[0].p_buffer =
                &p_config->rx_cache.p_buffer[UARTE_HW_RX_FIFO_SIZE];
            p_cb->rx.p_cache->cache[1].p_buffer =
                &p_config->rx_cache.p_buffer[UARTE_HW_RX_FIFO_SIZE + buf_len];
        }
    }

#if NRF_UARTE_HAS_ENDTX_STOPTX_SHORT
    uint32_t tx_int_mask = 0;

    nrfy_uarte_shorts_enable(p_instance->p_reg, NRF_UARTE_SHORT_ENDTX_STOPTX);
#else
    uint32_t tx_int_mask = (!event_handler || p_config->tx_stop_on_end) ?
                               0 : NRF_UARTE_INT_ENDTX_MASK;

    if (p_config->tx_stop_on_end)
    {
        p_cb->flags |= UARTE_FLAG_TX_STOP_ON_END;
    }
#endif

    p_cb->handler = event_handler;
    p_cb->state   = NRFX_DRV_STATE_INITIALIZED;

    // Handle case when other user (e.g. bootloader) left RX in active state.
    if (!prepare_rx(p_instance->p_reg))
    {
#if NRFX_CHECK(NRFX_PRS_ENABLED)
        nrfx_prs_release(p_instance->p_reg);
#endif
        return NRFX_ERROR_INTERNAL;
    }

    uarte_configure(p_instance, p_config);

    if (!prepare_tx(p_instance->p_reg, p_config->tx_stop_on_end))
    {
#if NRFX_CHECK(NRFX_PRS_ENABLED)
        nrfx_prs_release(p_instance->p_reg);
#endif
        return NRFX_ERROR_INTERNAL;
    }

    uint32_t int_mask = tx_int_mask | ((event_handler) ? rx_int_mask : 0);

    nrfy_uarte_int_enable(p_instance->p_reg, int_mask);

    return NRFX_SUCCESS;
}

nrfx_err_t nrfx_uarte_reconfigure(nrfx_uarte_t const *        p_instance,
                                  nrfx_uarte_config_t const * p_config)
{
    NRFX_ASSERT(p_config);

    uarte_control_block_t * p_cb = &m_cb[p_instance->drv_inst_idx];

    if (p_cb->state == NRFX_DRV_STATE_UNINITIALIZED)
    {
        return NRFX_ERROR_INVALID_STATE;
    }
    if (nrfx_uarte_tx_in_progress(p_instance))
    {
        return NRFX_ERROR_BUSY;
    }
    nrfy_uarte_disable(p_instance->p_reg);
    if (p_cb->handler)
    {
        p_cb->p_context = p_config->p_context;
    }
    uarte_configure(p_instance, p_config);

    if (prepare_tx(p_instance->p_reg, p_cb->flags & UARTE_FLAG_TX_STOP_ON_END))
    {
        return NRFX_SUCCESS;
    }
    else
    {
        return NRFX_ERROR_INTERNAL;
    }
}

void nrfx_uarte_uninit(nrfx_uarte_t const * p_instance)
{
    nrfx_err_t err;
    uarte_control_block_t * p_cb = &m_cb[p_instance->drv_inst_idx];
    NRF_UARTE_Type * p_uarte = p_instance->p_reg;

    NRFX_ASSERT(p_cb->state != NRFX_DRV_STATE_UNINITIALIZED);

    nrfy_uarte_int_disable(p_uarte,
                           NRF_UARTE_INT_ENDRX_MASK |
                           NRF_UARTE_INT_ENDTX_MASK |
                           NRF_UARTE_INT_ERROR_MASK |
                           NRF_UARTE_INT_RXTO_MASK  |
                           NRF_UARTE_INT_RXSTARTED_MASK  |
                           NRF_UARTE_INT_TXSTOPPED_MASK);
    nrfy_uarte_int_uninit(p_uarte);

#if NRF_UARTE_HAS_ENDTX_STOPTX_SHORT
    nrfy_uarte_shorts_disable(p_uarte, NRF_UARTE_SHORT_ENDTX_STOPTX);
#endif

#if NRFX_CHECK(NRFX_PRS_ENABLED)
    nrfx_prs_release(p_uarte);
#endif

    err = nrfx_uarte_rx_abort(p_instance, true, true);
    (void)err;

    err = nrfx_uarte_tx_abort(p_instance, true);
    (void)err;

    pins_to_default(p_instance);

    p_cb->flags   = 0;
    p_cb->state   = NRFX_DRV_STATE_UNINITIALIZED;
    p_cb->handler = NULL;
    NRFX_LOG_INFO("Instance uninitialized: %d.", p_instance->drv_inst_idx);
}

bool nrfx_uarte_init_check(nrfx_uarte_t const * p_instance)
{
    uarte_control_block_t * p_cb = &m_cb[p_instance->drv_inst_idx];

    return (p_cb->state != NRFX_DRV_STATE_UNINITIALIZED);
}

static void tx_start(NRF_UARTE_Type * p_uarte, const uint8_t *buf, size_t len, bool en_int)
{
    nrfy_uarte_tx_buffer_set(p_uarte, buf, len);
    nrfy_uarte_event_clear(p_uarte, NRF_UARTE_EVENT_ENDTX);
    nrfy_uarte_event_clear(p_uarte, NRF_UARTE_EVENT_TXSTOPPED);
    nrfy_uarte_event_clear(p_uarte, NRF_UARTE_EVENT_TXSTARTED);

    nrfy_uarte_enable(p_uarte);

    if (en_int)
    {
        nrfy_uarte_int_enable(p_uarte, NRF_UARTE_INT_TXSTOPPED_MASK);
    }

    nrfy_uarte_task_trigger(p_uarte, NRF_UARTE_TASK_STARTTX);
}

static bool is_rx_active(uarte_control_block_t * p_cb)
{
    return (p_cb->flags & UARTE_FLAG_RX_ENABLED) ? true : false;
}

/* Must be called with interrupts locked. */
static void disable_hw_from_tx(NRF_UARTE_Type *        p_uarte,
                                  uarte_control_block_t * p_cb)
{
    if (nrfy_uarte_event_check(p_uarte, NRF_UARTE_EVENT_TXSTOPPED) && !is_rx_active(p_cb))
    {
        nrfy_uarte_disable(p_uarte);
    }
}

/* Block until transfer is completed. Disable UARTE if RX is not active. */
static void block_on_tx(NRF_UARTE_Type *        p_uarte,
                        uarte_control_block_t * p_cb)
{
    bool stop_on_end = p_cb->flags & UARTE_FLAG_TX_STOP_ON_END;
    bool do_disable = true;

    while (!is_tx_ready(p_uarte, stop_on_end))
    {}

    NRFX_CRITICAL_SECTION_ENTER();

    if (!stop_on_end)
    {
        if (nrfy_uarte_event_check(p_uarte, NRF_UARTE_EVENT_ENDTX))
        {
            nrfy_uarte_task_trigger(p_uarte, NRF_UARTE_TASK_STOPTX);
            while (!is_tx_ready(p_uarte, true))
            {}
        }
        else
        {
            do_disable = false;
        }
    }
    else if (!nrfy_uarte_event_check(p_uarte, NRF_UARTE_EVENT_TXSTOPPED))
    {
        do_disable = false;
    }

    if (do_disable)
    {
        disable_hw_from_tx(p_uarte, p_cb);
    }
    NRFX_CRITICAL_SECTION_EXIT();
}

static nrfx_err_t wait_for_endtx(NRF_UARTE_Type * p_uarte,
                                 uint8_t const *  p_buf,
                                 uint32_t         length,
                                 bool             stop_on_end)
{
    const uint8_t * p_tx;
    bool ready;
    uint32_t amount;
    nrfx_err_t err;

    do {
            // Pend until TX is ready again or TX buffer pointer is replaced with new
            // address which indicates that current context got preempted and new
            // request was started from a higher priority context.
            ready = is_tx_ready(p_uarte, stop_on_end);
            amount = nrfy_uarte_tx_amount_get(p_uarte);
            p_tx = nrfy_uarte_tx_buffer_get(p_uarte);

            if (ready || (p_tx != p_buf))
            {
                break;
            }
#if defined(CONFIG_SOC_SERIES_BSIM_NRFXX)
            nrfx_coredep_delay_us(3);
#endif
    } while (true);

    // Check if transfer got aborted. Note that aborted transfer can only be
    // detected if new transfer is not started.
    err = ((p_tx == p_buf) && (length > amount)) ? NRFX_ERROR_FORBIDDEN : NRFX_SUCCESS;

    if ((err == NRFX_SUCCESS) && !stop_on_end)
    {
        nrfy_uarte_task_trigger(p_uarte, NRF_UARTE_TASK_STOPTX);
    }

    return err;
}

static nrfx_err_t poll_out(nrfx_uarte_t const * p_instance, uint8_t const * p_byte, bool early_ret)
{
    uarte_control_block_t * p_cb = &m_cb[p_instance->drv_inst_idx];
    NRF_UARTE_Type * p_uarte = p_instance->p_reg;
    NRFX_ASSERT(p_cb->state == NRFX_DRV_STATE_INITIALIZED);
    bool use_cache;
    nrfx_err_t err = NRFX_ERROR_BUSY;
    uint8_t const * p_buf;

    if (p_cb->tx.cache.p_buffer == NULL)
    {
#if 0
        if (!nrf_dma_accessible_check(p_uarte, p_byte))
        {
            return NRFX_ERROR_INVALID_PARAM;
        }
#endif
        use_cache = false;
    }
    else
    {
        use_cache = true;
    }

    NRFX_CRITICAL_SECTION_ENTER();
    bool tx_ready = is_tx_ready(p_uarte, p_cb->flags & UARTE_FLAG_TX_STOP_ON_END);

    if (tx_ready)
    {
        if (p_cb->tx.amount < 0)
        {
            p_cb->tx.amount = (int)nrfy_uarte_tx_amount_get(p_uarte);
        }
        if (use_cache)
        {
            // Last byte in cache buffer is used for polling out.
            p_cb->tx.cache.p_buffer[p_cb->tx.cache.length] = *p_byte;
            p_buf = &p_cb->tx.cache.p_buffer[p_cb->tx.cache.length];
        }
        else
        {
            p_buf = p_byte;
        }
        tx_start(p_uarte, p_buf, 1, early_ret);
        err = NRFX_SUCCESS;
    }
    NRFX_CRITICAL_SECTION_EXIT();

    if ((err == NRFX_SUCCESS) && !early_ret)
    {
        err = wait_for_endtx(p_uarte, p_buf, 1, p_cb->flags & UARTE_FLAG_TX_STOP_ON_END);

        NRFX_CRITICAL_SECTION_ENTER();
        disable_hw_from_tx(p_uarte, p_cb);
        NRFX_CRITICAL_SECTION_EXIT();
    }

    return err;
}

static bool tx_prepare_start(NRF_UARTE_Type *        p_uarte,
                             uarte_control_block_t * p_cb,
                             bool                    use_cache)
{
    if (!is_tx_ready(p_uarte, p_cb->flags & UARTE_FLAG_TX_STOP_ON_END))
    {
        // Transmitter is busy, set pending flag.
        p_cb->flags |= UARTE_FLAG_TX_PENDING;
        nrfy_uarte_int_enable(p_uarte, NRF_UARTE_INT_TXSTOPPED_MASK);
        return false;
    }

    uint8_t const * p_buf;
    uint32_t xfer_len;

    if (use_cache)
    {
        uint32_t chunk_len = NRFX_MIN(p_cb->tx.cache.length, p_cb->tx.curr.length - p_cb->tx.off);

        memcpy(p_cb->tx.cache.p_buffer, &p_cb->tx.curr.p_buffer[p_cb->tx.off], chunk_len);
        p_buf = p_cb->tx.cache.p_buffer;
        xfer_len = chunk_len;
    }
    else
    {
        p_buf = p_cb->tx.curr.p_buffer;
        xfer_len = p_cb->tx.curr.length;
    }
    p_cb->tx.amount = -1;
    tx_start(p_uarte, p_buf, xfer_len, true);

    return true;
}

static nrfx_err_t blocking_tx(nrfx_uarte_t const * p_instance,
                              uint8_t const *      p_buffer,
                              uint32_t             length,
                              uint32_t             flags)
{
    uarte_control_block_t * p_cb = &m_cb[p_instance->drv_inst_idx];
    bool early_ret = p_cb->handler && (flags & NRFX_UARTE_TX_EARLY_RETURN);
    nrfx_err_t err = NRFX_SUCCESS;

    if ((early_ret && !p_cb->tx.cache.p_buffer) || (p_cb->flags & UARTE_FLAG_TX_LINKED))
    {
        return NRFX_ERROR_FORBIDDEN;
    }

    for (uint32_t i = 0; i < length; i++)
    {
        do {
            err = poll_out(p_instance, &p_buffer[i], early_ret);
            if (err == NRFX_SUCCESS)
            {
                break;
            }
            if (err != NRFX_ERROR_BUSY)
            {
                // TX aborted or other error
                return err;
            }
#if defined(CONFIG_SOC_SERIES_BSIM_NRFXX)
            nrfx_coredep_delay_us(3);
#endif
        } while (true);

        if (!p_cb->handler && (p_cb->flags & UARTE_FLAG_TX_ABORTED))
        {
            NRFX_ATOMIC_FETCH_AND(&p_cb->flags, ~UARTE_FLAG_TX_ABORTED);
            err = NRFX_ERROR_FORBIDDEN;
            break;
        }
    }
    return err;
}

nrfx_err_t nrfx_uarte_tx(nrfx_uarte_t const * p_instance,
                         uint8_t const *      p_data,
                         size_t               length,
                         uint32_t             flags)
{
    uarte_control_block_t * p_cb = &m_cb[p_instance->drv_inst_idx];
    NRF_UARTE_Type * p_uarte = p_instance->p_reg;

    NRFX_ASSERT(p_cb->state == NRFX_DRV_STATE_INITIALIZED);
    //NRFX_ASSERT(UARTE_LENGTH_VALIDATE(p_instance->drv_inst_idx, length));
    NRFX_ASSERT(p_data);
    NRFX_ASSERT(length > 0);

    nrfx_err_t err_code = NRFX_SUCCESS;
    bool use_cache;

    if (length == 0)
    {
        return NRFX_ERROR_INVALID_LENGTH;
    }

    if (!p_cb->handler && (flags == 0))
    {
        if (p_cb->tx.curr.length == 0)
        {
            p_cb->tx.curr.length = length;
        }
        else
        {
            return NRFX_ERROR_BUSY;
        }

        err_code = blocking_tx(p_instance, p_data, length, 0);
        p_cb->tx.curr.length = 0;
        return err_code;
    }

    // Handle case when transfer is blocking.
    if (flags & (NRFX_UARTE_TX_EARLY_RETURN | NRFX_UARTE_TX_BLOCKING))
    {
        return blocking_tx(p_instance, p_data, length, flags);
    }

    // EasyDMA requires that transfer buffers are placed in DataRAM,
    // signal error if the are not.
    if (!nrf_dma_accessible_check(p_uarte, p_data))
    {
        if (!p_cb->tx.cache.p_buffer ||
            (/*NRFX_IS_ENABLED(NRFX_UARTE_CONFIG_TX_LINK) && */(flags & NRFX_UARTE_TX_LINK)))
        {
            err_code = NRFX_ERROR_INVALID_ADDR;
        }

        use_cache = true;
    }
    else
    {
        if ((p_cb->flags & UARTE_FLAG_TX_STOP_ON_END) &&
            (/*NRFX_IS_ENABLED(NRFX_UARTE_CONFIG_TX_LINK) &&*/ (flags & NRFX_UARTE_TX_LINK)))
        {
            // STOPTX on ENDTX connection cannot be used together with linking.
            err_code = NRFX_ERROR_FORBIDDEN;
        }

        use_cache = false;
    }

    if (err_code != NRFX_SUCCESS)
    {
        return err_code;
    }

    NRFX_CRITICAL_SECTION_ENTER();
    if (p_cb->tx.curr.length == 0)
    {
        p_cb->tx.curr.length = length;

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif

        p_cb->tx.curr.p_buffer = (uint8_t *)p_data;

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

        if (use_cache)
        {
            p_cb->flags |= UARTE_FLAG_TX_USE_CACHE;
        }

        tx_prepare_start(p_uarte, p_cb, use_cache);
    }
    else if (/*NRFX_IS_ENABLED(NRFX_UARTE_CONFIG_TX_LINK) &&*/
             !p_cb->tx.next.length && (flags & NRFX_UARTE_TX_LINK))
    {
        if (nrf_dma_accessible_check(p_uarte, p_cb->tx.curr.p_buffer))
        {
            bool res;

            p_cb->flags |= UARTE_FLAG_TX_LINKED;

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif

            p_cb->tx.next.p_buffer = (uint8_t *)p_data;

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

            p_cb->tx.next.length = length;
            NRFX_WAIT_FOR(nrfy_uarte_event_check(p_uarte, NRF_UARTE_EVENT_TXSTARTED), 10, 1, res);
            if (res)
            {
#if NRF_UARTE_HAS_ENDTX_STOPTX_SHORT
                nrfy_uarte_int_enable(p_instance->p_reg, NRF_UARTE_INT_ENDTX_MASK);
                nrfy_uarte_shorts_disable(p_uarte, NRF_UARTE_SHORT_ENDTX_STOPTX);
#endif
                nrfy_uarte_event_clear(p_uarte, NRF_UARTE_EVENT_TXSTARTED);
                nrfy_uarte_tx_buffer_set(p_uarte, p_data, length);
                err_code = NRFX_SUCCESS;
            }
            else
            {
                err_code = NRFX_ERROR_INTERNAL;
            }
        }
        else
        {
            err_code = NRFX_ERROR_FORBIDDEN;
        }
    }
    else
    {
        err_code = NRFX_ERROR_BUSY;
    }
    NRFX_CRITICAL_SECTION_EXIT();

    return err_code;
}

bool nrfx_uarte_tx_in_progress(nrfx_uarte_t const * p_instance)
{
    NRFX_ASSERT(m_cb[p_instance->drv_inst_idx].state != NRFX_DRV_STATE_UNINITIALIZED);

    return (m_cb[p_instance->drv_inst_idx].tx.curr.length != 0);
}

nrfx_err_t nrfx_uarte_tx_abort(nrfx_uarte_t const * p_instance, bool sync)
{
    uarte_control_block_t * p_cb = &m_cb[p_instance->drv_inst_idx];
    NRF_UARTE_Type * p_uarte = p_instance->p_reg;
    uint32_t int_mask;

    int_mask = uarte_int_lock(p_uarte);
    if (p_cb->tx.curr.length == 0)
    {
        uarte_int_unlock(p_uarte, int_mask);
        return NRFX_ERROR_INVALID_STATE;
    }

    NRFX_ATOMIC_FETCH_OR(&p_cb->flags, UARTE_FLAG_TX_ABORTED);

    nrfy_uarte_task_trigger(p_uarte, NRF_UARTE_TASK_STOPTX);

    if (sync)
    {
        block_on_tx(p_uarte, p_cb);
        nrfy_uarte_event_clear(p_uarte, NRF_UARTE_EVENT_ENDTX);
        p_cb->tx.curr.length = 0;
    }

    uarte_int_unlock(p_uarte, int_mask);

    NRFX_LOG_INFO("TX transaction aborted.");

    return NRFX_SUCCESS;
}

static void user_handler(uarte_control_block_t * p_cb, nrfx_uarte_evt_type_t type)
{
    nrfx_uarte_event_t event = {
        .type = type
    };

    p_cb->handler(&event, p_cb->p_context);
}

static void user_handler_on_rx_disabled(uarte_control_block_t * p_cb, size_t flush_cnt)
{
    nrfx_uarte_event_t event = {
        .type = NRFX_UARTE_EVT_RX_DISABLED,
        .data = {
            .rx_disabled = {
                .flush_cnt = flush_cnt
            }
        }
    };

    p_cb->handler(&event, p_cb->p_context);
}

static void user_handler_on_error(NRF_UARTE_Type * p_uarte, uarte_control_block_t * p_cb)
{
    nrfx_uarte_event_t event = {
        .type = NRFX_UARTE_EVT_ERROR,
        .data = {
            .error = {
                .error_mask = nrfy_uarte_errorsrc_get_and_clear(p_uarte)
            }
        }
    };

    p_cb->handler(&event, p_cb->p_context);
}

static void user_handler_on_rx_done(uarte_control_block_t * p_cb,
                                    const uint8_t *         p_data,
                                    size_t                  len)
{
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif

    nrfx_uarte_event_t event = {
        .type = NRFX_UARTE_EVT_RX_DONE,
        .data = {
            .rx = {
                .p_buffer = (uint8_t *)p_data,
                .length = len
            }
        }
    };

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

    p_cb->handler(&event, p_cb->p_context);
}

static void user_handler_on_tx_done(uarte_control_block_t * p_cb,
                                    const uint8_t *         p_data,
                                    size_t                  len,
                                    bool                    abort)
{
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif

    nrfx_uarte_event_t event = {
        .type = NRFX_UARTE_EVT_TX_DONE,
        .data = {
            .tx = {
                .p_buffer = (uint8_t *)p_data,
                .length = len,
                .flags = abort ? NRFX_UARTE_TX_DONE_ABORTED : 0
            }
        }
    };

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

    p_cb->handler(&event, p_cb->p_context);
}

static void release_rx(uarte_control_block_t * p_cb)
{
    /* Clear all RX flags. */
    NRFX_ATOMIC_FETCH_AND(&p_cb->flags, ~UARTE_RX_FLAGS);
}

static bool is_tx_active(NRF_UARTE_Type * p_uarte)
{
    return !nrfy_uarte_event_check(p_uarte, NRF_UARTE_EVENT_TXSTOPPED) ||
        nrfy_uarte_int_enable_check(p_uarte, NRF_UARTE_INT_TXSTOPPED_MASK);
}

static void disable_hw_from_rx(NRF_UARTE_Type * p_uarte)
{
    NRFX_CRITICAL_SECTION_ENTER();

    if (!is_tx_active(p_uarte))
    {
        nrfy_uarte_disable(p_uarte);
    }

    NRFX_CRITICAL_SECTION_EXIT();
}

static void on_rx_disabled(NRF_UARTE_Type        * p_uarte,
                           uarte_control_block_t * p_cb,
                           size_t                  flush_cnt)
{
    nrfy_uarte_event_clear(p_uarte, NRF_UARTE_EVENT_RXDRDY);
    nrfy_uarte_shorts_disable(p_uarte, NRF_UARTE_SHORT_ENDRX_STARTRX);
    nrfy_uarte_int_disable(p_uarte, rx_int_mask);
    disable_hw_from_rx(p_uarte);

    p_cb->rx.curr.p_buffer = NULL;
    p_cb->rx.next.p_buffer = NULL;
    release_rx(p_cb);
    user_handler_on_rx_disabled(p_cb, flush_cnt);
}

static void handler_on_rx_done(uarte_control_block_t * p_cb,
                               uint8_t *               p_data,
                               size_t                  len,
                               bool                    abort)
{
    bool cache_used = RX_CACHE_SUPPORTED && (p_cb->flags & UARTE_FLAG_RX_USE_CACHE);
    nrfx_uarte_rx_cache_t * p_cache = p_cb->rx.p_cache;

    if (!cache_used)
    {
        user_handler_on_rx_done(p_cb, p_data, len);
        return;
    }
    else if (!p_cache->user[0].p_buffer)
    {
        return;
    }

    memcpy(&p_cache->user[0].p_buffer[p_cache->received], p_data, len);
    p_cache->received += len;

    bool user_buf_end = p_cache->user[0].length == p_cache->received;

    if (user_buf_end || abort)
    {
        uint8_t *p_buf = p_cache->user[0].p_buffer;
        size_t buf_len = p_cache->received;

        p_cache->received = 0;
        p_cache->user[0] = p_cache->user[1];
        p_cache->user[1] = (nrfy_uarte_buffer_t){ NULL, 0 };
        if (p_cache->user[0].length)
        {
            p_cache->buf_req = true;
        }
        user_handler_on_rx_done(p_cb, p_buf, buf_len);
    }
}

/* Some data may be left in flush buffer. It need to be copied into rx buffer.
 * If flushed data exceeds input buffer rx enabled is terminated.
 * Returns true when flushed did not filled whole user buffer.
 */
static bool rx_flushed_handler(NRF_UARTE_Type * p_uarte, uarte_control_block_t * p_cb)
{
    if (p_cb->rx.flush.length == 0)
    {
        return true;
    }

    if ((uint32_t)p_cb->rx.flush.length >= p_cb->rx.curr.length)
    {
        uint8_t * p_buf = p_cb->rx.curr.p_buffer;
        size_t len = p_cb->rx.curr.length;

        p_cb->rx.curr.p_buffer = NULL;
        p_cb->rx.curr.length = 0;
        memcpy(p_buf, p_cb->rx.flush.p_buffer, len);
        p_cb->rx.flush.length -= len;
        memmove(p_cb->rx.flush.p_buffer, &p_cb->rx.flush.p_buffer[len], p_cb->rx.flush.length);

        if (p_cb->handler)
        {
            bool stop_on_end = p_cb->flags & UARTE_FLAG_RX_STOP_ON_END;

            if (stop_on_end)
            {
                NRFX_ATOMIC_FETCH_OR(&p_cb->flags, UARTE_FLAG_RX_ABORTED);
            }

            handler_on_rx_done(p_cb, p_buf, len, false);
            if (stop_on_end)
            {
                    on_rx_disabled(p_uarte, p_cb, 0);
            }
        }

        return false;
    }
    else
    {
        memcpy(p_cb->rx.curr.p_buffer, p_cb->rx.flush.p_buffer, p_cb->rx.flush.length);
        p_cb->rx.off = p_cb->rx.flush.length;
        p_cb->rx.flush.length = 0;
        NRFX_ATOMIC_FETCH_OR(&p_cb->flags, UARTE_FLAG_RX_FROM_FLUSH);
    }

    return true;
}

nrfx_err_t nrfx_uarte_rx_enable(nrfx_uarte_t const * p_instance, uint32_t flags)
{
    uarte_control_block_t * p_cb = &m_cb[p_instance->drv_inst_idx];
    NRF_UARTE_Type * p_uarte = p_instance->p_reg;
    uint32_t prev_flags;

    prev_flags = NRFX_ATOMIC_FETCH_OR(&p_cb->flags, UARTE_FLAG_RX_ENABLED);
    if (prev_flags & UARTE_FLAG_RX_ENABLED)
    {
        return NRFX_ERROR_BUSY;
    }

    if ((flags & NRFX_UARTE_RX_ENABLE_KEEP_FIFO_CONTENT) && !p_cb->rx.flush.p_buffer)
    {
        return NRFX_ERROR_FORBIDDEN;
    }

    nrfy_uarte_int_disable(p_uarte, rx_int_mask);
    nrfy_uarte_enable(p_uarte);
    nrfy_uarte_event_clear(p_uarte, NRF_UARTE_EVENT_ENDRX);
    nrfy_uarte_event_clear(p_uarte, NRF_UARTE_EVENT_RXSTARTED);
    nrfy_uarte_event_clear(p_uarte, NRF_UARTE_EVENT_RXTO);

    uint32_t rt_flags = (flags & NRFX_UARTE_RX_ENABLE_CONT ? UARTE_FLAG_RX_CONT : 0) |
                        (flags & NRFX_UARTE_RX_ENABLE_STOP_ON_END ? UARTE_FLAG_RX_STOP_ON_END : 0) |
                        (flags & NRFX_UARTE_RX_ENABLE_KEEP_FIFO_CONTENT ?
                                                UARTE_FLAG_RX_KEEP_FIFO_CONTENT : 0);

    NRFX_ATOMIC_FETCH_OR(&p_cb->flags, rt_flags);

    if ((p_cb->rx.curr.p_buffer == NULL) && p_cb->handler)
    {
        user_handler(p_cb, NRFX_UARTE_EVT_RX_BUF_REQUEST);
    }

    // Expecting to get buffer set as a response to the request.
    if (p_cb->rx.curr.p_buffer == NULL)
    {
        // If RX is not active then it means that reception is already stopped.
        // It may happen if RX flush had enough data to fill the user buffer.
        // In that case reception is disabled from the context of RX buffer set function.
        if (!is_rx_active(p_cb))
        {
            return NRFX_SUCCESS;
        }

        release_rx(p_cb);
        return NRFX_ERROR_NO_MEM;
    }

    if (nrfy_uarte_int_enable_check(p_uarte, NRF_UARTE_INT_RXDRDY_MASK) && p_cb->handler &&
        (p_cb->flags & UARTE_FLAG_RX_FROM_FLUSH))
    {
        NRFX_ATOMIC_FETCH_AND(&p_cb->flags, ~UARTE_FLAG_RX_FROM_FLUSH);
        user_handler(p_cb, NRFX_UARTE_EVT_RX_BYTE);
    }

    /* Check if instance is still enabled. It might get disabled at some point. */
    if (p_cb->flags & UARTE_FLAG_RX_ENABLED)
    {
        if (!nrfy_uarte_event_check(p_uarte, NRF_UARTE_EVENT_RXSTARTED))
        {
            /* Manually trigger RX if it was not yet started. */
            nrfy_uarte_task_trigger(p_uarte, NRF_UARTE_TASK_STARTRX);
        }

        if (p_cb->handler)
        {
            nrfy_uarte_int_enable(p_uarte, rx_int_mask);
        }
    }

    return NRFX_SUCCESS;
}

static nrfx_err_t rx_buffer_set(NRF_UARTE_Type *        p_uarte,
                                uarte_control_block_t * p_cb,
                                uint8_t *               p_data,
                                size_t                  length)
{
    nrfx_err_t err_code = NRFX_SUCCESS;

    if (p_cb->rx.curr.p_buffer == NULL ||
        (!p_cb->handler && nrfy_uarte_event_check(p_uarte, NRF_UARTE_EVENT_ENDRX)))
    {
        if (p_cb->rx.curr.p_buffer)
        {
            nrfy_uarte_event_clear(p_uarte, NRF_UARTE_EVENT_ENDRX);
        }

        p_cb->rx.curr.p_buffer = p_data;
        p_cb->rx.curr.length = length;

        if (rx_flushed_handler(p_uarte, p_cb))
        {
            nrfy_uarte_rx_buffer_set(p_uarte,
                                    &p_cb->rx.curr.p_buffer[p_cb->rx.off],
                                    p_cb->rx.curr.length - p_cb->rx.off);
            if (p_cb->flags & UARTE_FLAG_RX_ENABLED)
            {
                NRFX_ATOMIC_FETCH_AND(&p_cb->flags, ~UARTE_FLAG_RX_ABORTED);
                nrfy_uarte_task_trigger(p_uarte, NRF_UARTE_TASK_STARTRX);
                if (nrfy_uarte_event_check(p_uarte, NRF_UARTE_EVENT_RXTO))
                {
                    nrfy_uarte_event_clear(p_uarte, NRF_UARTE_EVENT_RXTO);
                }
            }
        }
    }
    else if (p_cb->rx.next.p_buffer == NULL)
    {
        p_cb->rx.next.p_buffer = p_data;
        p_cb->rx.next.length = length;

        nrfy_uarte_rx_buffer_set(p_uarte, p_data, length);
        if (p_cb->flags & UARTE_FLAG_RX_CONT)
        {
            nrfy_uarte_shorts_enable(p_uarte, NRF_UARTE_SHORT_ENDRX_STARTRX);
        }
    }
    else
    {
        err_code = NRFX_ERROR_BUSY;
    }

    return err_code;
}

static size_t get_curr_cache_buf_len(size_t cache_len, size_t len, size_t curr)
{
    if (len == 0)
    {
        return 0;
    }

    size_t rem = len - curr;

    return (rem > cache_len) ? NRFX_MIN(cache_len, rem / 2) : rem;
}

static size_t get_cache_buf_len(nrfx_uarte_rx_cache_t * p_cache)
{
    size_t user_len = p_cache->user[0].length;
    size_t len = get_curr_cache_buf_len(p_cache->cache_len, user_len, p_cache->started);

    if (!len)
    {
        if (p_cache->user[1].length) {
            p_cache->started = 0;
            len = get_curr_cache_buf_len(p_cache->cache_len, p_cache->user[1].length, 0);
        }
    }

    p_cache->started += len;

    return len;
}

nrfx_err_t nrfx_uarte_rx_buffer_set(nrfx_uarte_t const * p_instance,
                                    uint8_t *            p_data,
                                    size_t               length)
{
    NRFX_ASSERT(m_cb[p_instance->drv_inst_idx].state == NRFX_DRV_STATE_INITIALIZED);
    //NRFX_ASSERT(UARTE_LENGTH_VALIDATE(p_instance->drv_inst_idx, length));
    NRFX_ASSERT(p_data);
    NRFX_ASSERT(length > 0);

    uarte_control_block_t * p_cb = &m_cb[p_instance->drv_inst_idx];
    NRF_UARTE_Type * p_uarte = p_instance->p_reg;
    bool cont = false;
    uint32_t int_enabled;
    nrfx_err_t err = NRFX_SUCCESS;

    int_enabled = uarte_int_lock(p_uarte);

    if (p_cb->flags & UARTE_FLAG_RX_ABORTED)
    {
        err = NRFX_ERROR_INVALID_STATE;
    }
    else if (!nrf_dma_accessible_check(p_uarte, p_data))
    {
        // No cache buffer provided or blocking mode, transfer cannot be handled.
        if (!RX_CACHE_SUPPORTED || !p_cb->rx.p_cache || !p_cb->handler)
        {
            err = NRFX_ERROR_INVALID_ADDR;
        }
        else
        {
            nrfx_uarte_rx_cache_t * p_cache = p_cb->rx.p_cache;

            if (!p_cache->user[0].p_buffer)
            {
                p_cache->started = 0;
                p_cache->received = 0;
                p_cache->user[0].p_buffer = p_data;
                p_cache->user[0].length = length;
                p_data = p_cache->cache[0].p_buffer;
                length = get_cache_buf_len(p_cache);
                p_cache->idx = 1;
                p_cache->buf_req = true;
                NRFX_ATOMIC_FETCH_OR(&p_cb->flags, UARTE_FLAG_RX_USE_CACHE);
                cont = true;
            }
            else if (!p_cache->user[1].p_buffer)
            {
                p_cache->user[1].p_buffer = p_data;
                p_cache->user[1].length = length;
                err = NRFX_SUCCESS;
                if (!p_cb->rx.next.p_buffer)
                {
                    length = get_cache_buf_len(p_cache);
                    p_data = p_cache->cache[p_cache->idx++ & 0x1].p_buffer;
                    cont = true;
                }
            }
            else
            {
                err = NRFX_ERROR_BUSY;
            }
        }
    }
    else if (RX_CACHE_SUPPORTED && (p_cb->flags & UARTE_FLAG_RX_USE_CACHE))
    {
        // For first buffer cache was used. It is expected that following buffer will also
        // be cached.
        err = NRFX_ERROR_FORBIDDEN;
    }
    else
    {
        cont = true;
    }

    if (cont)
    {
        err = rx_buffer_set(p_uarte, p_cb, p_data, length);
    }

    uarte_int_unlock(p_uarte, int_enabled);

    return err;
}

static void rx_flush(NRF_UARTE_Type * p_uarte, uarte_control_block_t * p_cb)
{
    if (!(p_cb->flags & UARTE_FLAG_RX_KEEP_FIFO_CONTENT))
    {
        p_cb->rx.flush.length = 0;
        return;
    }

    /* Flushing RX fifo requires buffer bigger than 4 bytes to empty fifo*/
    uint32_t prev_rx_amount = nrfy_uarte_rx_amount_get(p_uarte);
    uint32_t check_content = prev_rx_amount <= UARTE_HW_RX_FIFO_SIZE;

    if (USE_WORKAROUND_FOR_FLUSHRX_ANOMALY && check_content)
    {
        /* There is a HW bug which results in rx amount value not being updated
         * when fifo was empty. It is then hard to determine if fifo contained
         * number of bytes equal to the last transfer or was empty. We try to
         * determine that by watermarking flush buffer to check if it was overwritten.
         * However, if fifo contained amount of bytes equal to last transfer and
         * bytes are equal to watermarking it will be dropped. */
        memset(p_cb->rx.flush.p_buffer,
               NRFX_UARTE_RX_FIFO_FLUSH_WORKAROUND_MAGIC_BYTE, UARTE_HW_RX_FIFO_SIZE);
    }

    nrfy_uarte_rx_buffer_set(p_uarte, p_cb->rx.flush.p_buffer, UARTE_HW_RX_FIFO_SIZE);
    /* Final part of handling RXTO event is in ENDRX interrupt
     * handler. ENDRX is generated as a result of FLUSHRX task.
     */
    nrfy_uarte_event_clear(p_uarte, NRF_UARTE_EVENT_ENDRX);
    nrfy_uarte_task_trigger(p_uarte, NRF_UARTE_TASK_FLUSHRX);
    while (!nrfy_uarte_event_check(p_uarte, NRF_UARTE_EVENT_ENDRX))
    {
        /* empty */
    }
    nrfy_uarte_event_clear(p_uarte, NRF_UARTE_EVENT_RXSTARTED);
    nrfy_uarte_event_clear(p_uarte, NRF_UARTE_EVENT_ENDRX);

    p_cb->rx.flush.length = nrfy_uarte_rx_amount_get(p_uarte);

    if (USE_WORKAROUND_FOR_FLUSHRX_ANOMALY)
    {
        if ((uint32_t)p_cb->rx.flush.length == prev_rx_amount)
        {
            if (!check_content) {
                p_cb->rx.flush.length = 0;
                return;
            }

            for (size_t i = 0; i < UARTE_HW_RX_FIFO_SIZE; i++)
            {
                if (p_cb->rx.flush.p_buffer[i] != NRFX_UARTE_RX_FIFO_FLUSH_WORKAROUND_MAGIC_BYTE)
                {
                    return;
                }
            }
            p_cb->rx.flush.length = 0;
        }
    }
}

static void wait_for_rx_completion(NRF_UARTE_Type *        p_uarte,
                                   uarte_control_block_t * p_cb)
{
    while(nrfy_uarte_event_check(p_uarte, NRF_UARTE_EVENT_RXTO) == false)
    {}

    nrfy_uarte_event_clear(p_uarte, NRF_UARTE_EVENT_RXSTARTED);
    nrfy_uarte_event_clear(p_uarte, NRF_UARTE_EVENT_ENDRX);
    nrfy_uarte_event_clear(p_uarte, NRF_UARTE_EVENT_RXTO);

    rx_flush(p_uarte, p_cb);
    disable_hw_from_rx(p_uarte);

    p_cb->rx.curr.p_buffer = NULL;
    p_cb->rx.next.p_buffer = NULL;
    release_rx(p_cb);
}

static nrfx_err_t rx_abort(NRF_UARTE_Type *        p_uarte,
                           uarte_control_block_t * p_cb,
                           bool                    disable_all,
                           bool                    sync)
{
    uint32_t flag;
    bool endrx_startrx = nrfy_uarte_shorts_get(p_uarte, NRF_UARTE_SHORT_ENDRX_STARTRX) != 0;
    uint32_t int_enabled;

    // We need to ensure that operation is not interrupted by the UARTE interrupt since we
    // are changing state flags. Otherwise interrupt may be executed with RX_ABORTED flag set
    // but before STOPRX task is triggered which may lead to unexpected behavior.
    if (!((p_cb->flags & (UARTE_FLAG_RX_ENABLED | UARTE_FLAG_RX_ABORTED)) ==
        UARTE_FLAG_RX_ENABLED))
    {
        // If we late with the abort we still want to clean flushed data since explicit
        // abort indicates that we are not interested in the data that ended up in the
        // FIFO.
        if (disable_all) {
            p_cb->rx.flush.length = 0;
        }

        return NRFX_ERROR_INVALID_STATE;
    }

    int_enabled = uarte_int_lock(p_uarte);

    if (disable_all || !endrx_startrx)
    {
        nrfy_uarte_shorts_disable(p_uarte, NRF_UARTE_SHORT_ENDRX_STARTRX);
        flag = UARTE_FLAG_RX_STOP_ON_END | UARTE_FLAG_RX_ABORTED | UARTE_FLAG_RX_FORCED_ABORT;
    }
    else
    {
        flag = UARTE_FLAG_RX_RESTARTED;
    }

    NRFX_ATOMIC_FETCH_OR(&p_cb->flags, flag);

    if (sync || !p_cb->handler)
    {
        nrfy_uarte_int_disable(p_uarte, rx_int_mask);
        nrfy_uarte_task_trigger(p_uarte, NRF_UARTE_TASK_STOPRX);
        wait_for_rx_completion(p_uarte, p_cb);
        int_enabled &= ~rx_int_mask;
    }
    else
    {
        nrfy_uarte_task_trigger(p_uarte, NRF_UARTE_TASK_STOPRX);
    }

    uarte_int_unlock(p_uarte, int_enabled);

    return NRFX_SUCCESS;
}

nrfx_err_t nrfx_uarte_rx_abort(nrfx_uarte_t const * p_instance, bool disable_all, bool sync)
{
    NRFX_ASSERT(p_instance);
    NRFX_ASSERT(m_cb[p_instance->drv_inst_idx].state == NRFX_DRV_STATE_INITIALIZED);

    uarte_control_block_t * p_cb = &m_cb[p_instance->drv_inst_idx];
    NRF_UARTE_Type * p_uarte = p_instance->p_reg;

    return rx_abort(p_uarte, p_cb, disable_all, sync);
}

nrfx_err_t nrfx_uarte_rx(nrfx_uarte_t const * p_instance,
                         uint8_t *            p_data,
                         size_t               length)
{
    nrfx_err_t err_code = nrfx_uarte_rx_buffer_set(p_instance, p_data, length);
    uarte_control_block_t * p_cb = &m_cb[p_instance->drv_inst_idx];

    NRFX_ASSERT(p_cb->state == NRFX_DRV_STATE_INITIALIZED);
    //NRFX_ASSERT(UARTE_LENGTH_VALIDATE(p_instance->drv_inst_idx, length));
    NRFX_ASSERT(p_data);
    NRFX_ASSERT(length > 0);

    if (err_code != NRFX_SUCCESS)
    {
        return err_code;
    }

    uint32_t flags = NRFX_UARTE_RX_ENABLE_CONT | NRFX_UARTE_RX_ENABLE_STOP_ON_END;

    err_code = nrfx_uarte_rx_enable(p_instance, flags);
    if (err_code != NRFX_ERROR_BUSY && err_code != NRFX_SUCCESS)
    {
        return err_code;
    }
    err_code = NRFX_SUCCESS;

    if (p_cb->handler == NULL)
    {
        size_t rx_amount = 0;

        do
        {
           err_code = nrfx_uarte_rx_ready(p_instance, &rx_amount);
        } while (err_code == NRFX_ERROR_BUSY);

        if ((err_code == NRFX_ERROR_ALREADY) || (length > rx_amount))
        {
            err_code = NRFX_ERROR_FORBIDDEN;
        }
        else
        {
            err_code = nrfx_uarte_rx_abort(p_instance, true, true);
            NRFX_ASSERT(err_code == NRFX_SUCCESS);
        }
    }

    return err_code;
}

nrfx_err_t nrfx_uarte_rx_ready(nrfx_uarte_t const * p_instance, size_t * p_rx_amount)
{
    uarte_control_block_t * p_cb = &m_cb[p_instance->drv_inst_idx];

    NRFX_ASSERT(p_cb->state == NRFX_DRV_STATE_INITIALIZED);

    if (p_cb->handler)
    {
        return NRFX_ERROR_FORBIDDEN;
    }

    if (!nrfy_uarte_enable_check(p_instance->p_reg))
    {
        if (p_rx_amount)
        {
            *p_rx_amount = nrfy_uarte_rx_amount_get(p_instance->p_reg);
        }

        return NRFX_ERROR_ALREADY;
    }
    else if (nrfy_uarte_event_check(p_instance->p_reg, NRF_UARTE_EVENT_ENDRX))
    {
        if (p_rx_amount)
        {
            *p_rx_amount = nrfy_uarte_rx_amount_get(p_instance->p_reg);
        }
        return NRFX_SUCCESS;
    }
    else
    {
        return NRFX_ERROR_BUSY;
    }
}

nrfx_err_t nrfx_uarte_int_trigger(nrfx_uarte_t const * p_instance)
{
    uarte_control_block_t * p_cb = &m_cb[p_instance->drv_inst_idx];

    if (!p_cb->handler)
    {
        return NRFX_ERROR_FORBIDDEN;
    }

    if (!(NRFX_ATOMIC_FETCH_OR(&p_cb->flags, UARTE_FLAG_TRIGGER) & UARTE_FLAG_TRIGGER))
    {
        NRFX_IRQ_PENDING_SET(nrfx_get_irq_number((void *)p_instance->p_reg));
    }

    return NRFX_SUCCESS;
}

uint32_t nrfx_uarte_errorsrc_get(nrfx_uarte_t const * p_instance)
{
    NRFX_ASSERT(m_cb[p_instance->drv_inst_idx].state != NRFX_DRV_STATE_UNINITIALIZED);
    /* Function must be used in blocking mode only. */
    NRFX_ASSERT(m_cb[p_instance->drv_inst_idx].handler == NULL);

    nrfy_uarte_event_clear(p_instance->p_reg, NRF_UARTE_EVENT_ERROR);
    return nrfy_uarte_errorsrc_get_and_clear(p_instance->p_reg);
}

bool nrfx_uarte_rx_new_data_check(nrfx_uarte_t const * p_instance)
{
    uarte_control_block_t * p_cb = &m_cb[p_instance->drv_inst_idx];
    bool flushed_data = (NRFX_ATOMIC_FETCH_AND(&p_cb->flags, ~UARTE_FLAG_RX_FROM_FLUSH) &
                        UARTE_FLAG_RX_FROM_FLUSH) != 0;

    if (nrfy_uarte_event_check(p_instance->p_reg, NRF_UARTE_EVENT_RXDRDY) || flushed_data)
    {
        nrfy_uarte_event_clear(p_instance->p_reg, NRF_UARTE_EVENT_RXDRDY);
        return true;
    }
    return false;
}

static void rxstarted_irq_handler(NRF_UARTE_Type * p_reg, uarte_control_block_t * p_cb)
{
    bool cache_used = RX_CACHE_SUPPORTED && (p_cb->flags & UARTE_FLAG_RX_USE_CACHE);

    // If both buffers are already setup just leave. It is possible in case
    // of following scenario. 1 byte was requested and received. RX done event
    // is generated and from that event nrfx_uarte_rx_buffer_set is called to
    // receive next RX. This sets current buffer. After ENDRX event is processed
    // then RXSTARTED event is processed (from that 1 byte transfer). From there
    // RX buffer request is called and next buffer is provided - both buffers are
    // set. Next RX is started and RXSTARTED event is triggered and we are in
    // the situation where both buffers are already set.
    if ((p_cb->rx.curr.p_buffer != NULL) && (p_cb->rx.next.p_buffer != NULL))
    {
        return;
    }

    if (!cache_used)
    {
        user_handler(p_cb, NRFX_UARTE_EVT_RX_BUF_REQUEST);
        return;
    }

    size_t len = get_cache_buf_len(p_cb->rx.p_cache);
    nrfx_uarte_rx_cache_t * p_cache = p_cb->rx.p_cache;

     if (len)
     {
        uint8_t * p_buf = p_cache->cache[p_cache->idx++ & 0x1].p_buffer;
        nrfx_err_t err = rx_buffer_set(p_reg, p_cb, p_buf, len);

        (void)err;
        NRFX_ASSERT(err == NRFX_SUCCESS);
    }

    if (p_cache->buf_req)
    {
        user_handler(p_cb, NRFX_UARTE_EVT_RX_BUF_REQUEST);
        p_cache->buf_req = false;
    }
}

static void rxto_irq_handler(NRF_UARTE_Type *        p_uarte,
                             uarte_control_block_t * p_cb)
{
    if (p_cb->rx.curr.p_buffer)
    {
        handler_on_rx_done(p_cb, p_cb->rx.curr.p_buffer, 0, true);
        p_cb->rx.curr.p_buffer = NULL;
    }

    if (RX_CACHE_SUPPORTED && (p_cb->flags & UARTE_FLAG_RX_USE_CACHE))
    {
        p_cb->rx.p_cache->user[0] = (nrfy_uarte_buffer_t){ NULL, 0 };
    }

    if (!(p_cb->flags & UARTE_FLAG_RX_FORCED_ABORT)) {
        rx_flush(p_uarte, p_cb);
    }

    on_rx_disabled(p_uarte, p_cb, p_cb->rx.flush.length);
}

static bool endrx_irq_handler(NRF_UARTE_Type *        p_uarte,
                              uarte_control_block_t * p_cb,
                              bool                    rxstarted)
{
    size_t rx_amount = (size_t)nrfy_uarte_rx_amount_get(p_uarte);
    bool premature = p_cb->flags & (UARTE_FLAG_RX_RESTARTED | UARTE_FLAG_RX_ABORTED);
    bool aborted = false;
    bool late = false;
    uint8_t *p_buf = p_cb->rx.curr.p_buffer;
    size_t len = rx_amount + p_cb->rx.off;

    p_cb->rx.curr = p_cb->rx.next;
    p_cb->rx.next = (nrfy_uarte_buffer_t){ NULL, 0 };
    handler_on_rx_done(p_cb, p_buf, len, premature);
    p_cb->rx.off = 0;

    NRFX_CRITICAL_SECTION_ENTER();

    p_cb->flags &= ~UARTE_FLAG_RX_RESTARTED;
    nrfy_uarte_shorts_disable(p_uarte, NRF_UARTE_SHORT_ENDRX_STARTRX);
    if (p_cb->flags & UARTE_FLAG_RX_ABORTED)
    {
            aborted = true;
    }
    else if (p_cb->rx.curr.p_buffer == NULL)
    {
        if (p_cb->flags & UARTE_FLAG_RX_STOP_ON_END)
        {
            nrfy_uarte_task_trigger(p_uarte, NRF_UARTE_TASK_STOPRX);
            p_cb->flags |= UARTE_FLAG_RX_ABORTED;
        }
    }
    else if (!((p_cb->flags & UARTE_FLAG_RX_CONT) &&
                (rxstarted || nrfy_uarte_event_check(p_uarte, NRF_UARTE_EVENT_RXSTARTED))))
    {
        // If continuous transfer is set then we expect that new RX is already started by short.
        // However it is possible that buffer was provided after ENDRX occurred. It can be checked
        // by examining RXSTARTED event. If it did not occur that indicates that second buffer did
        // not start but was set. It must be started manually. Usually RXSTARTED occurs almost
        // immediately after ENDRX (if short is set) so rxstarted variable check seems to be enough
        // but there are cases when RXSTARTED may be slightly delayed so if it is not set we
        // need to check HW register to make sure that it did not occur.
        nrfy_uarte_task_trigger(p_uarte, NRF_UARTE_TASK_STARTRX);
        if (nrfy_uarte_event_check(p_uarte, NRF_UARTE_EVENT_RXTO))
        {
            late = true;
        }
    }

    NRFX_CRITICAL_SECTION_EXIT();

    if (late)
    {
        nrfy_uarte_event_clear(p_uarte, NRF_UARTE_EVENT_RXTO);
        nrfy_uarte_event_clear(p_uarte, NRF_UARTE_EVENT_RXSTARTED);
        nrfy_uarte_task_trigger(p_uarte, NRF_UARTE_TASK_STOPRX);
        user_handler(p_cb, NRFX_UARTE_EVT_RX_BUF_TOO_LATE);
    }

    return aborted;
}

static void pending_tx_handler(NRF_UARTE_Type *  p_uarte,
                               uarte_tx_data_t * p_tx)
{
    /* If there is a pending tx request, it means that uart_tx()
     * was called when there was ongoing blocking transfer. Handling
     * TXSTOPPED interrupt means that blocking transfer has completed.
     */
    NRFX_ASSERT(p_tx->next.p_buffer);

    NRFX_CRITICAL_SECTION_ENTER();

    if (nrfy_uarte_event_check(p_uarte, NRF_UARTE_EVENT_TXSTOPPED))
    {
        p_tx->curr.p_buffer = p_tx->next.p_buffer;
        p_tx->next.p_buffer = NULL;
        p_tx->amount = -1;
        tx_start(p_uarte, p_tx->curr.p_buffer, p_tx->curr.length, true);
    }

    NRFX_CRITICAL_SECTION_EXIT();
}

static void txstopped_irq_handler(NRF_UARTE_Type *        p_uarte,
                                  uarte_control_block_t * p_cb)
{
    nrfy_uarte_int_disable(p_uarte, NRF_UARTE_INT_TXSTOPPED_MASK);

    NRFX_CRITICAL_SECTION_ENTER();
    disable_hw_from_tx(p_uarte, p_cb);
    NRFX_CRITICAL_SECTION_EXIT();

    // If no length set, it means that it was a blocking transfer.
    if (p_cb->tx.curr.length == 0)
    {
        return;
    }

    // if p_buf is null it indicates that tx setup interrupted poll out and
    // tx buffer is pending.
    if (p_cb->tx.curr.p_buffer == NULL)
    {
        pending_tx_handler(p_uarte, &p_cb->tx);
        return;
    }

    size_t amount;
    bool use_cache;
    bool aborted = p_cb->flags & UARTE_FLAG_TX_ABORTED;
    NRFX_CRITICAL_SECTION_ENTER();
    if (p_cb->flags & UARTE_FLAG_TX_PENDING)
    {
        amount = 0;
        use_cache = !nrf_dma_accessible_check(p_uarte, p_cb->tx.curr.p_buffer);
    }
    else
    {
        amount = p_cb->tx.amount >= 0 ? (size_t)p_cb->tx.amount : nrfy_uarte_tx_amount_get(p_uarte);
        use_cache = true;
    }
    NRFX_CRITICAL_SECTION_EXIT();

    p_cb->tx.off += amount;
    if (p_cb->tx.off == p_cb->tx.curr.length || aborted)
    {
        uint32_t off = p_cb->tx.off;

        // Transfer completed.
        p_cb->flags &= ~UARTE_FLAG_TX_ABORTED;
        p_cb->tx.curr.length = 0;
        p_cb->tx.off = 0;
        user_handler_on_tx_done(p_cb, p_cb->tx.curr.p_buffer, off, aborted);
    }
    else
    {
        NRFX_CRITICAL_SECTION_ENTER();
        p_cb->flags &= ~UARTE_FLAG_TX_PENDING;
        tx_prepare_start(p_uarte, p_cb, use_cache);
        NRFX_CRITICAL_SECTION_EXIT();
    }
}

static void error_irq_handler(NRF_UARTE_Type *        p_uarte,
                              uarte_control_block_t * p_cb)
{
    user_handler_on_error(p_uarte, p_cb);
}

static void endtx_irq_handler(NRF_UARTE_Type * p_uarte, uarte_control_block_t * p_cb)
{
    if (NRFX_IS_ENABLED(NRFX_UARTE_CONFIG_TX_LINK) && (p_cb->flags & UARTE_FLAG_TX_LINKED))
    {
        uint8_t const * p_buf = p_cb->tx.curr.p_buffer;
        uint32_t len = p_cb->tx.curr.length;
        bool aborted = p_cb->flags & UARTE_FLAG_TX_ABORTED;

        nrfy_uarte_event_clear(p_uarte, NRF_UARTE_EVENT_ENDTX);
        // If we have no PPI connection then start the transfer ASAP.
        if (!nrfy_uarte_event_check(p_uarte, NRF_UARTE_EVENT_TXSTARTED) && !aborted)
        {
            nrfy_uarte_task_trigger(p_uarte, NRF_UARTE_TASK_STARTTX);
        }

#if NRF_UARTE_HAS_ENDTX_STOPTX_SHORT
        nrfy_uarte_int_disable(p_uarte, NRF_UARTE_INT_ENDTX_MASK);
        nrfy_uarte_shorts_enable(p_uarte, NRF_UARTE_SHORT_ENDTX_STOPTX);
#endif
        NRFX_CRITICAL_SECTION_ENTER();
        p_cb->tx.curr = p_cb->tx.next;
        p_cb->tx.next.length = 0;
        p_cb->tx.next.p_buffer = NULL;
        p_cb->flags &= ~UARTE_FLAG_TX_LINKED;
        aborted = p_cb->flags & UARTE_FLAG_TX_ABORTED;
        NRFX_CRITICAL_SECTION_EXIT();

        if (aborted)
        {
            p_cb->tx.amount = 0;
            len = nrfy_uarte_tx_amount_get(p_uarte);
        }

        user_handler_on_tx_done(p_cb, p_buf, len, aborted);
    }
    else
    {
        // Locking since blocking transfer can interrupt at anytime. In that case we don't
        // want to stop ongoing blocking transfer.
        NRFX_CRITICAL_SECTION_ENTER();
        if (nrfy_uarte_event_check(p_uarte, NRF_UARTE_EVENT_ENDTX))
        {
            nrfy_uarte_event_clear(p_uarte, NRF_UARTE_EVENT_ENDTX);
            nrfy_uarte_task_trigger(p_uarte, NRF_UARTE_TASK_STOPTX);
        }
        NRFX_CRITICAL_SECTION_EXIT();
    }
}

static void int_trigger_handler(uarte_control_block_t * p_cb)
{
    user_handler(p_cb, NRFX_UARTE_EVT_TRIGGER);
}

static inline bool event_check_and_clear(NRF_UARTE_Type * p_uarte,
                                         nrf_uarte_event_t event,
                                         uint32_t int_mask)
{
    if (nrfy_uarte_event_check(p_uarte, event) && (int_mask & NRFY_EVENT_TO_INT_BITMASK(event))) {
        nrfy_uarte_event_clear(p_uarte, event);
        return true;
    }

    return false;
}

static inline bool event_check(NRF_UARTE_Type * p_uarte,
                               nrf_uarte_event_t event,
                               uint32_t int_mask)
{
    return nrfy_uarte_event_check(p_uarte, event) &&
           (int_mask & NRFY_EVENT_TO_INT_BITMASK(event));
}

static void irq_handler(NRF_UARTE_Type * p_uarte, uarte_control_block_t * p_cb)
{
    // ENDTX must be handled before TXSTOPPED so we read event status in the reversed order of
    // handling.
    uint32_t int_mask = nrfy_uarte_int_enable_check(p_uarte, UINT32_MAX);
    bool txstopped = event_check(p_uarte, NRF_UARTE_EVENT_TXSTOPPED, int_mask);
    bool endtx =  event_check(p_uarte, NRF_UARTE_EVENT_ENDTX, int_mask);

    if (p_cb->handler)
    {
        if (event_check_and_clear(p_uarte, NRF_UARTE_EVENT_ERROR, int_mask))
        {
            error_irq_handler(p_uarte, p_cb);
        }

        // ENDRX must be handled before RXSTARTED. RXTO must be handled as the last one. We collect
        // state of all 3 events before processing to prevent reordering in case of higher interrupt
        // preemption. We read event status in the reversed order of handling.
        bool rxto = event_check_and_clear(p_uarte, NRF_UARTE_EVENT_RXTO, int_mask);
        bool rxstarted = event_check_and_clear(p_uarte, NRF_UARTE_EVENT_RXSTARTED, int_mask);
        bool endrx = event_check_and_clear(p_uarte, NRF_UARTE_EVENT_ENDRX, int_mask);
        bool rxdrdy = event_check_and_clear(p_uarte, NRF_UARTE_EVENT_RXDRDY, int_mask);

        if (rxdrdy)
        {
            user_handler(p_cb, NRFX_UARTE_EVT_RX_BYTE);
        }

        if (endrx)
        {
            // If interrupt was executed exactly when ENDRX occurred it is possible
            // that RXSTARTED (which is read before ENDRX) is read as false but it
            // actually occurred (if there is a linked reception). Read again to be sure.
            if (!rxstarted)
            {
                rxstarted = event_check_and_clear(p_uarte, NRF_UARTE_EVENT_RXSTARTED, int_mask);
            }

            if (endrx_irq_handler(p_uarte, p_cb, rxstarted) == true)
            {
                rxstarted = false;
            }
        }

        if (rxstarted)
        {
            rxstarted_irq_handler(p_uarte, p_cb);
        }

        if (rxto)
        {
            rxto_irq_handler(p_uarte, p_cb);
        }
    }

    if (endtx)
    {
        endtx_irq_handler(p_uarte, p_cb);
    }

    if (txstopped)
    {
        txstopped_irq_handler(p_uarte, p_cb);
    }

    if (NRFX_ATOMIC_FETCH_AND(&p_cb->flags, ~UARTE_FLAG_TRIGGER) & UARTE_FLAG_TRIGGER)
    {
        int_trigger_handler(p_cb);
    }
}

#if NRFX_CHECK(NRFX_UARTE0_ENABLED)
void nrfx_uarte_0_irq_handler(void)
{
    irq_handler(NRF_UARTE0, &m_cb[NRFX_UARTE0_INST_IDX]);
}
#endif

#if NRFX_CHECK(NRFX_UARTE1_ENABLED)
void nrfx_uarte_1_irq_handler(void)
{
    irq_handler(NRF_UARTE1, &m_cb[NRFX_UARTE1_INST_IDX]);
}
#endif

#if NRFX_CHECK(NRFX_UARTE2_ENABLED)
void nrfx_uarte_2_irq_handler(void)
{
    irq_handler(NRF_UARTE2, &m_cb[NRFX_UARTE2_INST_IDX]);
}
#endif

#if NRFX_CHECK(NRFX_UARTE3_ENABLED)
void nrfx_uarte_3_irq_handler(void)
{
    irq_handler(NRF_UARTE3, &m_cb[NRFX_UARTE3_INST_IDX]);
}
#endif

#endif // NRFX_CHECK(NRFX_UARTE_ENABLED)
