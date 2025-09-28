/*
 * Copyright 2025 Core Devices LLC
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


#pragma once
 
extern bool audec_init(AudioDevice* audio_device);
extern void audec_start(AudioDevice* audio_device, AudioTransCB cb);
extern uint32_t audec_write(AudioDevice* audio_device, void *writeBuf, uint32_t size);
extern void audec_set_vol(AudioDevice* audio_device, int volume);
extern void audec_stop(AudioDevice* audio_device);