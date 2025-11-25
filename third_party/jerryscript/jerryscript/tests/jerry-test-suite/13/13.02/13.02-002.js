// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var foo = function (c, y) {
};
var check = foo.hasOwnProperty("length") && foo.length === 2;

foo.length = 12;
if (foo.length === 12)
  check = false;

for (p in foo)
{
  if (p === "length")
    check = false;
}
delete foo.length;
if (!foo.hasOwnProperty("length"))
  check = false;

assert(check === true);
