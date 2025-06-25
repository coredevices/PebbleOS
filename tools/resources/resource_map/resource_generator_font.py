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

from resources.types.resource_object import ResourceObject
from resources.resource_map.resource_generator import ResourceGenerator

from font.fontgen import Font, MAX_GLYPHS_EXTENDED, MAX_GLYPHS

from pebble_sdk_platform import pebble_platforms, maybe_import_internal

from threading import Lock

import os
import re


class FontResourceGenerator(ResourceGenerator):
    """
    ResourceGenerator for the 'font' type
    """

    type = 'font'
    lock = Lock()

    @staticmethod
    def definitions_from_dict(bld, definition_dict, resource_source_path):
        maybe_import_internal(bld.env)

        definitions = ResourceGenerator.definitions_from_dict(bld, definition_dict,
                                                              resource_source_path)

        # Parse additional font specific fields
        for d in definitions:
            d.max_glyph_size = pebble_platforms[bld.env.PLATFORM_NAME]['MAX_FONT_GLYPH_SIZE']
            d.character_list = definition_dict.get('characterList')
            d.character_regex = definition_dict.get('characterRegex')
            d.compatibility = definition_dict.get('compatibility')
            d.compress = definition_dict.get('compress')
            d.extended = bool(definition_dict.get('extended'))
            d.tracking_adjust = definition_dict.get('trackingAdjust')

        return definitions

    @classmethod
    def generate_object(cls, task, definition):
        font_path = task.inputs[0].abspath()
        font_ext = os.path.splitext(font_path)[-1]
        if font_ext in (".ttf", ".otf"):
            font_data = cls.build_font_data(font_path, definition)
        elif font_ext == ".pbf":
            font_data = open(font_path, "rb").read()
        else:
            raise Exception(f"Unsupported font format: {font_ext}")

        return ResourceObject(definition, font_data)

    @classmethod
    def build_font_data(cls, ttf_path, definition):
        # PBL-23964: it turns out that font generation is not thread-safe with freetype
        # 2.4 (and possibly later versions). To avoid running into this, we use a lock.
        with cls.lock:
            height = FontResourceGenerator._get_font_height_from_name(definition.name)
            is_legacy = definition.compatibility == "2.7"
            max_glyphs = MAX_GLYPHS_EXTENDED if definition.extended else MAX_GLYPHS

            font = Font(ttf_path, height, max_glyphs, definition.max_glyph_size, is_legacy)

            if definition.character_regex is not None:
                font.set_regex_filter(definition.character_regex)

            if definition.character_list is not None:
                font.set_codepoint_list(definition.character_list)

            if definition.compress:
                font.set_compression(definition.compress)

            if definition.tracking_adjust is not None:
                font.set_tracking_adjust(definition.tracking_adjust)

            font.build_tables()
            return font.bitstring()


    @staticmethod
    def _get_font_height_from_name(name):
        """
        Search the name of the font for an integer which will be used as the
        pixel height of the generated font
        """

        match = re.search('([0-9]+)', name)

        if match is None:
            if name != 'FONT_FALLBACK' and name != 'FONT_FALLBACK_INTERNAL':
                raise ValueError('Font {0}: no height found in name\n'.format(name))

            return 14

        return int(match.group(0))
