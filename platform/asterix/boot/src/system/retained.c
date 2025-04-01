#include "drivers/dbgserial.h"

#include "retained.h"
#include "util/crc32.h"

static uint32_t __attribute__((section(".retained"))) retained[256 / 4];

void retained_init()
{
	uint32_t crc32_computed = crc32(0, retained, NRF_RETAINED_REGISTER_CRC * 4);
	if (crc32_computed != retained[NRF_RETAINED_REGISTER_CRC]) {
		dbgserial_print("Retained register CRC failed: expected CRC ");
		dbgserial_print_hex(crc32_computed);
		dbgserial_print(", got CRC ");
		dbgserial_print_hex(retained[NRF_RETAINED_REGISTER_CRC]);
		dbgserial_print(".  Clearing bootbits!");
		dbgserial_newline();
		memset(retained, 0, sizeof(retained));
	}
}

void retained_write(uint8_t id, uint32_t value)
{
	retained[id] = value;
	uint32_t crc32_computed = crc32(0, retained, NRF_RETAINED_REGISTER_CRC * 4);
	retained[NRF_RETAINED_REGISTER_CRC] = crc32_computed;
}

uint32_t retained_read(uint8_t id)
{
	return retained[id];
}
