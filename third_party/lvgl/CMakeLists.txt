# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

# LVGL takes its options from the CONFIG_LV_* symbols, not from an lv_conf.h
pbl_compile_definitions(LV_CONF_SKIP LV_LVGL_H_INCLUDE_SIMPLE)
pbl_include_directories(
  lvgl/include
  lvgl/include/lvgl
  lvgl
  port/include
)

# Only what the CONFIG_LV_* symbols turn on is compiled. The drivers of LVGL, its other
# renderers and what needs libraries that are not here are never built.
pbl_library_named(lvgl)
pbl_library_kind(stlib)
pbl_library_compile_options(-w)

# Core
pbl_library_sources_glob(
  lvgl/src/*.c
  lvgl/src/core/*.c
  lvgl/src/display/*.c
  lvgl/src/draw/*.c
  lvgl/src/draw/convert/*.c
  lvgl/src/font/lv_font.c
  lvgl/src/font/binfont_loader/*.c
  lvgl/src/font/fmt_txt/*.c
  lvgl/src/fs/lv_fs.c
  lvgl/src/image/lv_bin_decoder.c
  lvgl/src/image/lv_image_decoder.c
  lvgl/src/indev/*.c
  lvgl/src/layouts/*.c
  lvgl/src/logging/*.c
  lvgl/src/misc/*.c
  lvgl/src/misc/cache/*.c
  lvgl/src/misc/cache/class/*.c
  lvgl/src/misc/cache/instance/*.c
  lvgl/src/osal/lv_os.c
  lvgl/src/osal/lv_os_none.c
  lvgl/src/stdlib/*.c
  lvgl/src/themes/*.c
  lvgl/src/tick/*.c
)

# Standard library
pbl_library_sources_ifdef(CONFIG_LV_USE_BUILTIN_MALLOC
  lvgl/src/stdlib/builtin/lv_mem_core_builtin.c
  lvgl/src/stdlib/builtin/lv_tlsf.c
)
pbl_library_sources_ifdef(CONFIG_LV_USE_BUILTIN_STRING lvgl/src/stdlib/builtin/lv_string_builtin.c)
pbl_library_sources_ifdef(CONFIG_LV_USE_BUILTIN_SPRINTF
  lvgl/src/stdlib/builtin/lv_sprintf_builtin.c
)
pbl_library_sources_ifdef(CONFIG_LV_USE_CLIB_MALLOC lvgl/src/stdlib/clib/lv_mem_core_clib.c)
pbl_library_sources_ifdef(CONFIG_LV_USE_CLIB_STRING lvgl/src/stdlib/clib/lv_string_clib.c)
pbl_library_sources_ifdef(CONFIG_LV_USE_CLIB_SPRINTF lvgl/src/stdlib/clib/lv_sprintf_clib.c)

# Software renderer
if(CONFIG_LV_USE_DRAW_SW)
  pbl_library_sources_glob(lvgl/src/draw/sw/*.c lvgl/src/draw/sw/blend/*.c)
endif()
pbl_library_sources_ifdef(CONFIG_LV_USE_SNAPSHOT lvgl/src/draw/snapshot/lv_snapshot.c)

# Fonts
pbl_library_sources_ifdef(CONFIG_LV_FONT_DEJAVU_16_PERSIAN_HEBREW
  lvgl/src/font/lv_font_dejavu_16_persian_hebrew.c
)
pbl_library_sources_ifdef(CONFIG_LV_FONT_MONTSERRAT_8 lvgl/src/font/lv_font_montserrat_8.c)
pbl_library_sources_ifdef(CONFIG_LV_FONT_MONTSERRAT_10 lvgl/src/font/lv_font_montserrat_10.c)
pbl_library_sources_ifdef(CONFIG_LV_FONT_MONTSERRAT_12 lvgl/src/font/lv_font_montserrat_12.c)
pbl_library_sources_ifdef(CONFIG_LV_FONT_MONTSERRAT_14 lvgl/src/font/lv_font_montserrat_14.c)
pbl_library_sources_ifdef(CONFIG_LV_FONT_MONTSERRAT_16 lvgl/src/font/lv_font_montserrat_16.c)
pbl_library_sources_ifdef(CONFIG_LV_FONT_MONTSERRAT_18 lvgl/src/font/lv_font_montserrat_18.c)
pbl_library_sources_ifdef(CONFIG_LV_FONT_MONTSERRAT_20 lvgl/src/font/lv_font_montserrat_20.c)
pbl_library_sources_ifdef(CONFIG_LV_FONT_MONTSERRAT_22 lvgl/src/font/lv_font_montserrat_22.c)
pbl_library_sources_ifdef(CONFIG_LV_FONT_MONTSERRAT_24 lvgl/src/font/lv_font_montserrat_24.c)
pbl_library_sources_ifdef(CONFIG_LV_FONT_MONTSERRAT_26 lvgl/src/font/lv_font_montserrat_26.c)
pbl_library_sources_ifdef(CONFIG_LV_FONT_MONTSERRAT_28 lvgl/src/font/lv_font_montserrat_28.c)
pbl_library_sources_ifdef(CONFIG_LV_FONT_MONTSERRAT_30 lvgl/src/font/lv_font_montserrat_30.c)
pbl_library_sources_ifdef(CONFIG_LV_FONT_MONTSERRAT_32 lvgl/src/font/lv_font_montserrat_32.c)
pbl_library_sources_ifdef(CONFIG_LV_FONT_MONTSERRAT_34 lvgl/src/font/lv_font_montserrat_34.c)
pbl_library_sources_ifdef(CONFIG_LV_FONT_MONTSERRAT_36 lvgl/src/font/lv_font_montserrat_36.c)
pbl_library_sources_ifdef(CONFIG_LV_FONT_MONTSERRAT_38 lvgl/src/font/lv_font_montserrat_38.c)
pbl_library_sources_ifdef(CONFIG_LV_FONT_MONTSERRAT_40 lvgl/src/font/lv_font_montserrat_40.c)
pbl_library_sources_ifdef(CONFIG_LV_FONT_MONTSERRAT_42 lvgl/src/font/lv_font_montserrat_42.c)
pbl_library_sources_ifdef(CONFIG_LV_FONT_MONTSERRAT_44 lvgl/src/font/lv_font_montserrat_44.c)
pbl_library_sources_ifdef(CONFIG_LV_FONT_MONTSERRAT_46 lvgl/src/font/lv_font_montserrat_46.c)
pbl_library_sources_ifdef(CONFIG_LV_FONT_MONTSERRAT_48 lvgl/src/font/lv_font_montserrat_48.c)
pbl_library_sources_ifdef(CONFIG_LV_DEMO_BENCHMARK_ALIGNED_FONTS
  lvgl/src/font/lv_font_montserrat_14_aligned.c
)
pbl_library_sources_ifdef(CONFIG_LV_FONT_MONTSERRAT_28_COMPRESSED
  lvgl/src/font/lv_font_montserrat_28_compressed.c
)
pbl_library_sources_ifdef(CONFIG_LV_FONT_SOURCE_HAN_SANS_SC_14_CJK
  lvgl/src/font/lv_font_source_han_sans_sc_14_cjk.c
)
pbl_library_sources_ifdef(CONFIG_LV_FONT_SOURCE_HAN_SANS_SC_16_CJK
  lvgl/src/font/lv_font_source_han_sans_sc_16_cjk.c
)
pbl_library_sources_ifdef(CONFIG_LV_FONT_UNSCII_8 lvgl/src/font/lv_font_unscii_8.c)
pbl_library_sources_ifdef(CONFIG_LV_FONT_UNSCII_16 lvgl/src/font/lv_font_unscii_16.c)
pbl_library_sources_ifdef(CONFIG_LV_USE_FONT_MANAGER
  lvgl/src/font/font_manager/lv_font_manager.c
  lvgl/src/font/font_manager/lv_font_manager_recycle.c
)
pbl_library_sources_ifdef(CONFIG_LV_USE_IMGFONT lvgl/src/font/imgfont/lv_imgfont.c)

# File systems, image decoders and compression
pbl_library_sources_ifdef(CONFIG_LV_USE_FS_MEMFS lvgl/src/fs/lv_fs_memfs.c)
pbl_library_sources_ifdef(CONFIG_LV_USE_BMP lvgl/src/image/lv_bmp.c)
pbl_library_sources_ifdef(CONFIG_LV_USE_LODEPNG
  lvgl/src/image/lv_lodepng.c
  lvgl/src/libs/lodepng/lodepng.c
)
pbl_library_sources_ifdef(CONFIG_LV_USE_TJPGD lvgl/src/image/lv_tjpgd.c lvgl/src/libs/tjpgd/tjpgd.c)
pbl_library_sources_ifdef(CONFIG_LV_USE_SVG
  lvgl/src/image/svg/lv_svg.c
  lvgl/src/image/svg/lv_svg_decoder.c
  lvgl/src/image/svg/lv_svg_parser.c
  lvgl/src/image/svg/lv_svg_render.c
  lvgl/src/image/svg/lv_svg_token.c
)
pbl_library_sources_ifdef(CONFIG_LV_USE_LZ4_INTERNAL lvgl/src/libs/lz4/lz4.c)
pbl_library_sources_ifdef(CONFIG_LV_USE_RLE lvgl/src/libs/rle/lv_rle.c)

# Layouts and themes
pbl_library_sources_ifdef(CONFIG_LV_USE_FLEX lvgl/src/layouts/flex/lv_flex.c)
pbl_library_sources_ifdef(CONFIG_LV_USE_GRID lvgl/src/layouts/grid/lv_grid.c)
pbl_library_sources_ifdef(CONFIG_LV_USE_THEME_DEFAULT lvgl/src/themes/default/lv_theme_default.c)
pbl_library_sources_ifdef(CONFIG_LV_USE_THEME_MONO lvgl/src/themes/mono/lv_theme_mono.c)
pbl_library_sources_ifdef(CONFIG_LV_USE_THEME_SIMPLE lvgl/src/themes/simple/lv_theme_simple.c)

# Widgets
pbl_library_sources_ifdef(CONFIG_LV_USE_ANIMIMG lvgl/src/widgets/animimage/lv_animimage.c)
pbl_library_sources_ifdef(CONFIG_LV_USE_ARC lvgl/src/widgets/arc/lv_arc.c)
pbl_library_sources_ifdef(CONFIG_LV_USE_ARCLABEL lvgl/src/widgets/arclabel/lv_arclabel.c)
pbl_library_sources_ifdef(CONFIG_LV_USE_BAR lvgl/src/widgets/bar/lv_bar.c)
pbl_library_sources_ifdef(CONFIG_LV_USE_BARCODE
  lvgl/src/widgets/barcode/lv_barcode.c
  lvgl/src/libs/barcode/code128.c
)
pbl_library_sources_ifdef(CONFIG_LV_USE_BUTTON lvgl/src/widgets/button/lv_button.c)
pbl_library_sources_ifdef(CONFIG_LV_USE_BUTTONMATRIX
  lvgl/src/widgets/buttonmatrix/lv_buttonmatrix.c
)
pbl_library_sources_ifdef(CONFIG_LV_USE_CALENDAR
  lvgl/src/widgets/calendar/lv_calendar.c
  lvgl/src/widgets/calendar/lv_calendar_chinese.c
  lvgl/src/widgets/calendar/lv_calendar_header_arrow.c
  lvgl/src/widgets/calendar/lv_calendar_header_dropdown.c
)
pbl_library_sources_ifdef(CONFIG_LV_USE_CANVAS lvgl/src/widgets/canvas/lv_canvas.c)
pbl_library_sources_ifdef(CONFIG_LV_USE_CHART lvgl/src/widgets/chart/lv_chart.c)
pbl_library_sources_ifdef(CONFIG_LV_USE_CHECKBOX lvgl/src/widgets/checkbox/lv_checkbox.c)
pbl_library_sources_ifdef(CONFIG_LV_USE_DROPDOWN lvgl/src/widgets/dropdown/lv_dropdown.c)
pbl_library_sources_ifdef(CONFIG_LV_USE_GIF lvgl/src/widgets/gif/lv_gif.c lvgl/src/libs/gif/gif.c)
pbl_library_sources_ifdef(CONFIG_LV_USE_IMAGE lvgl/src/widgets/image/lv_image.c)
pbl_library_sources_ifdef(CONFIG_LV_USE_IMAGEBUTTON lvgl/src/widgets/imagebutton/lv_imagebutton.c)
pbl_library_sources_ifdef(CONFIG_LV_USE_IME_PINYIN lvgl/src/widgets/ime/lv_ime_pinyin.c)
pbl_library_sources_ifdef(CONFIG_LV_USE_KEYBOARD lvgl/src/widgets/keyboard/lv_keyboard.c)
pbl_library_sources_ifdef(CONFIG_LV_USE_LABEL lvgl/src/widgets/label/lv_label.c)
pbl_library_sources_ifdef(CONFIG_LV_USE_LED lvgl/src/widgets/led/lv_led.c)
pbl_library_sources_ifdef(CONFIG_LV_USE_LINE lvgl/src/widgets/line/lv_line.c)
pbl_library_sources_ifdef(CONFIG_LV_USE_LIST lvgl/src/widgets/list/lv_list.c)
pbl_library_sources_ifdef(CONFIG_LV_USE_MENU lvgl/src/widgets/menu/lv_menu.c)
pbl_library_sources_ifdef(CONFIG_LV_USE_MSGBOX lvgl/src/widgets/msgbox/lv_msgbox.c)
pbl_library_sources_ifdef(CONFIG_LV_USE_OBJ_PROPERTY
  lvgl/src/widgets/property/lv_animimage_properties.c
  lvgl/src/widgets/property/lv_arc_properties.c
  lvgl/src/widgets/property/lv_bar_properties.c
  lvgl/src/widgets/property/lv_buttonmatrix_properties.c
  lvgl/src/widgets/property/lv_chart_properties.c
  lvgl/src/widgets/property/lv_checkbox_properties.c
  lvgl/src/widgets/property/lv_dropdown_properties.c
  lvgl/src/widgets/property/lv_image_properties.c
  lvgl/src/widgets/property/lv_keyboard_properties.c
  lvgl/src/widgets/property/lv_label_properties.c
  lvgl/src/widgets/property/lv_led_properties.c
  lvgl/src/widgets/property/lv_line_properties.c
  lvgl/src/widgets/property/lv_menu_properties.c
  lvgl/src/widgets/property/lv_obj_properties.c
  lvgl/src/widgets/property/lv_roller_properties.c
  lvgl/src/widgets/property/lv_scale_properties.c
  lvgl/src/widgets/property/lv_slider_properties.c
  lvgl/src/widgets/property/lv_span_properties.c
  lvgl/src/widgets/property/lv_spinbox_properties.c
  lvgl/src/widgets/property/lv_spinner_properties.c
  lvgl/src/widgets/property/lv_style_properties.c
  lvgl/src/widgets/property/lv_switch_properties.c
  lvgl/src/widgets/property/lv_table_properties.c
  lvgl/src/widgets/property/lv_tabview_properties.c
  lvgl/src/widgets/property/lv_textarea_properties.c
)
pbl_library_sources_ifdef(CONFIG_LV_USE_QRCODE
  lvgl/src/widgets/qrcode/lv_qrcode.c
  lvgl/src/libs/qrcode/qrcodegen.c
)
pbl_library_sources_ifdef(CONFIG_LV_USE_ROLLER lvgl/src/widgets/roller/lv_roller.c)
pbl_library_sources_ifdef(CONFIG_LV_USE_SCALE lvgl/src/widgets/scale/lv_scale.c)
pbl_library_sources_ifdef(CONFIG_LV_USE_SLIDER lvgl/src/widgets/slider/lv_slider.c)
pbl_library_sources_ifdef(CONFIG_LV_USE_SPAN lvgl/src/widgets/span/lv_span.c)
pbl_library_sources_ifdef(CONFIG_LV_USE_SPINBOX lvgl/src/widgets/spinbox/lv_spinbox.c)
pbl_library_sources_ifdef(CONFIG_LV_USE_SPINNER lvgl/src/widgets/spinner/lv_spinner.c)
pbl_library_sources_ifdef(CONFIG_LV_USE_SWITCH lvgl/src/widgets/switch/lv_switch.c)
pbl_library_sources_ifdef(CONFIG_LV_USE_TABLE lvgl/src/widgets/table/lv_table.c)
pbl_library_sources_ifdef(CONFIG_LV_USE_TABVIEW lvgl/src/widgets/tabview/lv_tabview.c)
pbl_library_sources_ifdef(CONFIG_LV_USE_TEXTAREA lvgl/src/widgets/textarea/lv_textarea.c)
pbl_library_sources_ifdef(CONFIG_LV_USE_TILEVIEW lvgl/src/widgets/tileview/lv_tileview.c)
pbl_library_sources_ifdef(CONFIG_LV_USE_WIN lvgl/src/widgets/win/lv_win.c)

# Others
pbl_library_sources_ifdef(CONFIG_LV_USE_FILE_EXPLORER
  lvgl/src/others/file_explorer/lv_file_explorer.c
)
pbl_library_sources_ifdef(CONFIG_LV_USE_FRAGMENT
  lvgl/src/others/fragment/lv_fragment.c
  lvgl/src/others/fragment/lv_fragment_manager.c
)
pbl_library_sources_ifdef(CONFIG_LV_USE_TRANSLATION lvgl/src/others/translation/lv_translation.c)
pbl_library_sources_ifdef(CONFIG_LV_USE_MONKEY lvgl/src/debugging/monkey/lv_monkey.c)
pbl_library_sources_ifdef(CONFIG_LV_USE_PROFILER
  lvgl/src/debugging/profiler/lv_profiler_builtin.c
  lvgl/src/debugging/profiler/lv_profiler_builtin_posix.c
)
pbl_library_sources_ifdef(CONFIG_LV_USE_SYSMON lvgl/src/debugging/sysmon/lv_sysmon.c)
pbl_library_sources_ifdef(CONFIG_LV_USE_TEST
  lvgl/src/debugging/test/lv_test_display.c
  lvgl/src/debugging/test/lv_test_display_egl.c
  lvgl/src/debugging/test/lv_test_fs.c
  lvgl/src/debugging/test/lv_test_helpers.c
  lvgl/src/debugging/test/lv_test_indev.c
  lvgl/src/debugging/test/lv_test_indev_gesture.c
  lvgl/src/debugging/test/lv_test_screenshot_compare.c
)

# Demos
pbl_library_sources_ifdef(CONFIG_LV_BUILD_DEMOS lvgl/demos/lv_demos.c)
if(CONFIG_LV_USE_DEMO_BENCHMARK)
  pbl_library_sources_glob(RECURSE lvgl/demos/benchmark/*.c)
endif()
if(CONFIG_LV_USE_DEMO_KEYPAD_AND_ENCODER)
  pbl_library_sources_glob(RECURSE lvgl/demos/keypad_encoder/*.c)
endif()
if(CONFIG_LV_USE_DEMO_MUSIC)
  pbl_library_sources_glob(RECURSE lvgl/demos/music/*.c)
endif()
if(CONFIG_LV_USE_DEMO_RENDER)
  pbl_library_sources_glob(RECURSE lvgl/demos/render/*.c)
endif()
if(CONFIG_LV_USE_DEMO_STRESS)
  pbl_library_sources_glob(RECURSE lvgl/demos/stress/*.c)
endif()
if(CONFIG_LV_USE_DEMO_WIDGETS)
  pbl_library_sources_glob(RECURSE lvgl/demos/widgets/*.c)
endif()

pbl_library_named(lvgl_port)
pbl_library_sources(port/src/pbl_lvgl.c)
pbl_library_sources_ifdef(CONFIG_LVGL_DRAW_GPU port/src/pbl_lvgl_draw_gpu.c)
