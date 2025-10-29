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

var WatchfaceHelper = function(date) {
  function clockwiseRad(fraction) {
    // TODO: figure out if this is actually correct orientation for Canvas APIs
    return (1.5 - fraction) * 2 * Math.PI;
  }

  date = date || new Date();
  var secondFraction = date.getSeconds() / 60;
  var minuteFraction = (date.getMinutes()) / 60;
  var hourFraction = (date.getHours() % 12 + minuteFraction) / 12;
  this.secondAngle = clockwiseRad(secondFraction);
  this.minuteAngle = clockwiseRad(minuteFraction);
  this.hourAngle = clockwiseRad(hourFraction);
};

/*global rocky, Rocky:false */
// in the future, we will replace the singleton
// `rocky` as well as the namespace `Rocky`, e.g.
// `Rocky.tween` and `Rocky.WatchfaceHelper` with modules

// book keeping so that we can easily animate the two hands for the watchface
// .scale/.angle are updated by tween/event handler (see below)
var renderState = {
  minute: {style: 'white', scale: 0.80, angle: 0},
  hour: {style: 'red', scale: 0.51, angle: 0}
};

// helper function for the draw function (see below)
// extracted as a standalone function to satisfy common believe in efficient JS code
// TODO: verify that this has actually any effect on byte code level
var drawHand = function(handState, ctx, cx, cy, maxRadius) {
  ctx.lineWidth = 8;
  ctx.strokeStyle = handState.style;
  ctx.beginPath();
  ctx.moveTo(cx, cy);
  ctx.lineTo(cx + Math.sin(handState.angle) * handState.scale * maxRadius,
             cy + Math.cos(handState.angle) * handState.scale * maxRadius);
  ctx.stroke();
};

// the 'draw' event is being emitted after each call to rocky.requestDraw() but
// at most once for each screen update, even if .requestDraw() is called frequently
// the 'draw' event might also fire at other meaningful times (e.g. upon launch)
rocky.on('draw', function(drawEvent) {
  var ctx = drawEvent.context;
  var w = ctx.canvas.unobstructedWidth;
  var h = ctx.canvas.unobstructedHeight;

  // clear canvas on each render
  ctx.fillStyle = 'black';
  ctx.fillRect(0, 0, ctx.canvas.clientWidth, ctx.canvas.clientHeight);

  // center point
  var cx = w / 2;
  var cy = h / 2;
  var maxRadius = Math.min(w, h - 2 * 10) / 2;
  drawHand(renderState.minute, ctx, cx, cy, maxRadius);
  drawHand(renderState.hour, ctx, cx, cy, maxRadius);

  // Draw a 12 o clock indicator
  drawHand({style: 'white', scale: 0, angle: 0}, ctx, cx, 8, 0);
  // overdraw center so that no white part of the minute hand is visible
  drawHand({style: 'red', scale: 0, angle: 0}, ctx, cx, cy, 0);
});

// listener is called on each full minute and once immediately after registration
rocky.on('minutechange', function(e) {
  // WatchfaceHelper will later be extracted as npm module
  var wfh = new WatchfaceHelper(e.date);
  renderState.minute.angle = wfh.minuteAngle;
  renderState.hour.angle = wfh.hourAngle;
  rocky.requestDraw();
});

console.log('TicToc launched');
