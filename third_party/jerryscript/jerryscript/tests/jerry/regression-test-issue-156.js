// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2015 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

function dec(x) { return x - 1 };
for (var i = 5; i > 0; i = dec(i)) {}
for (var i = 11; i = dec (i); i--) {}
for (var i = dec (12); i > 0; i--) {}
