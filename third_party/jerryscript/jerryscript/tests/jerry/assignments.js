// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var b = 5;

assert((b += 10) == 15);
assert((b -= 3) == 12);
assert((b *= 10) == 120);
assert((b /= 10) == 12);
assert((b %= 10) == 2);
