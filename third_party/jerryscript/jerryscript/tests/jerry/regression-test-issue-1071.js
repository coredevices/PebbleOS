// SPDX-FileCopyrightText: 2016 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2016 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

var d = new Date(864858163349847396, 1, 1, 1, 1, 1, 1, 1)
assert (d.toGMTString() === "Invalid Date");

d = new Date(864858, 1, 1, 1, 1, 1, 1, 1)
assert (d.toGMTString() === "Invalid Date");

d = new Date(86485, 1, 1, 1, 1, 1, 1, 1)
assert (d.toGMTString() !== "Invalid Date");
