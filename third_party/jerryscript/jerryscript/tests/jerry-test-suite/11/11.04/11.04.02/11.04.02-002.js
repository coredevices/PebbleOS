// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var b = 1;

var a = void(++b);

assert(a == undefined && b == 2);
