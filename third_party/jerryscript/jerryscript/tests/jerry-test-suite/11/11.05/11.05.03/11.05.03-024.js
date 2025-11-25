// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var n = {
  valueOf: function () {
    return 109.5;
  }
}
var d = {
  valueOf: function () {
    return 5.5;
  }
}
assert(n % d === 5)