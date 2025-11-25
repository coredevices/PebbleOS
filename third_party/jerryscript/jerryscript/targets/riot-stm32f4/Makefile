# SPDX-FileCopyrightText: 2016 Samsung Electronics Co., Ltd.
# SPDX-FileCopyrightText: 2016 University of Szeged
# SPDX-License-Identifier: Apache-2.0

# application name
APPLICATION = riot_jerry

# default BOARD enviroment
BOARD ?= stm32f4discovery

# path to the RIOT base directory
RIOTBASE ?= $(CURDIR)/../RIOT
# path to the JERRYSCRIPT directory
JERRYDIR ?= $(CURDIR)

# path to the application directory
APPDIR ?= $(JERRYDIR)/targets/riot-stm32f4/source

# path to the binary directory
BINDIR ?= $(JERRYDIR)/targets/riot-stm32f4/bin/

# Comment this out to disable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
CFLAGS += -DDEVELHELP

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

INCLUDES += -I$(JERRYDIR)/jerry-core/

# Add the shell and some shell commands
USEMODULE += shell
USEMODULE += shell_commands

# Add the jerry lib
USEMODULE += libjerrycore

include $(RIOTBASE)/Makefile.include
