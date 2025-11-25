// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var a = Object.getOwnPropertyDescriptor(Number, 'prototype');

assert((a.writable === false &&
        a.enumerable === false &&
        a.configurable === false));
