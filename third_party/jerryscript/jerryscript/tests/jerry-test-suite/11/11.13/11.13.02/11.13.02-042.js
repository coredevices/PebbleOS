// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

object = {
  valueOf: function () {
    return 15
  },
  toString: function () {
    return ""
  }
}

copyObject = object;
assert(((object -= 2) === (copyObject - 2)) && (object === (copyObject - 2)))