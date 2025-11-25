// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2015 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

assert ([].toLocaleString() === "");
assert ([1].toLocaleString() === "1");
assert ([1,2].toLocaleString() === "1,2");
assert ([1,2,3].toLocaleString() === "1,2,3");

var test_ok = {
  length: 1,
  toLocaleString: function() { return "1"; }
};

assert ([3, test_ok, 4, test_ok].toLocaleString() === "3,1,4,1");


var obj = { toLocaleString: function() {} };
var test_non_str_locale = [undefined, obj, null, obj, obj];

assert(test_non_str_locale.toLocaleString() === ",undefined,,undefined,undefined");

var test_fail = {
  toLocaleString: "FAIL"
};

try {
  [test_fail].toLocaleString();
  assert (false);
} catch (e) {
  assert (e instanceof TypeError);
}


var test_fail_call = {
  toLocaleString: function() { throw new ReferenceError("foo"); }
};


try {
  [1, 2, test_fail_call].toLocaleString();
  assert (false);
} catch (e) {
  assert (e.message === "foo");
  assert (e instanceof ReferenceError);
}
