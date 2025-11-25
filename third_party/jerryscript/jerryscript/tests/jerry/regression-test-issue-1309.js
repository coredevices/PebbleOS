// SPDX-FileCopyrightText: 2016 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2016 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

var errorMessage = "toStringThrows"

var toStringThrows = {
  toString : function() {
    throw new Error(errorMessage);
  }
}

try {
  var obj = {};
  obj[toStringThrows] = 3;
  assert(false);
} catch (e) {
  assert(e.message == errorMessage);
}

