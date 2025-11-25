// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

obj = new Object();
assert(isNaN(obj - NaN) && isNaN(NaN - obj))