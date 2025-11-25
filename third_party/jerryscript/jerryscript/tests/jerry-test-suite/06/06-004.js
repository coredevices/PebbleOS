// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var arg = 3;
function a() {
  return 5 + arg;
}

arg = 4;
var b = function () {
  return 6 + arg;
};

arg = 5;
c = function e() {
  return 7 + arg;
};

assert(a() + b() + c() === 33);
