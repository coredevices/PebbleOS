// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var o = {a: 1, b: 2, c: 3};

for (var p in o)
{
  o[p] += 4;
}

assert(o.a === 5 && o.b === 6 && o.c === 7);
