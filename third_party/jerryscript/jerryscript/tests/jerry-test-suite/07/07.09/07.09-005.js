// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var b = 4, c = 5;

a = b
--c

assert(a === 4 && c === 4);
