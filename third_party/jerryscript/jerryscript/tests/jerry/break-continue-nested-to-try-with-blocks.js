// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

/* with */

for (var i = 0; i < 10; i++)
{
  with ({})
  {
    break;

    assert (false);
  }
}
assert (i === 0);

for (var i = 0; i < 10; i++)
{
  with ({})
  {
    continue;

    assert (false);
  }
}
assert (i === 10);

/* try */
for (var i = 0; i < 10; i++)
{
  try
  {
    break;

    assert (false);
  }
  catch (e)
  {
  }
}
assert (i === 0);

for (var i = 0; i < 10; i++)
{
  try
  {
    continue;

    assert (false);
  }
  catch (e)
  {
  }
}
assert (i === 10);

/* catch */
for (var i = 0; i < 10; i++)
{
  try
  {
    throw new TypeError ();
    assert (false);
  }
  catch (e)
  {
    break;
    assert (false);
  }
}
assert (i === 0);

for (var i = 0; i < 10; i++)
{
  try
  {
    throw new TypeError ();
    assert (false);
  }
  catch (e)
  {
    continue;
    assert (false);
  }
}
assert (i === 10);


/* finally */
for (var i = 0; i < 10; i++)
{
  try
  {
    throw new TypeError ();
    assert (false);
  }
  catch (e)
  {
  }
  finally
  {
    break;
    assert (false);
  }
}
assert (i === 0);

for (var i = 0; i < 10; i++)
{
  try
  {
    throw new TypeError ();
    assert (false);
  }
  catch (e)
  {
  }
  finally
  {
    continue;
    assert (false);
  }
}
assert (i === 10);


/* with - switch */

str = '';
for (var i = 0; i < 10; i++)
{
  with ({})
  {
    switch (i)
    {
      case 0:
        str += 'A';
        break;
      default:
        str += 'B';
        continue;
    }

    str += 'C';
  }
}
assert (str === 'ACBBBBBBBBB');
