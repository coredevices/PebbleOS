// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2015 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

try {
    Boolean(decodeURI(decodeURIComponent(Number())));
 } catch(err) {}
 try {
    ReferenceError(isNaN(__proto__));
 } catch(err) {}
 try {
    isNaN(__proto__);
 } catch(err) {}
 try {
    load();
 } catch(err) {}
 try {
    RegExp("\n");
 } catch(err) {}
 try {
    Set();
 } catch(err) {}
 try {
    ReferenceError(performance.__proto__.isPrototypeOf(arguments.map(os)));
 } catch(err) {}
 try {
    Float64Array(performance,WeakSet(),Infinity.valueOf());
 } catch(err) {}
 try {
    arguments.push(Int8Array(Boolean(isFinite(quit())),ArrayBuffer(os.system()),Array(read())));
 } catch(err) {}
 try {
    Boolean(encodeURI(DataView(ArrayBuffer(os),parseFloat(Set()),URIError(Object(Int8Array(Function(parseInt(write(),RangeError(__proto__.valueOf()))),Int16Array(Map(),__proto__.valueOf(),readbuffer()),Math))))));
 } catch(err) {}
