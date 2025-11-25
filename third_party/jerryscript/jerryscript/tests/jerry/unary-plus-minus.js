// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

assert (+0 === -0);
assert (1/-0 === -Infinity);
assert (1/+0 === Infinity);
assert ("3" -+-+-+ "1" + "1" / "3" * "6" + "2" === "42");
assert (isNaN (-NaN));