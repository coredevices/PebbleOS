// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var cond = 1.999;
var cnt = 0;

for (; cond < 10.333; cond += 1.121)
  cnt++;

assert(cnt == 8);
