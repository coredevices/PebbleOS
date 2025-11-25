#!/bin/bash
# SPDX-FileCopyrightText: 2016 Samsung Electronics Co., Ltd.
# SPDX-FileCopyrightText: 2016 University of Szeged
# SPDX-License-Identifier: Apache-2.0

make -C tools/unit-tests build
tools/unit-tests/gen-test-libm >tests/unit/test-libm.inc.h
make -C tools/unit-tests clean
