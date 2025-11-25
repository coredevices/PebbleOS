// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var n = {
  valueOf: function () {
    return -Infinity;
  }
}
var d = {
  valueOf: function () {
    return 0;
  }
}
assert(isNaN(n % d))