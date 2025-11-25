// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2015 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

// Our own join method if the internal join is not implemented.
function join(sep)
{
  sep = sep ? sep : ",";
  var result = "";

  for (var i = 0; i < this.length; ++i) {
    result += this[i];
    if (i + 1 < this.length) {
      result += sep;
    }
  }

  return result;
}

// Force fallback to object.prototype.toString()
Array.prototype.join = 1;

assert ([1].toString() === "[object Array]");

Array.prototype.join = join;

assert ([1, 2].toString() === "1,2");

var test = [1,2,3];
test.join = function() { throw ReferenceError ("foo"); };

try {
  test.toString();

  assert (false);
} catch (e) {
  assert (e.message === "foo");
  assert (e instanceof ReferenceError);
}


// Test if the join returns a ReferenceError
var arr = [1,2]
Object.defineProperty(arr, 'join', { 'get' : function () {throw new ReferenceError ("foo"); } });
try {
  arr.toString();

  assert (false);
} catch (e) {
  assert (e.message === "foo");
  assert (e instanceof ReferenceError);
}
