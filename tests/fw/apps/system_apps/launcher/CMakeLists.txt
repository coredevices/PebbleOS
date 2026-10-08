# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_launcher_menu_layer
  MODULES resources graphics_bitdepth gdraw png text menu_layer
  SOURCES
    fw/applib/graphics/gdraw_command_transforms.c
    fw/applib/graphics/text_render.c
    fw/applib/graphics/text_resources.c
    fw/applib/ui/animation_interpolate.c
    fw/applib/ui/animation_timing.c
    fw/applib/ui/layer.c
    fw/applib/ui/shadows.c
    fw/applib/ui/window.c
    fw/system/hexdump.c
    tests/fakes/fake_rtc.c
    fw/applib/template_string.c
    fw/applib/ui/content_indicator.c
    fw/applib/ui/kino/kino_reel_custom.c
    fw/applib/ui/menu_layer_system_cells.c
    fw/applib/ui/menu_text_scroll.c
    fw/shell/system_theme_style.c
    fw/apps/system/launcher/default/app_glance.c
    fw/apps/system/launcher/default/app_glance_alarms.c
    fw/apps/system/launcher/default/app_glance_generic.c
    fw/apps/system/launcher/default/app_glance_music.c
    fw/apps/system/launcher/default/app_glance_notifications.c
    fw/apps/system/launcher/default/app_glance_private.c
    fw/apps/system/launcher/default/app_glance_service.c
    fw/apps/system/launcher/default/app_glance_settings.c
    fw/apps/system/launcher/default/app_glance_structured.c
    fw/apps/system/launcher/default/app_glance_watchfaces.c
    fw/apps/system/launcher/default/app_glance_weather.c
    fw/apps/system/launcher/default/app_glance_workout.c
    fw/apps/system/launcher/default/menu_layer.c
    fw/apps/system/timeline/text_node.c
    fw/board/displays/display_getafix.c
    fw/services/app_glances/app_glance_service.c
    fw/services/blob_db/app_glance_db.c
    fw/services/timeline/attribute.c
    tests/fakes/fake_fonts.c
    tests/fakes/fake_settings_file.c
    tests/fixtures/resources/timeline_resource_table.auto.c
    tests/fixtures/screen_grid.c
    tests/stubs/stubs_animation.c
    tests/stubs/stubs_app_menu_data_source.c
    fw/applib/pbl_std/strftime.c
    fw/applib/pbl_std/timelocal.c
    fw/services/clock/time.c
  PLATFORMS obelix gabbro asterix
  OVERRIDES dummy_board
  TEST_IMAGES
)
