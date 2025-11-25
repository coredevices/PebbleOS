// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

object1 = {
  valueOf: function () {
    return 1;
  },
  toString: function () {
    return 0;
  }
}

object2 = {
  valueOf: function () {
    return 1;
  },
  toString: function () {
    return 0;
  }
}

assert(object1 + object2 === 2)