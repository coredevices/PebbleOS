// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

function x() {
}

assert((delete x) == false && typeof x == "function");
