#include "drivers/periph_config.h"
#include "os/mutex.h"
#include "system/logging.h"
#include "system/passert.h"

#define NRF5_COMPATIBLE
#include <mcu.h>

#include "FreeRTOS.h"

#define PERIPH_CONFIG_DEBUG 0

#if PERIPH_CONFIG_DEBUG
#define PERIPH_CONFIG_LOG(fmt, args...) \
        PBL_LOG(LOG_LEVEL_DEBUG, fmt, ## args)
#else
#define PERIPH_CONFIG_LOG(fmt, args...)
#endif

void periph_config_init(void) {
}

void periph_config_acquire_lock(void) {
}

void periph_config_release_lock(void) {
}

void periph_config_enable(void *periph, uint32_t rcc_bit) {
}

void periph_config_disable(void *periph, uint32_t rcc_bit) {
}
