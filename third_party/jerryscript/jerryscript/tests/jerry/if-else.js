// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var was_then = false, was_else = false;

if (true) {
  was_then = true;
} else {
  was_else = true;
}

assert (was_then && !was_else);

was_then = false;
was_else = false;

if (false) {
  was_then = true;
} else {
  was_else = true;
}

assert (was_else && !was_then);

