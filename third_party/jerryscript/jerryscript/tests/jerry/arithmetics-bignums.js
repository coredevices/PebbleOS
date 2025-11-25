// SPDX-FileCopyrightText: 2014-2016 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var big = 2147483646;

big++;
assert(big == 2147483647);

big += 1;
assert(big == 2147483648); // overflow on 32bit numbers

big++;
assert(big == 2147483649); // overflow on 32bit numbers

assert ((1152921504606846976).toString() === "1152921504606847000")

assert (1.797693134862315808e+308 === Infinity);
