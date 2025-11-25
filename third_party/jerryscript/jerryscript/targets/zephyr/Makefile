# SPDX-FileCopyrightText: © 2016 Intel Corporation
# SPDX-License-Identifier: Apache-2.0

# use TAB-8
.DEFAULT_GOAL := all

ifndef ZEPHYR_BASE
$(error Missing zephyr base)
endif

# For testing without real hardware use qemu_x86 instead of arduino_101
BOARD ?= arduino_101_factory

O ?= $(PROJECT_BASE)/outdir

ZEPHYR    ?= $(ZEPHYR_BASE)
TYPE      ?= release
JERRYHEAP ?= 16

ZEPHYRINC   = $(ZEPHYR_BASE)/include
ZEPHYRLIB   = $(ZEPHYR_BASE)/lib

TARGET_ZEPHYR ?= ./targets/zephyr
SOURCE_DIR = $(TARGET_ZEPHYR)/src

export JERRY_INCLUDE = $(CURDIR)/jerry-core/

MDEF_FILE = $(realpath $(SOURCE_DIR)/../prj.mdef)
CONF_FILE = $(realpath $(SOURCE_DIR)/../prj.conf)

ifdef V
$(info Zephyr)
$(info SOURCE_DIR=$(SOURCE_DIR))
$(info JERRY_INCLUDE=$(JERRY_INCLUDE))
$(info CONF_FILE =$(CONF_FILE))
endif

KERNEL_TYPE = micro

KBUILD_VERBOSE = $(V)

APP = main-zephyr.c

ALL_LIBS += $(USER_LIBS)
export ALL_LIBS
LDFLAGS_zephyr += $(USER_LIB_INCLUDE_DIR)
export LDFLAGS_zephyr

include ${ZEPHYR_BASE}/Makefile.inc 

.PHONY = showconfig

