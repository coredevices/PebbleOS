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

// Set callback for the app ready event
Pebble.addEventListener("ready",
                        function(e) {
                          console.log("connected!" + e.ready);
                          console.log(e.type);
                        });

// Set callback for appmessage events
Pebble.addEventListener("appmessage",
                        function(e) {
                          console.log("sending reply");
                          Pebble.sendAppMessage({"test0": "42"});
                        });

