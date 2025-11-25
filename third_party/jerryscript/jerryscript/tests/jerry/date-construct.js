// SPDX-FileCopyrightText: 2015-2016 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2015-2016 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

assert (Date.length == 7);
assert (Object.prototype.toString.call (Date.prototype) === '[object Date]');

var d;

try
{
  d = new Date({toString: function() { throw new Error("foo"); }});
  assert (false);
}
catch (e)
{
  assert (e instanceof Error);
  assert (e.message === "foo");
}

assert (isNaN(Date.prototype.valueOf.call(Date.prototype)));

d = new Date("abcd");
assert (isNaN(d.valueOf()));

d = new Date();
assert (!isNaN(d.valueOf()));

d = new Date("2015-01-01");
assert (d.valueOf() == 1420070400000);

d = new Date(1420070400000);
assert (d.valueOf() == 1420070400000);

d = new Date(2015,0,1,0,0,0,0);
assert (d.valueOf() - (d.getTimezoneOffset() * 60000) == 1420070400000);

d = new Date(8.64e+15);
assert (d.valueOf() == 8.64e+15);

d = new Date(8.64e+15 + 1);
assert (isNaN(d.valueOf()));

d = new Date(20000000, 0, 1);
assert (isNaN(d.valueOf()));

d = new Date(0, 20000000, 1);
assert (isNaN(d.valueOf()));

var Obj = function (val)
{
  this.value = val;
  this.valueOf = function () { throw new ReferenceError ("valueOf-" + this.value); };
  this.toString = function () { throw new ReferenceError ("toString-"+ this.value); };
};

try
{
  d = new Date (new Obj (1), new Obj (2));
  // Should not be reached.
  assert (false);
}
catch (e)
{
  assert (e instanceof ReferenceError);
  assert (e.message === "valueOf-1");
}

assert (typeof Date (2015) == "string");
assert (typeof Date() != typeof (new Date ()));
assert (Date (Number.NaN) == Date ());
