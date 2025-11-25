// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

try {
  try {
    throw "error";
  } catch (e) {
    throw e;
  }
  assert(false);
} catch (e) {
}
