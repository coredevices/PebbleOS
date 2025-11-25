#!/bin/bash
# SPDX-FileCopyrightText: 2014-2015 Samsung Electronics Co., Ltd.
# SPDX-License-Identifier: Apache-2.0

echo "#define JERRY_MCU_SCRIPT \\" > $2
# escape all " characters, wrap each line in double quotes and end the line with '\'
sed 's/"/\\"/g' $1 | sed 's/^.*$/"\0" \\/g' >> $2
echo >> $2
