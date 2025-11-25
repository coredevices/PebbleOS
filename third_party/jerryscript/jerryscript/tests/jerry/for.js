// SPDX-FileCopyrightText: 2014-2015 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2015 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

// 1.
var i = 0;
for (; i < 100; i++) {
}
assert(i == 100);

// 2.
for (var j = 0; j < 100; j++) {
}
assert(j == 100);

// 3.
for (i = 0; ; ) {
  if (i == 100) {
    break;
    assert(false);
  }
  i++;
}
assert(i == 100);

// 4.
for (i = 0; i < 10; i++) {
  for (j = 0; j < 10; j++) {
  }
}
assert(i != 100);
assert(j != 100);
assert(i == 10);
assert(j == 10);

// 5.
s = '';
for (
var i = {x: 0};

 i.x < 2
;
 i.x++

)
 {
  s += i.x;
}

assert (s === '01');

// 6.
s = '';
for (
var i = {x: 0};

 i.x < 2
;

 i.x++

)
 {
  s += i.x;
}

assert (s === '01');

// 7.
a = [];
for (; a[0]; ) {
  assert (false);
}
