#!/bin/bash
# SPDX-FileCopyrightText: 2016 Samsung Electronics Co., Ltd.
# SPDX-FileCopyrightText: 2016 University of Szeged
# SPDX-License-Identifier: Apache-2.0

sudo apt-get update -q
sudo apt-get install -q -y \
    gcc-arm-linux-gnueabihf libc6-dev-armhf-cross \
    qemu-user-static
