// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var i = 0;

function a() {}

a.prototype.b = function() { i++; }

a.prototype.c = function() { this.b(); i++; }

var aa = new a();
aa.c();

assert (i === 2);
