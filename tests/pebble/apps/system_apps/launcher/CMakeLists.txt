# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_launcher_menu_layer
  MODULES resources graphics_bitdepth gdraw png text menu_layer
  SOURCES
    pebble/applib/graphics/gdraw_command_transforms.c
    pebble/applib/graphics/text_render.c
    pebble/applib/graphics/text_resources.c
    pebble/applib/ui/animation_interpolate.c
    pebble/applib/ui/animation_timing.c
    pebble/applib/ui/layer.c
    pebble/applib/ui/shadows.c
    pebble/applib/ui/window.c
    subsys/logging/hexdump.c
    tests/fakes/fake_rtc.c
    pebble/applib/template_string.c
    pebble/applib/ui/content_indicator.c
    pebble/applib/ui/kino/kino_reel_custom.c
    pebble/applib/ui/menu_layer_system_cells.c
    pebble/applib/ui/menu_text_scroll.c
    pebble/shell/system_theme_style.c
    pebble/apps/system/launcher/default/app_glance.c
    pebble/apps/system/launcher/default/app_glance_alarms.c
    pebble/apps/system/launcher/default/app_glance_generic.c
    pebble/apps/system/launcher/default/app_glance_music.c
    pebble/apps/system/launcher/default/app_glance_notifications.c
    pebble/apps/system/launcher/default/app_glance_private.c
    pebble/apps/system/launcher/default/app_glance_service.c
    pebble/apps/system/launcher/default/app_glance_settings.c
    pebble/apps/system/launcher/default/app_glance_structured.c
    pebble/apps/system/launcher/default/app_glance_watchfaces.c
    pebble/apps/system/launcher/default/app_glance_weather.c
    pebble/apps/system/launcher/default/app_glance_workout.c
    pebble/apps/system/launcher/default/menu_layer.c
    pebble/apps/system/timeline/text_node.c
    tests/fakes/fake_round_display.c
    pebble/services/app_glances/app_glance_service.c
    pebble/services/blob_db/app_glance_db.c
    pebble/services/timeline/attribute.c
    tests/fakes/fake_fonts.c
    tests/fakes/fake_settings.c
    tests/fixtures/resources/timeline_resource_table.auto.c
    tests/fixtures/screen_grid.c
    tests/stubs/stubs_animation.c
    tests/stubs/stubs_app_menu_data_source.c
    pebble/applib/pbl_std/strftime.c
    pebble/applib/pbl_std/timelocal.c
    pebble/services/clock/time.c
  PLATFORMS emery gabbro flint
  OVERRIDES dummy_board
  TEST_IMAGES
)
