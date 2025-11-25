// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

try {
  (function() {
    function decl() {}
  })();
  decl();
  assert(false);
} catch (e) {
  assert(e instanceof ReferenceError);
}

try {
  var o = {
    get p() {
      function decl() {
      }
    }
  };
  decl();
  assert(false);
} catch (e) {
  assert(e instanceof ReferenceError);
}
