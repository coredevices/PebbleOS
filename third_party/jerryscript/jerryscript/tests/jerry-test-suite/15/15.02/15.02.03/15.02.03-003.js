// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0


writable = false;
enumerable = false;
configurable = false;

Object.prototype = "qwerty";
if (Object.prototype === "qwerty")
  writable = true;

for (prop in Object)
{
  if (Object[prop] == "qwerty")
    enumerable = true;
}

if (delete Object.prototype)
  configurable = true;

assert(!writable && !enumerable && !configurable);
