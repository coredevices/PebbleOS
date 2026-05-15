# SPDX-FileCopyrightText: 2026 Google LLC
# SPDX-License-Identifier: Apache-2.0

import subprocess
import sys

from resources.resource_map.resource_generator import ResourceGenerator
from resources.types.resource_object import ResourceObject


class ResourceGeneratorMo(ResourceGenerator):
    type = "mo"

    @staticmethod
    def generate_object(task, definition):
        result = subprocess.run(
            ["msgfmt", "-c", "-o", "-", task.inputs[0].abspath()],
            check=False,
            capture_output=True,
        )
        if result.returncode:
            sys.stderr.buffer.write(result.stderr)
            result.check_returncode()

        return ResourceObject(definition, result.stdout)
