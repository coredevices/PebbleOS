# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

from resources.types.resource_object import ResourceObject
from resources.resource_map.resource_generator import ResourceGenerator

from pebble_sdk_platform import pebble_platforms

import bitmapgen

def _pbi_generator(task, definition, format_str):
    pb = bitmapgen.PebbleBitmap(task.inputs[0].abspath(), bitmap_format=format_str)
    return ResourceObject(definition, pb.convert_to_pbi())


class PbiResourceGenerator(ResourceGenerator):
    type = 'pbi'

    @staticmethod
    def generate_object(task, definition):
        return _pbi_generator(task, definition, 'bw')


class Pbi8ResourceGenerator(ResourceGenerator):
    type = 'pbi8'

    @staticmethod
    def generate_object(task, definition):
        env = task.generator.env

        format = 'color' if 'color' in pebble_platforms[env.PLATFORM_NAME]['TAGS'] else 'bw'

        return _pbi_generator(task, definition, format)

# This implementation is in the "pbi" file because it's implemented using pbis, even though it's
# named with "png" prefix and we have a "png" file.
class PngTransResourceGenerator(ResourceGenerator):
    type = 'png-trans'

    @staticmethod
    def generate_object(task, definition):
        if 'WHITE' in definition.name:
            color_map = bitmapgen.WHITE_COLOR_MAP
        elif 'BLACK' in definition.name:
            color_map = bitmapgen.BLACK_COLOR_MAP
        else:
            task.generator.bld.fatal('png-trans with neither white nor black in the name: ' +
                                      resource_definition.name)

        pb = bitmapgen.PebbleBitmap(task.inputs[0].abspath(), color_map=color_map)
        return ResourceObject(definition, pb.convert_to_pbi())
