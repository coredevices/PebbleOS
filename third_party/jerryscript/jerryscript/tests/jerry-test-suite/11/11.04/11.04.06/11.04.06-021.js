// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var a = new Object;
a.valueOf = function () {
  return true;
}

assert(+a === 1)