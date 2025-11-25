// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var a = new Object;
a.toString = function () {
  return "1";
}

assert(-a === -1)