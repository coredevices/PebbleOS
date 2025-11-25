// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2015 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

var str1 = "ab";
var str2 = "cd";
assert (str1.localeCompare(str1) === 0);
assert (str1.localeCompare(str2) === -1);
assert (str2.localeCompare(str1) === 1);

var x = "32";
var y = "-32";
assert (y.localeCompare(-31) === 1);
assert (y.localeCompare("") === 1);
assert (y.localeCompare(-32) === 0);
assert (x.localeCompare(33) === -1);
assert (x.localeCompare() === -1);
assert (x.localeCompare(null) === -1);
assert (x.localeCompare(NaN) === -1);
assert (x.localeCompare(Infinity) === -1);
assert (x.localeCompare(-Infinity) === 1);

var array1 = ["1", 2];
var array2 = [3, 4];
assert (String.prototype.localeCompare.call(42, array1) === 1);
assert (String.prototype.localeCompare.call(array1, null) === -1);
assert (String.prototype.localeCompare.call(array1, array1) === 0);
assert (String.prototype.localeCompare.call(array1, array2) === -1);
assert (String.prototype.localeCompare.call(array2, array1) === 1);

try {
  var res = String.prototype.localeCompare.call(null, 0);
  assert (false);
} catch (e) {
  assert (e instanceof TypeError);
}

try {
  var res = String.prototype.localeCompare.call();
  assert (false);
} catch (e) {
  assert (e instanceof TypeError);
}
