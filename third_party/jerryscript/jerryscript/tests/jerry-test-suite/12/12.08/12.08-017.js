// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var o = {a: 1, b: 2, c: 3};

ForLabel:
        for (var p in o)
{
  if (p === "b")
    break ForLabel;
  o[p] += 4;
}

assert(o.a + o.b + o.c === 10);
