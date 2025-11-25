// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var eps = 0.000000001;
var a = 1.12;

assert(--a >= 0.12 - eps &&
        a <= 0.12 + eps)