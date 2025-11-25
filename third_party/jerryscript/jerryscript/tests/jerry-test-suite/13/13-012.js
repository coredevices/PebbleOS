// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

function foo(a, b, c, d) {
  var deleted = true;
  for (i = 0; i < arguments.length; i++)
  {
    delete arguments[i];
    deleted = deleted && (typeof (arguments[i]) === "undefined");
  }
  return deleted;
}

assert(foo("A", "F", 1, true) === true);
