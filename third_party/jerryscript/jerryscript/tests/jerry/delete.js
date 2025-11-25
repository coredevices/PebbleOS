// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

/* argument is not reference */
assert (delete 0 === true);
assert (delete "0" === true);
assert (delete (a = 1) === true);
assert (delete delete a === true);

/* argument is unresolvable reference */
assert (delete undefined_variable === true);

/* argument is object-based reference */
var a = [1];
assert (a[0] === 1);
assert (delete a[0] === true);
assert (a[0] == undefined);

var b = {c:0};
assert (b.c === 0);
assert (delete b.c === true);
assert (b.c === undefined);

/* argument is lexical environment-based reference */
var a = 1;
assert (a === 1);
assert (delete a === false);
assert (a === 1);
