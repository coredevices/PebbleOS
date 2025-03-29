#include "retained.h"

static uint32_t __attribute__((section(".retained"))) retained[20];

void retained_write(uint8_t id, uint32_t value)
{
	retained[id * 4U] = value;
}

uint32_t retained_read(uint8_t id)
{
	return retained[id * 4U];
}
