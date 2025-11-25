# SPDX-FileCopyrightText: 2016 Samsung Electronics Co., Ltd.
# SPDX-FileCopyrightText: 2016 University of Szeged
# SPDX-License-Identifier: Apache-2.0

-include $(TOPDIR)/Make.defs

# Jerryscript built-in application info

CONFIG_JERRYSCRIPT_PRIORITY ?= SCHED_PRIORITY_DEFAULT
CONFIG_JERRYSCRIPT_STACKSIZE ?= 16384
CONFIG_JERRYSCRIPT_HEAPSIZE ?= 107520

APPNAME = jerry
# path to the project dir, "jerry-nuttx" by default
ROOT_DIR = ../../..
PRIORITY = $(CONFIG_JERRYSCRIPT_PRIORITY)
STACKSIZE = $(CONFIG_JERRYSCRIPT_STACKSIZE)
CFLAGS += -std=c99 -DJERRY_NDEBUG '-DCONFIG_MEM_HEAP_AREA_SIZE=$(CONFIG_JERRYSCRIPT_HEAPSIZE)'
CFLAGS += -I$(ROOT_DIR)/ $(shell find $(ROOT_DIR)/jerryscript/jerry-core -type d | sed -r -e 's/^/-I/g')

# Jerryscript

.PHONY: jerry_core_allin.c
jerry_core_allin.c:
	echo '#include "jerryscript/jerry-libm/nextafter.c"' > jerry_core_allin.c
	find $(ROOT_DIR)/jerryscript/jerry-core -name "*.c" | sed -r -e 's/(\.\.\/)*(.+)/#include "\2"/g' >> jerry_core_allin.c

ASRCS =
CSRCS = jerry_core_allin.c
MAINSRC = jerry_main.c

CONFIG_JERRYSCRIPT_PROGNAME ?= jerry$(EXEEXT)
PROGNAME = $(CONFIG_JERRYSCRIPT_PROGNAME)

include $(APPDIR)/Application.mk
