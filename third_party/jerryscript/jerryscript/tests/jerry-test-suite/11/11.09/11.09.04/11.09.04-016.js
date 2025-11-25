// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var x = 123.00, y = 0.0123e+4, eps = .00001
assert(x <= y + eps && x >= y - eps)