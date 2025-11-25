// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

object = {
  valueOf: function () {
    return "12"
  },
  toString: function () {
    return 0
  }
}

copyObject = object;
assert(((object += 3) === (copyObject + 3)) && (object === (copyObject + 3)))