// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2015 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

function check_reference_error (s) {
  try {
    eval (s);
    assert (false);
  } catch (e) {
    assert (e instanceof ReferenceError);
  }
}

check_reference_error ('v_0 = {0: delete 3. instanceof foo() ^ {} }');
