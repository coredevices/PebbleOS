// SPDX-FileCopyrightText: 2016 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2016 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

var array = [ [0] ];
var obj = { prop : "" };
var i = 0;
var count = 0;

(function () {
      for (array[0][i] in obj)
          count++;
})();

assert(array[0][0] == "prop");
assert(count == 1);

