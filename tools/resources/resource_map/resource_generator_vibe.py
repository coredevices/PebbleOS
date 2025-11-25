# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

from resources.types.resource_object import ResourceObject
from resources.resource_map.resource_generator import ResourceGenerator

from pebble_sdk_platform import pebble_platforms

import json2vibe

from io import BytesIO


class VibeResourceGenerator(ResourceGenerator):
    type = 'vibe'

    @staticmethod
    def generate_object(task, definition):
        out = BytesIO()
        json2vibe.convert_to_file(task.inputs[0].abspath(), out)

        return ResourceObject(definition, out.getvalue())
