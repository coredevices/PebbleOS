// SPDX-FileCopyrightText: 2016 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2016 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

/* String which is 32 bytes long. */
var str = "1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+";

for (var i = 0; i < 10; i++) {
  str = str + str;
}

str = "(function() { return " + str + "1 })";

/* Eat memory. */
var array = [];

try
{
  for (var i = 0; i < 15; i++)
  {
    array[i] = eval(str);
  }
  assert (false);
}
catch (err)
{
  array = null;
  assert (err === null);
}
