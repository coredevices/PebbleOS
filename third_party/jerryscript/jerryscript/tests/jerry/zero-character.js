// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

try {
  eval("/*var " + String.fromCharCode(0) + "xx = 1*/");
}
catch (e) {
  assert (false);
}

assert ("x\0y" !== "x\0z");
