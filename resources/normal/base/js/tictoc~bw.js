/**
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

var rocky = _rocky;

var clockData = {
    time: '',
    date: ''
};

rocky.on('draw', function(drawEvent) {
  var ctx = drawEvent.context;
  var w = ctx.canvas.unobstructedWidth;
  var h = ctx.canvas.unobstructedHeight;
  var obstruction_h = ctx.canvas.clientHeight - ctx.canvas.unobstructedHeight - 3;

  ctx.clearRect(0, 0, ctx.canvas.clientWidth, ctx.canvas.clientHeight);


  ctx.fillStyle = '#FFF';
  ctx.textAlign = 'left';

  // TIME
  ctx.font = '49px Roboto-subset';
  ctx.fillText(clockData.time, 7, 89 - obstruction_h);

  // HORIZONTAL LINE
  ctx.fillRect(8, 94 - obstruction_h, w - 20, 2); // indented

  ctx.font = '21px Roboto';
  ctx.fillText(clockData.date, 8, 65 - obstruction_h);
});

rocky.on('minutechange', function(e) {
  var d = e.date;
  var localeTime = d.toLocaleTimeString().split(' '); // ['12:31:21', 'AM'] or ['00:31:21']
  clockData.time = localeTime[0].split(':').slice(0, 2).join(':'); // '12:31' or '00:31'

  var monthDate = d.toLocaleDateString(undefined, {month: 'long'});
  var dayDate = d.toLocaleDateString(undefined, {day: 'numeric'});
  // left pad with whitespace (to render exactly like the original)
  dayDate = dayDate.length > 1 ? dayDate : ' ' + dayDate;
  clockData.date = monthDate + ' ' + dayDate;

  rocky.requestDraw();
});
