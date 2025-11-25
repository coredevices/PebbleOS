// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2015 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

// Test raised by fuzzer
v_0 = [,];
v_1 = [,];
v_2 = Object.defineProperties([,], { '0': {  get: function() { } } });

// Test change from data to accessor type
var a = { x:2 };
Object.defineProperty(a, "x", {
      enumerable: true,
      configurable: true,
      get: function() { return 0; }
});

// Test change from accessor to data type
var obj = {test: 2};

Object.defineProperty(obj, "test", {
      enumerable: true,
      configurable: true,
      get: function() { return 0; }
});

Object.defineProperty(obj, "x", {
      enumerable: true,
      configurable: true,
      value: -2
});
