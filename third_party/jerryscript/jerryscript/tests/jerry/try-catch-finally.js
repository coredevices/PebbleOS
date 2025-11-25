// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var was_catch = false, was_finally = false;

try {
} catch (err) {
  was_catch = true;
} finally {
  was_finally = true;
}

assert (!was_catch && was_finally);

was_catch = false;
was_finally = false;

try {
  throw 1;
  assert (0);
} catch (err) {
  assert (err === 1);
  was_catch = true;
} finally {
  was_finally = true;
}

assert (was_catch && was_finally);