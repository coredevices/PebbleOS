#!/bin/bash
# SPDX-FileCopyrightText: 2016 Samsung Electronics Co., Ltd.
# SPDX-FileCopyrightText: 2016 University of Szeged.
# SPDX-License-Identifier: Apache-2.0

JERRY=$1
TEST=$2

MEM_PEAK=`$JERRY $TEST --mem-stats | grep "Peak allocated =" | awk '{print $4}'`

echo $MEM_PEAK
