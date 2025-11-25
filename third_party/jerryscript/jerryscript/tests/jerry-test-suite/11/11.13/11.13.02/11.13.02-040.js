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
assert(((object *= 2) === (copyObject * 2)) && (object === (copyObject * 2)))