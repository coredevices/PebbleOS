# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

class PebbleCommanderError(Exception):
    pass


class ParameterError(PebbleCommanderError):
    pass


class PromptResponseError(PebbleCommanderError):
    pass
