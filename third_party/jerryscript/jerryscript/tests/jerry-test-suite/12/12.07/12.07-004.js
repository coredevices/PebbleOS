// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var o = {a: 1, b: 2, c: 3};

ForLabel:
        for (var p in o)
{
  if (p === "b")
    continue ForLabel;
  o[p] += 4;
}

assert(o.a === 5 && o.b === 2 && o.c === 7);
