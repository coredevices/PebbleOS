#!/bin/sh
# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0


gcc test_base64.c base64.c -I../ -g -std=c99 -DUNIT_TEST && ./a.out
