// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var cond = 23;
var a = [1, 2, 4];
var cnt = 0;

for (var i = (cond < 24) ? 0 in a : 2; i < 10; ++i)
{
  ++cnt;
}

assert(cnt == 9)