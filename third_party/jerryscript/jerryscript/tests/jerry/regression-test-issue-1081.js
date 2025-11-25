// SPDX-FileCopyrightText: 2016 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2016 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

try { new RegExp().compile(A)  } catch (err) {}
try { A } catch (err) {}
try { new RegExp().constructor.prototype.toString()  } catch (A) {}
try { A } catch (err) {}
try { new RegExp(1, "g").exec(1)  } catch (A) {}
try { A } catch (err) {}
try { A } catch (A) {}
try { A } catch (A) {}
try { new (Boolean.constructor.prototype)  } catch (err) {}
try { Boolean  } catch (A) {}
try { A } catch(A) {}
try { new RegExp().compile(new (RegExp)())  } catch (A) {}
try { Math.max().constructor.seal(A) } catch (err) {}
try { Date.parse(RegExp.prototype.compile(RegExp.prototype)) } catch(A) {}
try { Boolean.A() } catch (err) {}
