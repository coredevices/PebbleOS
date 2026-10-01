/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// List of specific node timers. These are started and stopped using PROFILER_NODE_START
// and PROFILER_NODE_STOP
PROFILER_NODE(mic)
PROFILER_NODE(framebuffer_dma)
PROFILER_NODE(render_modal)
PROFILER_NODE(render_app)
PROFILER_NODE(dirty_rect)
PROFILER_NODE(gfx_test_update_proc)
PROFILER_NODE(voice_encode)
PROFILER_NODE(compositor)
PROFILER_NODE(hrm_handling)
PROFILER_NODE(display_transfer)
PROFILER_NODE(text_render_flash)
PROFILER_NODE(text_render_compress)
