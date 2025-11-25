// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var a = new Array(1, 2, 3, 4, 5, 6, 7);
a.eight = 8;

var p;
for (p in a)
{
  a[p] += 1;
}

assert(a[0] === 2 && a[1] === 3 && a[2] === 4 && a[3] === 5 &&
        a[4] === 6 && a[5] === 7 && a[6] === 8 && a['eight'] === 9);
