// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

assert((7 < 4) == false);
assert((7 > 4) == true);

assert((7 <= 11) == true);
assert((11 <= 11) == true);

assert((7 >= 11) == false);
assert((7 >= 7) == true);

assert(0 > (0 - 'Infinity'));
assert(0 < (0 - '-Infinity'));
assert((0 - 'Infinity') < (0 - '-Infinity'));

assert('a' > '');
assert(!('' < ''));
assert(!('' > ''));
assert('abcd' > 'abc');
assert('abc' < 'abcd');
assert('abcd' <= 'abcd');
assert('abcd' >= 'abcd');
assert(!('abcd' > 'abcd'));
