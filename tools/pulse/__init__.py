# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

from . import flash_imaging
from . import pulse_logging
from . import pulse_prompt
from .socket import Connection
from .exceptions import PulseError

Connection.register_extension('flash', flash_imaging.FlashImagingProtocol)
Connection.register_extension('logging', pulse_logging.LoggingProtocol)
Connection.register_extension('prompt', pulse_prompt.PromptProtocol)
