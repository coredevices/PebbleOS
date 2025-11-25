#!/bin/bash
# SPDX-FileCopyrightText: 2016 Samsung Electronics Co., Ltd.
# SPDX-FileCopyrightText: 2016 University of Szeged
# SPDX-License-Identifier: Apache-2.0

brew update

PKGS="
    cmake
    cppcheck vera++
    "

for pkg in $PKGS
do
    if ! ( brew list -1 | grep -q "^${pkg}\$" )
    then
        brew install $pkg
    fi
done
