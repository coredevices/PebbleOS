// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2015 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

var res = RegExp.prototype.exec(10);

assert (res[0] === "");
assert (res.input === "10");
assert (res.index === 0);
assert (res.length === 1);
