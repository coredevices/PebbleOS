var JAN = 0;
var FEB = 1;
var MAR = 2;
var APR = 3;
var MAY = 4;
var JUN = 5;
var JUL = 6;
var AUG = 7;
var SEP = 8;
var OCT = 9;
var NOV = 10;
var DEC = 11;

var SUN = 0;
var MON = 1;
var TUE = 2;
var WED = 3;
var THU = 4;
var FRI = 5;
var SAT = 6;

function matches(d, year, month, day, hour, min, sec, msec, wkday) {
  equal(d.getFullYear(), year, "year");
  equal(d.getMonth(), month, "month");
  equal(d.getDate(), day, "day of month");
  equal(d.getHours(), hour, "hour");
  equal(d.getMinutes(), min, "minutes");
  equal(d.getSeconds(), sec, "seconds");
  equal(d.getMilliseconds(), msec, "milliseconds");
  equal(d.getDay(), wkday, "weekday");
  equal(d.getYear(), year-1900, "year-1900");
}

function matchesUTC(d, year, month, day, hour, min, sec, msec, wkday) {
  equal(d.getUTCFullYear(), year, "UTC year");
  equal(d.getUTCMonth(), month, "UTC month");
  equal(d.getUTCDate(), day, "UTC day of month");
  equal(d.getUTCHours(), hour, "UTC hour");
  equal(d.getUTCMinutes(), min, "UTC minutes");
  equal(d.getUTCSeconds(), sec, "UTC seconds");
  equal(d.getUTCMilliseconds(), msec, "UTC milliseconds");
  equal(d.getUTCDay(), wkday, "UTC weekday");
}


//////////////////  Constructor tests  ///////////////////

test("no-arg constructor; no time set", function() {
  TimeShift.setTimezoneOffset(-120);
  var now = new Date();
  var d = new TimeShift.Date();
  ok(now.getTime() - d.getTime() < 500);
});

test("no-arg constructor; time set", function() {
  TimeShift.setTimezoneOffset(-120);
  TimeShift.setTime(1375991584123);  // Thu 2013-08-08 19:53:04.123 UTC
  var d = new TimeShift.Date();
  matches(d, 2013, AUG, 8, 21, 53, 4, 123, THU);
  matchesUTC(d, 2013, AUG, 8, 19, 53, 4, 123, THU);
  TimeShift.setTime(undefined);
});

test("no-arg constructor; reset to normal time", function() {
  TimeShift.setTimezoneOffset(-120);
  TimeShift.setTime(1375991584123);  // Thu 2013-08-08 19:53:04.123 UTC
  TimeShift.setTime(undefined);
  var now = new Date();
  var d = new TimeShift.Date();
  ok(now.getTime() - d.getTime() < 500);
});


test("timestamp constructor", function() {
  TimeShift.setTimezoneOffset(-120);
  var d = new TimeShift.Date(1375991584123);  // Thu 2013-08-08 19:53:04.123 UTC
  matches(d, 2013, AUG, 8, 21, 53, 4, 123, THU);
  matchesUTC(d, 2013, AUG, 8, 19, 53, 4, 123, THU);
});

test("constructor year-month", function() {
  TimeShift.setTimezoneOffset(180);
  var d = new TimeShift.Date(2012, MAR);  // Thu 2012-03-01 00:00:00 -0300
  matches(d, 2012, MAR, 1, 00, 00, 00, 000, THU);
  matchesUTC(d, 2012, MAR, 1, 03, 00, 00, 000, THU);
});

test("constructor year-month-day", function() {
  TimeShift.setTimezoneOffset(-180);
  var d = new TimeShift.Date(2012, MAR, 5);  // Mon 2012-03-05 00:00:00 +0300
  matches(d, 2012, MAR, 5, 00, 00, 00, 000, MON);
  matchesUTC(d, 2012, MAR, 4, 21, 00, 00, 000, SUN);
});

test("constructor year-month-day-hour", function() {
  TimeShift.setTimezoneOffset(-180);
  var d = new TimeShift.Date(2012, MAR, 5, 23);  // Mon 2012-03-05 23:00:00 +0300
  matches(d, 2012, MAR, 5, 23, 00, 00, 000, MON);
  matchesUTC(d, 2012, MAR, 5, 20, 00, 00, 000, MON);
});

test("constructor year-month-day-hour-min", function() {
  TimeShift.setTimezoneOffset(-180);
  var d = new TimeShift.Date(2012, MAR, 5, 23, 45);  // Mon 2012-03-05 23:45:00 +0300
  matches(d, 2012, MAR, 5, 23, 45, 00, 000, MON);
  matchesUTC(d, 2012, MAR, 5, 20, 45, 00, 000, MON);
});

test("constructor year-month-day-hour-min-sec", function() {
  TimeShift.setTimezoneOffset(-180);
  var d = new TimeShift.Date(2012, MAR, 5, 23, 45, 12);  // Mon 2012-03-05 23:45:12 +0300
  matches(d, 2012, MAR, 5, 23, 45, 12, 000, MON);
  matchesUTC(d, 2012, MAR, 5, 20, 45, 12, 000, MON);
});

test("constructor year-month-day-hour-min-sec-msec", function() {
  TimeShift.setTimezoneOffset(-180);
  var d = new TimeShift.Date(2012, MAR, 5, 23, 45, 12, 23);  // Mon 2012-03-05 23:45:12.023 +0300
  matches(d, 2012, MAR, 5, 23, 45, 12, 23, MON);
  matchesUTC(d, 2012, MAR, 5, 20, 45, 12, 23, MON);
});

test("constructor year-month with timezone shift over month border", function() {
  TimeShift.setTimezoneOffset(-120);
  var d = new TimeShift.Date(2012, MAR);  // Thu 2012-03-01 00:00:00 +0200
  matches(d, 2012, MAR, 1, 00, 00, 00, 000, THU);
  matchesUTC(d, 2012, FEB, 29, 22, 00, 00, 000, WED);
});

test("timezone shift over year boundary", function() {
  TimeShift.setTimezoneOffset(-630);
  var d = new TimeShift.Date(1356989624234);  // Mon 2012-12-31 21:33:44.234 UTC
  matches(d, 2013, JAN, 1, 8, 03, 44, 234, TUE);
  matchesUTC(d, 2012, DEC, 31, 21, 33, 44, 234, MON);
});


///////////////////  Setter tests  /////////////////////

test("set test precondition", function() {
  TimeShift.setTimezoneOffset(-300);
  var d = new TimeShift.Date(2012, MAR, 5, 23, 45, 12);  // Mon 2012-03-05 23:45:12 +0500
  matches(d, 2012, MAR, 5, 23, 45, 12, 000, MON);
  matchesUTC(d, 2012, MAR, 5, 18, 45, 12, 000, MON);
});

//// Local time

test("setFullYear(year)", function() {
  TimeShift.setTimezoneOffset(-300);
  var d = new TimeShift.Date(2012, MAR, 5, 23, 45, 12);  // Mon 2012-03-05 23:45:12 +0500
  d.setFullYear(2011);
  matches(d, 2011, MAR, 5, 23, 45, 12, 000, SAT);
  matchesUTC(d, 2011, MAR, 5, 18, 45, 12, 000, SAT);
});

test("setFullYear(year, month)", function() {
  TimeShift.setTimezoneOffset(-300);
  var d = new TimeShift.Date(2012, MAR, 5, 23, 45, 12);  // Mon 2012-03-05 23:45:12 +0500
  d.setFullYear(2011, APR);
  matches(d, 2011, APR, 5, 23, 45, 12, 000, TUE);
  matchesUTC(d, 2011, APR, 5, 18, 45, 12, 000, TUE);
});

test("setFullYear(year, month, day)", function() {
  TimeShift.setTimezoneOffset(-300);
  var d = new TimeShift.Date(2012, MAR, 5, 23, 45, 12);  // Mon 2012-03-05 23:45:12 +0500
  d.setFullYear(2011, APR, 7);
  matches(d, 2011, APR, 7, 23, 45, 12, 000, THU);
  matchesUTC(d, 2011, APR, 7, 18, 45, 12, 000, THU);
});

test("setYear(year)", function() {
  TimeShift.setTimezoneOffset(-300);
  var d = new TimeShift.Date(2012, MAR, 5, 23, 45, 12);  // Mon 2012-03-05 23:45:12 +0500
  d.setYear(111);
  matches(d, 2011, MAR, 5, 23, 45, 12, 000, SAT);
  matchesUTC(d, 2011, MAR, 5, 18, 45, 12, 000, SAT);
});

test("setMonth(month)", function() {
  TimeShift.setTimezoneOffset(-300);
  var d = new TimeShift.Date(2012, MAR, 5, 23, 45, 12);  // Mon 2012-03-05 23:45:12 +0500
  d.setMonth(APR);
  matches(d, 2012, APR, 5, 23, 45, 12, 000, THU);
  matchesUTC(d, 2012, APR, 5, 18, 45, 12, 000, THU);
});

test("setMonth(month, day)", function() {
  TimeShift.setTimezoneOffset(-300);
  var d = new TimeShift.Date(2012, MAR, 5, 23, 45, 12);  // Mon 2012-03-05 23:45:12 +0500
  d.setMonth(APR, 7);
  matches(d, 2012, APR, 7, 23, 45, 12, 000, SAT);
  matchesUTC(d, 2012, APR, 7, 18, 45, 12, 000, SAT);
});

test("setDate(day)", function() {
  TimeShift.setTimezoneOffset(-300);
  var d = new TimeShift.Date(2012, MAR, 5, 23, 45, 12);  // Mon 2012-03-05 23:45:12 +0500
  d.setDate(7);
  matches(d, 2012, MAR, 7, 23, 45, 12, 000, WED);
  matchesUTC(d, 2012, MAR, 7, 18, 45, 12, 000, WED);
});

test("setHours(hour)", function() {
  TimeShift.setTimezoneOffset(-300);
  var d = new TimeShift.Date(2012, MAR, 5, 23, 45, 12);  // Mon 2012-03-05 23:45:12 +0500
  d.setHours(20);
  matches(d, 2012, MAR, 5, 20, 45, 12, 000, MON);
  matchesUTC(d, 2012, MAR, 5, 15, 45, 12, 000, MON);
});

test("setHours(hour, min)", function() {
  TimeShift.setTimezoneOffset(-300);
  var d = new TimeShift.Date(2012, MAR, 5, 23, 45, 12);  // Mon 2012-03-05 23:45:12 +0500
  d.setHours(20, 51);
  matches(d, 2012, MAR, 5, 20, 51, 12, 000, MON);
  matchesUTC(d, 2012, MAR, 5, 15, 51, 12, 000, MON);
});

test("setHours(hour, min, sec)", function() {
  TimeShift.setTimezoneOffset(-300);
  var d = new TimeShift.Date(2012, MAR, 5, 23, 45, 12);  // Mon 2012-03-05 23:45:12 +0500
  d.setHours(20, 51, 22);
  matches(d, 2012, MAR, 5, 20, 51, 22, 000, MON);
  matchesUTC(d, 2012, MAR, 5, 15, 51, 22, 000, MON);
});

test("setHours(hour, min, sec, msec)", function() {
  TimeShift.setTimezoneOffset(-300);
  var d = new TimeShift.Date(2012, MAR, 5, 23, 45, 12);  // Mon 2012-03-05 23:45:12 +0500
  d.setHours(20, 51, 22, 123);
  matches(d, 2012, MAR, 5, 20, 51, 22, 123, MON);
  matchesUTC(d, 2012, MAR, 5, 15, 51, 22, 123, MON);
});

test("setMinutes(min)", function() {
  TimeShift.setTimezoneOffset(-300);
  var d = new TimeShift.Date(2012, MAR, 5, 23, 45, 12);  // Mon 2012-03-05 23:45:12 +0500
  d.setMinutes(52);
  matches(d, 2012, MAR, 5, 23, 52, 12, 000, MON);
  matchesUTC(d, 2012, MAR, 5, 18, 52, 12, 000, MON);
});

test("setMinutes(min, sec)", function() {
  TimeShift.setTimezoneOffset(-300);
  var d = new TimeShift.Date(2012, MAR, 5, 23, 45, 12);  // Mon 2012-03-05 23:45:12 +0500
  d.setMinutes(52, 22);
  matches(d, 2012, MAR, 5, 23, 52, 22, 000, MON);
  matchesUTC(d, 2012, MAR, 5, 18, 52, 22, 000, MON);
});

test("setMinutes(min, sec, msec)", function() {
  TimeShift.setTimezoneOffset(-300);
  var d = new TimeShift.Date(2012, MAR, 5, 23, 45, 12);  // Mon 2012-03-05 23:45:12 +0500
  d.setMinutes(52, 22, 123);
  matches(d, 2012, MAR, 5, 23, 52, 22, 123, MON);
  matchesUTC(d, 2012, MAR, 5, 18, 52, 22, 123, MON);
});

test("setSeconds(sec)", function() {
  TimeShift.setTimezoneOffset(-300);
  var d = new TimeShift.Date(2012, MAR, 5, 23, 45, 12);  // Mon 2012-03-05 23:45:12 +0500
  d.setSeconds(22);
  matches(d, 2012, MAR, 5, 23, 45, 22, 000, MON);
  matchesUTC(d, 2012, MAR, 5, 18, 45, 22, 000, MON);
});

test("setSeconds(sec, msec)", function() {
  TimeShift.setTimezoneOffset(-300);
  var d = new TimeShift.Date(2012, MAR, 5, 23, 45, 12);  // Mon 2012-03-05 23:45:12 +0500
  d.setSeconds(22, 123);
  matches(d, 2012, MAR, 5, 23, 45, 22, 123, MON);
  matchesUTC(d, 2012, MAR, 5, 18, 45, 22, 123, MON);
});

test("setMilliseconds(msec)", function() {
  TimeShift.setTimezoneOffset(-300);
  var d = new TimeShift.Date(2012, MAR, 5, 23, 45, 12);  // Mon 2012-03-05 23:45:12 +0500
  d.setMilliseconds(123);
  matches(d, 2012, MAR, 5, 23, 45, 12, 123, MON);
  matchesUTC(d, 2012, MAR, 5, 18, 45, 12, 123, MON);
});


//// UTC time

test("setUTCFullYear(year)", function() {
  TimeShift.setTimezoneOffset(-300);
  var d = new TimeShift.Date(2012, MAR, 5, 23, 45, 12);  // Mon 2012-03-05 23:45:12 +0500
  d.setUTCFullYear(2011);
  matches(d, 2011, MAR, 5, 23, 45, 12, 000, SAT);
  matchesUTC(d, 2011, MAR, 5, 18, 45, 12, 000, SAT);
});

test("setUTCFullYear(year, month)", function() {
  TimeShift.setTimezoneOffset(-300);
  var d = new TimeShift.Date(2012, MAR, 5, 23, 45, 12);  // Mon 2012-03-05 23:45:12 +0500
  d.setUTCFullYear(2011, APR);
  matches(d, 2011, APR, 5, 23, 45, 12, 000, TUE);
  matchesUTC(d, 2011, APR, 5, 18, 45, 12, 000, TUE);
});

test("setUTCFullYear(year, month, day)", function() {
  TimeShift.setTimezoneOffset(-300);
  var d = new TimeShift.Date(2012, MAR, 5, 23, 45, 12);  // Mon 2012-03-05 23:45:12 +0500
  d.setUTCFullYear(2011, APR, 7);
  matches(d, 2011, APR, 7, 23, 45, 12, 000, THU);
  matchesUTC(d, 2011, APR, 7, 18, 45, 12, 000, THU);
});

test("setUTCMonth(month)", function() {
  TimeShift.setTimezoneOffset(-300);
  var d = new TimeShift.Date(2012, MAR, 5, 23, 45, 12);  // Mon 2012-03-05 23:45:12 +0500
  d.setUTCMonth(APR);
  matches(d, 2012, APR, 5, 23, 45, 12, 000, THU);
  matchesUTC(d, 2012, APR, 5, 18, 45, 12, 000, THU);
});

test("setUTCMonth(month, day)", function() {
  TimeShift.setTimezoneOffset(-300);
  var d = new TimeShift.Date(2012, MAR, 5, 23, 45, 12);  // Mon 2012-03-05 23:45:12 +0500
  d.setUTCMonth(APR, 7);
  matches(d, 2012, APR, 7, 23, 45, 12, 000, SAT);
  matchesUTC(d, 2012, APR, 7, 18, 45, 12, 000, SAT);
});

test("setUTCDate(day)", function() {
  TimeShift.setTimezoneOffset(-300);
  var d = new TimeShift.Date(2012, MAR, 5, 23, 45, 12);  // Mon 2012-03-05 23:45:12 +0500
  d.setUTCDate(7);
  matches(d, 2012, MAR, 7, 23, 45, 12, 000, WED);
  matchesUTC(d, 2012, MAR, 7, 18, 45, 12, 000, WED);
});

test("setUTCHours(hour)", function() {
  TimeShift.setTimezoneOffset(-300);
  var d = new TimeShift.Date(2012, MAR, 5, 23, 45, 12);  // Mon 2012-03-05 23:45:12 +0500
  d.setUTCHours(10);
  matches(d, 2012, MAR, 5, 15, 45, 12, 000, MON);
  matchesUTC(d, 2012, MAR, 5, 10, 45, 12, 000, MON);
});

test("setUTCHours(hour, min)", function() {
  TimeShift.setTimezoneOffset(-300);
  var d = new TimeShift.Date(2012, MAR, 5, 23, 45, 12);  // Mon 2012-03-05 23:45:12 +0500
  d.setUTCHours(10, 51);
  matches(d, 2012, MAR, 5, 15, 51, 12, 000, MON);
  matchesUTC(d, 2012, MAR, 5, 10, 51, 12, 000, MON);
});

test("setUTCHours(hour, min, sec)", function() {
  TimeShift.setTimezoneOffset(-300);
  var d = new TimeShift.Date(2012, MAR, 5, 23, 45, 12);  // Mon 2012-03-05 23:45:12 +0500
  d.setUTCHours(10, 51, 22);
  matches(d, 2012, MAR, 5, 15, 51, 22, 000, MON);
  matchesUTC(d, 2012, MAR, 5, 10, 51, 22, 000, MON);
});

test("setUTCHours(hour, min, sec, msec)", function() {
  TimeShift.setTimezoneOffset(-300);
  var d = new TimeShift.Date(2012, MAR, 5, 23, 45, 12);  // Mon 2012-03-05 23:45:12 +0500
  d.setUTCHours(10, 51, 22, 123);
  matches(d, 2012, MAR, 5, 15, 51, 22, 123, MON);
  matchesUTC(d, 2012, MAR, 5, 10, 51, 22, 123, MON);
});

test("setUTCMinutes(min)", function() {
  TimeShift.setTimezoneOffset(-300);
  var d = new TimeShift.Date(2012, MAR, 5, 23, 45, 12);  // Mon 2012-03-05 23:45:12 +0500
  d.setUTCMinutes(52);
  matches(d, 2012, MAR, 5, 23, 52, 12, 000, MON);
  matchesUTC(d, 2012, MAR, 5, 18, 52, 12, 000, MON);
});

test("setUTCMinutes(min, sec)", function() {
  TimeShift.setTimezoneOffset(-300);
  var d = new TimeShift.Date(2012, MAR, 5, 23, 45, 12);  // Mon 2012-03-05 23:45:12 +0500
  d.setUTCMinutes(52, 22);
  matches(d, 2012, MAR, 5, 23, 52, 22, 000, MON);
  matchesUTC(d, 2012, MAR, 5, 18, 52, 22, 000, MON);
});

test("setUTCMinutes(min, sec, msec)", function() {
  TimeShift.setTimezoneOffset(-300);
  var d = new TimeShift.Date(2012, MAR, 5, 23, 45, 12);  // Mon 2012-03-05 23:45:12 +0500
  d.setUTCMinutes(52, 22, 123);
  matches(d, 2012, MAR, 5, 23, 52, 22, 123, MON);
  matchesUTC(d, 2012, MAR, 5, 18, 52, 22, 123, MON);
});

test("setUTCSeconds(sec)", function() {
  TimeShift.setTimezoneOffset(-300);
  var d = new TimeShift.Date(2012, MAR, 5, 23, 45, 12);  // Mon 2012-03-05 23:45:12 +0500
  d.setUTCSeconds(22);
  matches(d, 2012, MAR, 5, 23, 45, 22, 000, MON);
  matchesUTC(d, 2012, MAR, 5, 18, 45, 22, 000, MON);
});

test("setUTCSeconds(sec, msec)", function() {
  TimeShift.setTimezoneOffset(-300);
  var d = new TimeShift.Date(2012, MAR, 5, 23, 45, 12);  // Mon 2012-03-05 23:45:12 +0500
  d.setUTCSeconds(22, 123);
  matches(d, 2012, MAR, 5, 23, 45, 22, 123, MON);
  matchesUTC(d, 2012, MAR, 5, 18, 45, 22, 123, MON);
});

test("setUTCMilliseconds(msec)", function() {
  TimeShift.setTimezoneOffset(-300);
  var d = new TimeShift.Date(2012, MAR, 5, 23, 45, 12);  // Mon 2012-03-05 23:45:12 +0500
  d.setUTCMilliseconds(123);
  matches(d, 2012, MAR, 5, 23, 45, 12, 123, MON);
  matchesUTC(d, 2012, MAR, 5, 18, 45, 12, 123, MON);
});

//// Other setters

test("setTime(time)", function() {
  TimeShift.setTimezoneOffset(-300);
  var d = new TimeShift.Date(2012, MAR, 5, 23, 45, 12);  // Mon 2012-03-05 23:45:12 +0500
  d.setTime(1375991584123);  // Thu 2013-08-08 19:53:04.123 UTC
  matches(d, 2013, AUG, 9, 00, 53, 04, 123, FRI);
  matchesUTC(d, 2013, AUG, 8, 19, 53, 04, 123, THU);
});


////////////////////  Other functionality  ////////////////////

test("getTime(), valueOf()", function() {
  TimeShift.setTimezoneOffset(-300);
  var d = new TimeShift.Date(2012, MAR, 5, 23, 45, 12, 123);  // Mon 2012-03-05 23:45:12.123 +0500
  equal(d.getTime(), 1330973112123);
  equal(d.valueOf(), 1330973112123);
});

test("getTimezoneOffset()", function() {
  TimeShift.setTimezoneOffset(-300);
  var d = new TimeShift.Date(2012, MAR, 5, 23, 45, 12, 123);  // Mon 2012-03-05 23:45:12.123 +0500
  equal(d.getTimezoneOffset(), -300);
  TimeShift.setTimezoneOffset(-650);
  equal(d.getTimezoneOffset(), -650);
  TimeShift.setTimezoneOffset(550);
  equal(d.getTimezoneOffset(), 550);
});


/////////////////////  "Class" functions  ////////////////////

test("Date.now(); no time set", function() {
  TimeShift.setTimezoneOffset(-120);
  var now = Date.now();
  var t = TimeShift.Date.now();
  ok(now - t < 500);
});

test("Date.now(); time set", function() {
  TimeShift.setTimezoneOffset(-120);
  TimeShift.setTime(1375991584123);  // Thu 2013-08-08 19:53:04.123 UTC
  var t = TimeShift.Date.now();
  equal(t, 1375991584123);
  TimeShift.setTime(undefined);
});

test("Date.now(); reset to normal time", function() {
  TimeShift.setTimezoneOffset(-120);
  TimeShift.setTime(1375991584123);  // Thu 2013-08-08 19:53:04.123 UTC
  TimeShift.setTime(undefined);
  var now = Date.now();
  var t = TimeShift.Date.now();
  ok(now - t < 500);
});

test("Date.UTC", function() {
  TimeShift.setTimezoneOffset(-120);
  var t = TimeShift.Date.UTC(2012, MAR, 5, 23, 45, 12, 123);  // Mon 2012-03-05 23:45:12 UTC
  equal(t, 1330991112123);
});

// Date.parse is probably not correct


/////////////////////  String functions (approximate)  /////////////////////

test("toString()", function() {
  TimeShift.setTimezoneOffset(-300);
  var d = new TimeShift.Date(2012, MAR, 5, 6, 7, 8, 123);  // Mon 2012-03-05 06:07:08.123 +0500
  equal(d.toString(), "Mon Mar 05 2012 06:07:08 GMT+0500");
});

test("toString() 2", function() {
  TimeShift.setTimezoneOffset(300);
  var d = new TimeShift.Date(2012, DEC, 29, 6, 7, 8, 123);  // Sat 2012-12-29 06:07:08.123 -0500
  equal(d.toString(), "Sat Dec 29 2012 06:07:08 GMT-0500");
});

test("toString() 3", function() {
  TimeShift.setTimezoneOffset(0);
  var d = new TimeShift.Date(2012, JAN, 1, 6, 7, 8, 123);  // Sun 2012-01-01 06:07:08.123 GMT
  equal(d.toString(), "Sun Jan 01 2012 06:07:08 GMT");
});

test("toUTCString()", function() {
  TimeShift.setTimezoneOffset(-300);
  var d = new TimeShift.Date(2012, MAR, 5, 23, 45, 12, 123);  // Mon 2012-03-05 23:45:12.123 +0500
  // IE has slightly own format
  ok(d.toUTCString().match(/^Mon, 0?5 Mar 2012 18:45:12 (GMT|UTC)$/));
});

test("toISOString()", function() {
  TimeShift.setTimezoneOffset(-300);
  var d = new TimeShift.Date(2012, MAR, 5, 23, 45, 12, 123);  // Mon 2012-03-05 23:45:12.123 +0500
  equal(d.toISOString(), "2012-03-05T18:45:12.123Z");
});

test("toJSON()", function() {
  TimeShift.setTimezoneOffset(-300);
  var d = new TimeShift.Date(2012, MAR, 5, 23, 45, 12, 123);  // Mon 2012-03-05 23:45:12.123 +0500
  equal(d.toJSON(), "2012-03-05T18:45:12.123Z");
});

