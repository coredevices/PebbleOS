// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2015 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

try {
	eval("switch(foo){} {}");
	assert(false);
} catch (e) {
	assert(e instanceof ReferenceError);
}

