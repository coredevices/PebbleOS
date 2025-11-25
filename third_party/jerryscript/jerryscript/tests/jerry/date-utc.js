// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2015 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

var d;

d = Date.UTC(undefined);
assert (isNaN(d));

d = Date.UTC({});
assert (isNaN(d));

d = Date.UTC(2015);
assert (isNaN(d));

d = Date.UTC(2000 + 15, 0);
assert (d == 1420070400000);

d = Date.UTC(2015, 0);
assert (d == 1420070400000);

d = Date.UTC(2015, 0, 1);
assert (d == 1420070400000);

d = Date.UTC(2015, 0, 1, 0);
assert (d == 1420070400000);

d = Date.UTC(2015, 0, 1, 0, 0);
assert (d == 1420070400000);

d = Date.UTC(2015, 0, 1, 0, 0, 0);
assert (d == 1420070400000);

d = Date.UTC(2015, 0, 1, 0, 0, 0, 0);
assert (d == 1420070400000);

d = Date.UTC(2015, 6, 3, 14, 35, 43, 123);
assert (d == 1435934143123);
