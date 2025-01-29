#include "drivers/gpio.h"
#include "drivers/periph_config.h"
#include "drivers/voltage_monitor.h"
#include "kernel/util/delay.h"
#include "os/mutex.h"
#include "system/passert.h"

void voltage_monitor_init(void) {
}

void voltage_monitor_device_init(VoltageMonitorDevice *device) {
}

void voltage_monitor_read(VoltageMonitorDevice *device, VoltageReading *reading_out) {
}
