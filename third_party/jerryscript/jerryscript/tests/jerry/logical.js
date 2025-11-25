// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

assert((!false) === true);

assert((true || true) === true);
assert((false || true) === true);
assert((false || false) === false);
assert((true || false) === true);

assert((true && true) === true);
assert((true && false) === false);
assert((false && true) === false);
assert((false && false) === false);