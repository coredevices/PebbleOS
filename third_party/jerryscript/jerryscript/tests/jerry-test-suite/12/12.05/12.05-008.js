// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var a = 1, b = 2;
var c;
if (a === 1)
  if (b === 1)
    c = 3;
  else
  if (b === 2)
    c = 5;
  else
    c = 7;

assert(c === 5);
      