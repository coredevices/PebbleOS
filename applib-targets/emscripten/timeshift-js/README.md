TimeShift.js
============

TimeShift.js allows mocking / overriding JavaScript's Date object so that you can set the current time and timezone.  It is meant for creating repeatable tests that utilize the current time or date.

Usage
-----

```javascript
new Date().toString();                      // Original Date object
"Fri Aug 09 2013 23:37:42 GMT+0300 (EEST)"

Date = TimeShift.Date;                      // Overwrite Date object
new Date().toString();
"Fri Aug 09 2013 23:37:43 GMT+0300"

TimeShift.setTimezoneOffset(-60);           // Set timezone to GMT+0100 (note the sign)
new Date().toString();
"Fri Aug 09 2013 21:37:44 GMT+0100"

TimeShift.setTime(1328230923000);           // Set the time to 2012-02-03 01:02:03 GMT
new Date().toString();
"Fri Feb 03 2012 02:02:03 GMT+0100"

TimeShift.setTimezoneOffset(0);             // Set timezone to GMT
new Date().toString();
"Fri Feb 03 2012 01:02:03 GMT"

TimeShift.getTime();                        // Get overridden values
1328230923000
TimeShift.getTimezoneOffset();
0

TimeShift.setTime(undefined);               // Reset to current time
new Date().toString();
"Fri Aug 09 2013 20:37:45 GMT"

new Date().desc();                          // Helper method
"utc=Fri, 09 Aug 2013 20:37:46 GMT   local=Fri, 09 Aug 2013 20:37:46 GMT   offset=0"

new TimeShift.OriginalDate().toString();    // Use original Date object
"Fri Aug 09 2013 23:37:47 GMT+0300 (EEST)"
```

Time zones
----------

TimeShift.js always utilizes its internal time zone offset when converting between local time and UTC.  The offset factor is fixed, and it does not take into account DST changes.  Effectively it emulates a time zone with no DST.

```javascript
new Date(1370034000000).toString();         // Original Date object uses variable offset
"Sat Jun 01 2013 00:00:00 GMT+0300 (EEST)"
new Date(1356991200000).toString();
"Tue Jan 01 2013 00:00:00 GMT+0200 (EET)"

Date = TimeShift.Date;                      // TimeShift.js uses fixed offset
new Date(1370034000000).toString();
"Sat Jun 01 2013 00:00:00 GMT+0300"
new Date(1356991200000).toString();
"Tue Jan 01 2013 01:00:00 GMT+0300"
```

The default time zone offset is the current local time zone offset.  Note that this can change depending on local DST.  Setting the time zone offset affects also previously created Date instances.

The time zone offset has the same sign as [Date.getTimezoneOffset](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/getTimezoneOffset).  For example, -120 is GMT+0200 and +120 is GMT-0200.

Caveats
-------

The mock implementation of Date is not perfect.

* Many string-generation methods are incomplete and return something indicative, but not fully correct.  In particular `toDateString`, `toLocaleDateString`, `toLocaleString`, `toLocaleTimeString`, `toTimeString` produce somewhat incorrect results.

* The `toString` method does not contain any time zone name.

* The `parse` method delegates directly to the original method and may not handle time zones correctly.

* DST changes cannot be emulated.  The time zone offset it always fixed.

* If a library or other code holds an original Date object or a reference to the Date prototype, things may break (e.g. error messages like "this is not a Date object").  In this case you should overwrite the Date object before loading the library.


If you'd like to fix some of these issues, please fork the repository, implement the desired functionality, add unit tests to `tests.js` and send a pull request.

License
-------

TimeShift.js is Copyright 2013 Mobile Wellness Solutions MWS Ltd and Sampo Niskanen.

It is released under the MIT license.
