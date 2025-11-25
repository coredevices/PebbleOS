// SPDX-FileCopyrightText: 2016 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2016 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

Array.prototype.push(1)
Object.freeze(Array.prototype)
try { new RegExp().constructor.prototype.exec()  } catch($){}
