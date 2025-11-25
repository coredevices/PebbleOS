// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

a = {
  n: Number,
  s: String
};
b = {
  n: Number,
  s: String
};
a.n = 1;
b.n = 2;
a.s = "qwe";
b.s = "rty";

assert(((a).n + (b).n === 3) && ((a).s + (b).s === "qwerty"));
