// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var b = 1;
for (var i = 1; i < 10; ++i)
{
  b *= i;
}
var c = b;

assert(c == 362880);
