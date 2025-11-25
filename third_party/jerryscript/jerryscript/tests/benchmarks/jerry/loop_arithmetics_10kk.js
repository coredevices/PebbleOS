// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var count = 10000000;
var x = 7;
var y = 3;

var tmp1;
var tmp2;
var tmp3;
var tmp4;

for (var i = 0; i < count; i++)
{
  tmp1 = x * x;
  tmp2 = y * y;
  tmp3 = tmp1 * tmp1;
  tmp4 = tmp2 * tmp2;
}
