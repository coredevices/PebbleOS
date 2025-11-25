// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

for (var p in String) {
  if (p === String.prototype) {
    assert(false);
  }
}
