// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2015 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

try {
  function f_1() {
    v_1 + v_1;
  }

  f_1(new f_1);

  assert (false);
} catch (e) {
  assert (e instanceof ReferenceError);
}
