# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

from resources.types.resource_object import ResourceObject
from resources.resource_map.resource_generator import ResourceGenerator

class ResourceGeneratorRaw(ResourceGenerator):
    type = 'raw'

    @staticmethod
    def generate_object(task, definition):
        with open(task.inputs[0].abspath(), 'rb') as f:
            return ResourceObject(definition, f.read())
