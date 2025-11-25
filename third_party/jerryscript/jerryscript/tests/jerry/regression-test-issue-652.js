// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2015 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

try {
  eval("this / 10");
} catch (e) {
  assert (false);
}

try {
  eval("var v_0 = 10;\nv_0++ / 1");
} catch (e) {
  assert (false);
}

try {
  eval("var v_0 = 10;\nif (v_0++ / 1) {\n}");
} catch (e) {
  assert (false);
}
