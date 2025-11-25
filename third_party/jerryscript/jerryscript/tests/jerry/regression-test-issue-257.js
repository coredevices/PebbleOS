// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2015 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

var r;
r = new RegExp("]");
assert (r.exec("]") == "]");

r = new RegExp("}");
assert (r.exec("}") == "}");
