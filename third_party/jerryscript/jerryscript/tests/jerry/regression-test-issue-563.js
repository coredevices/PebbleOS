// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2015 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

try {
  eval('if (true) /abc/.exec("abc");');
} catch (e) {
  assert (false);
}

try {
  eval('if (true) {} /abc/.exec("abc");');
} catch (e) {
  assert (false);
}

try {
  eval('var a\n/abc/.exec("abc");');
} catch (e) {
  assert (false);
}
