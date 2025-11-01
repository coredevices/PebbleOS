# Copyright 2024 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

chalk_platform = {
    "NAME": "chalk",
    "MAX_APP_BINARY_SIZE": 0x10000,  # 64K
    "MAX_APP_MEMORY_SIZE": 0x10000,  # 64K
    "MAX_WORKER_MEMORY_SIZE": 0x2800,  # 10K
    "MAX_RESOURCES_SIZE_APPSTORE": 0x40000,  # 256K
    "MAX_RESOURCES_SIZE": 0x100000,  # 1024K
    "DEFINES": ["PBL_PLATFORM_CHALK", "PBL_COLOR", "PBL_ROUND",
                "PBL_MICROPHONE", "PBL_SMARTSTRAP", "PBL_HEALTH",
                "PBL_COMPASS", "PBL_SMARTSTRAP_POWER",
                "PBL_DISPLAY_WIDTH=180", "PBL_DISPLAY_HEIGHT=180"],
    "BUILD_DIR": "chalk",
    "BUNDLE_BIN_DIR": "chalk",
    "ADDITIONAL_TEXT_LINES_FOR_PEBBLE_H": [],
    "MAX_FONT_GLYPH_SIZE": 256,
    "TAGS": ["chalk", "color", "round", "mic", "strap", "strappower",
             "compass", "health", "180w", "180h"]
}

basalt_platform = {
    "NAME": "basalt",
    "MAX_APP_BINARY_SIZE": 0x10000,  # 64K
    "MAX_APP_MEMORY_SIZE": 0x10000,  # 64K
    "MAX_WORKER_MEMORY_SIZE": 0x2800,  # 10K
    "MAX_RESOURCES_SIZE_APPSTORE": 0x40000,  # 256K
    "MAX_RESOURCES_SIZE": 0x100000,  # 1024K
    "DEFINES": ["PBL_PLATFORM_BASALT", "PBL_COLOR", "PBL_RECT",
                "PBL_MICROPHONE", "PBL_SMARTSTRAP", "PBL_HEALTH",
                "PBL_COMPASS", "PBL_SMARTSTRAP_POWER",
                "PBL_DISPLAY_WIDTH=144", "PBL_DISPLAY_HEIGHT=168"],
    "BUILD_DIR": "basalt",
    "BUNDLE_BIN_DIR": "basalt",
    "ADDITIONAL_TEXT_LINES_FOR_PEBBLE_H": [],
    "MAX_FONT_GLYPH_SIZE": 256,
    "TAGS": ["basalt", "color", "rect", "mic", "strap", "strappower",
             "compass", "health", "144w", "168h"],
}

aplite_platform = {
    "NAME": "aplite",
    "MAX_APP_BINARY_SIZE": 0x10000,  # 64K
    "MAX_APP_MEMORY_SIZE": 0x6000,  # 24K
    "MAX_WORKER_MEMORY_SIZE": 0x2800,  # 10K
    "MAX_RESOURCES_SIZE_APPSTORE": 0x20000,  # 128K
    "MAX_RESOURCES_SIZE_APPSTORE_2_X": 0x18000,  # 96K
    "MAX_RESOURCES_SIZE": 0x80000,  # 512K
    "DEFINES": ["PBL_PLATFORM_APLITE", "PBL_BW", "PBL_RECT", "PBL_COMPASS",
                "PBL_DISPLAY_WIDTH=144", "PBL_DISPLAY_HEIGHT=168"],
    "BUILD_DIR": "aplite",
    "BUNDLE_BIN_DIR": "aplite",
    "ADDITIONAL_TEXT_LINES_FOR_PEBBLE_H": [],
    "MAX_FONT_GLYPH_SIZE": 256,
    "TAGS": ["aplite", "bw", "rect", "compass", "144w", "168h"],
}

diorite_platform = {
    "NAME": "diorite",
    "MAX_APP_BINARY_SIZE": 0x10000,  # 64K
    "MAX_APP_MEMORY_SIZE": 0x10000,  # 64K
    "MAX_WORKER_MEMORY_SIZE": 0x2800,  # 10K
    "MAX_RESOURCES_SIZE_APPSTORE": 0x40000,  # 256K
    "MAX_RESOURCES_SIZE": 0x100000,  # 1024K
    "DEFINES": ["PBL_PLATFORM_DIORITE", "PBL_BW", "PBL_RECT",
                "PBL_MICROPHONE", "PBL_HEALTH", "PBL_SMARTSTRAP",
                "PBL_DISPLAY_WIDTH=144", "PBL_DISPLAY_HEIGHT=168"],
    "BUILD_DIR": "diorite",
    "BUNDLE_BIN_DIR": "diorite",
    "ADDITIONAL_TEXT_LINES_FOR_PEBBLE_H": [],
    "MAX_FONT_GLYPH_SIZE": 256,
    "TAGS": ["diorite", "bw", "rect", "mic", "strap", "health", "144w", "168h"]
}

emery_platform = {
    "NAME": "emery",
    "MAX_APP_BINARY_SIZE": 0x20000,  # 128K
    "MAX_APP_MEMORY_SIZE": 0x20000,  # 128K
    "MAX_WORKER_MEMORY_SIZE": 0x2800,  # 10K
    "MAX_RESOURCES_SIZE_APPSTORE": 0x40000,  # 256K
    "MAX_RESOURCES_SIZE": 0x100000,  # 1024K
    "DEFINES": ["PBL_PLATFORM_EMERY", "PBL_COLOR", "PBL_RECT",
                "PBL_MICROPHONE", "PBL_SMARTSTRAP", "PBL_HEALTH",
                "PBL_SMARTSTRAP_POWER", "PBL_COMPASS",
                "PBL_DISPLAY_WIDTH=200", "PBL_DISPLAY_HEIGHT=228"],
    "BUILD_DIR": "emery",
    "BUNDLE_BIN_DIR": "emery",
    "ADDITIONAL_TEXT_LINES_FOR_PEBBLE_H": [],
    "MAX_FONT_GLYPH_SIZE": 320,
    "TAGS": ["emery", "color", "rect", "mic", "strap", "health", "strappower",
             "compass", "200w", "228h"]
}

flint_platform = {
    "NAME": "flint",
    "MAX_APP_BINARY_SIZE": 0x10000,  # 64K
    "MAX_APP_MEMORY_SIZE": 0x10000,  # 64K
    "MAX_WORKER_MEMORY_SIZE": 0x2800,  # 10K
    "MAX_RESOURCES_SIZE_APPSTORE": 0x40000,  # 256K
    "MAX_RESOURCES_SIZE": 0x100000,  # 1024K
    "DEFINES": ["PBL_PLATFORM_FLINT", "PBL_BW", "PBL_RECT",
                "PBL_MICROPHONE", "PBL_HEALTH", "PBL_COMPASS"
                "PBL_DISPLAY_WIDTH=144", "PBL_DISPLAY_HEIGHT=168"],
    "BUILD_DIR": "flint",
    "BUNDLE_BIN_DIR": "flint",
    "ADDITIONAL_TEXT_LINES_FOR_PEBBLE_H": [],
    "MAX_FONT_GLYPH_SIZE": 256,
    "TAGS": ["flint", "bw", "rect", "mic", "health", "compass", "144w", "168h"],
}


pebble_platforms = {
    "flint": flint_platform,
    "emery": emery_platform,
    "diorite": diorite_platform,
    "chalk": chalk_platform,
    "basalt": basalt_platform,
    "aplite": aplite_platform,
}


# When this function is called from the firmware build, INTERNAL_SDK_BUILD will always
# have some value. If it's true, import internal; otherwise don't.
# If INTERNAL_SDK_BUILD doesn't exist at all, then we're in an SDK build and can assume
# that we should use the file if it exists, so try importing unconditionally.
def maybe_import_internal(env):
    if 'INTERNAL_SDK_BUILD' in env:
        if env.INTERNAL_SDK_BUILD:
            import pebble_sdk_platform_internal
    else:
        try:
            import pebble_sdk_platform_internal
        except ImportError:
            pass
