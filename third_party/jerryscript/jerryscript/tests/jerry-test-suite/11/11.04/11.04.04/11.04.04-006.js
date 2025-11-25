// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var a = 1.12;
var eps = 0.00000001;

assert(++a >= 2.12 - eps && a <= 2.12 + eps);
