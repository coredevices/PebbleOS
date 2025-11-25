// SPDX-FileCopyrightText: 2016 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2016 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

try
{
  Function (Array.isArray);
  assert (false);
}
catch (e)
{ 
  assert (e instanceof SyntaxError);
}

try { Function(TypeError.prototype) } catch (err) { }
