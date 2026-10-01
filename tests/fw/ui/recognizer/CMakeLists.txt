# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_recognizer
  SOURCES
    fw/applib/ui/recognizer/recognizer.c
    tests/fw/ui/recognizer/test_recognizer_impl.c
)

pbl_clar_test(test_recognizer_manager
  SOURCES
    fw/applib/graphics/gtypes.c
    fw/applib/ui/layer.c
    fw/applib/ui/recognizer/recognizer.c
    fw/applib/ui/recognizer/recognizer_manager.c
    tests/fw/ui/recognizer/test_recognizer_impl.c
  DEFINES CONFIG_TOUCH
)

pbl_clar_test(test_tap
  SOURCES
    fw/applib/ui/recognizer/recognizer.c
    fw/applib/ui/recognizer/tap.c
    tests/fw/ui/recognizer/test_recognizer_impl.c
    tests/fakes/fake_rtc.c
  DEFINES CONFIG_TOUCH
)

pbl_clar_test(test_pan
  SOURCES
    fw/applib/ui/recognizer/recognizer.c
    fw/applib/ui/recognizer/pan.c
    tests/fw/ui/recognizer/test_recognizer_impl.c
    tests/fakes/fake_rtc.c
  DEFINES CONFIG_TOUCH
)

pbl_clar_test(test_swipe
  SOURCES
    fw/applib/ui/recognizer/recognizer.c
    fw/applib/ui/recognizer/swipe.c
    tests/fw/ui/recognizer/test_recognizer_impl.c
    tests/fakes/fake_rtc.c
  DEFINES CONFIG_TOUCH
)

pbl_clar_test(test_recognizer_competition
  SOURCES
    fw/applib/graphics/gtypes.c
    fw/applib/ui/layer.c
    fw/applib/ui/recognizer/recognizer.c
    fw/applib/ui/recognizer/recognizer_manager.c
    fw/applib/ui/recognizer/pan.c
    fw/applib/ui/recognizer/swipe.c
    fw/applib/ui/recognizer/tap.c
    tests/fw/ui/recognizer/test_recognizer_impl.c
    tests/fakes/fake_rtc.c
  DEFINES CONFIG_TOUCH
)

pbl_clar_test(test_recognizer_lifecycle
  SOURCES
    fw/applib/graphics/gtypes.c
    fw/applib/graphics/gcolor_definitions.c
    fw/applib/ui/layer.c
    fw/applib/ui/window.c
    fw/applib/ui/window_manager.c
    fw/applib/ui/recognizer/recognizer.c
    fw/applib/ui/recognizer/recognizer_manager.c
    tests/fw/ui/recognizer/test_recognizer_impl.c
  DEFINES CONFIG_TOUCH
)

pbl_clar_test(test_touch_nav
  SOURCES
    fw/applib/graphics/gtypes.c
    fw/applib/ui/layer.c
    fw/applib/ui/picker_touch.c
    fw/applib/ui/recognizer/recognizer.c
    fw/applib/ui/recognizer/recognizer_manager.c
    fw/applib/ui/recognizer/pan.c
    fw/applib/ui/recognizer/swipe.c
    fw/applib/ui/recognizer/tap.c
    fw/applib/ui/recognizer/touch_nav.c
    tests/fakes/fake_rtc.c
  DEFINES CONFIG_TOUCH
)
