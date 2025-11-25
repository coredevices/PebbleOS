// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

object = {
  valueOf: function () {
    return 1
  },
  toString: function () {
    return 0
  }
}

copyObject = object;
assert(((object |= 1) === (copyObject | 1)) && (object === (copyObject | 1)))
