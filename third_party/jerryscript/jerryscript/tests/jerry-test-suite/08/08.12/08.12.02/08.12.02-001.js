// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var prot = {
  b: 3
};

function Custom() {
}

Custom.prototype = prot;

var obj = new Custom();

assert(obj.b === 3);
