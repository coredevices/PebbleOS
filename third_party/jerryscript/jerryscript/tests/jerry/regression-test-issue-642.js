// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2015 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

Function.prototype.length = function() {};
Function.prototype.bind(0);
assert (Function.prototype.length === 0);
