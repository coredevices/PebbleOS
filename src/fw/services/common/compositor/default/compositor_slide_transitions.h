/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/common/compositor/compositor.h"

const CompositorTransition *compositor_slide_transition_timeline_get(bool timeline_is_future,
                                                                     bool timeline_is_destination,
                                                                     bool timeline_is_empty);
