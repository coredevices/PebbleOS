// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var bitField = 0x1000000;
var cnt = 0;

while (bitField >>= 1)
{
  cnt++;
}

assert(cnt === 24);
