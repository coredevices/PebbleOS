// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var a = new String("qwe");

names = Object.getOwnPropertyNames(a);

assert(names instanceof Array);

var is_0 = false, is_1 = false, is_2 = false, is_length = false;
for (var i = 0; i <= 3; i++)
{
  if (names[i] === "0") { is_0 = true; }
  if (names[i] === "1") { is_1 = true; }
  if (names[i] === "2") { is_2 = true; }
  if (names[i] === "length") { is_length = true; }
}

assert (is_0 && is_1 && is_2 && is_length);
