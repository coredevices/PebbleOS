# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_recognizer
  SOURCES
    pebble/applib/ui/recognizer/recognizer.c
    tests/pebble/ui/recognizer/test_recognizer_impl.c
)

pbl_clar_test(test_recognizer_manager
  SOURCES
    pebble/applib/graphics/gtypes.c
    pebble/applib/ui/layer.c
    pebble/applib/ui/recognizer/recognizer.c
    pebble/applib/ui/recognizer/recognizer_manager.c
    tests/pebble/ui/recognizer/test_recognizer_impl.c
  DEFINES CONFIG_TOUCH
)

pbl_clar_test(test_tap
  SOURCES
    pebble/applib/ui/recognizer/recognizer.c
    pebble/applib/ui/recognizer/tap.c
    tests/pebble/ui/recognizer/test_recognizer_impl.c
    tests/fakes/fake_rtc.c
  DEFINES CONFIG_TOUCH
)

pbl_clar_test(test_pan
  SOURCES
    pebble/applib/ui/recognizer/recognizer.c
    pebble/applib/ui/recognizer/pan.c
    tests/pebble/ui/recognizer/test_recognizer_impl.c
    tests/fakes/fake_rtc.c
  DEFINES CONFIG_TOUCH
)

pbl_clar_test(test_swipe
  SOURCES
    pebble/applib/ui/recognizer/recognizer.c
    pebble/applib/ui/recognizer/swipe.c
    tests/pebble/ui/recognizer/test_recognizer_impl.c
    tests/fakes/fake_rtc.c
  DEFINES CONFIG_TOUCH
)

pbl_clar_test(test_recognizer_competition
  SOURCES
    pebble/applib/graphics/gtypes.c
    pebble/applib/ui/layer.c
    pebble/applib/ui/recognizer/recognizer.c
    pebble/applib/ui/recognizer/recognizer_manager.c
    pebble/applib/ui/recognizer/pan.c
    pebble/applib/ui/recognizer/swipe.c
    pebble/applib/ui/recognizer/tap.c
    tests/pebble/ui/recognizer/test_recognizer_impl.c
    tests/fakes/fake_rtc.c
  DEFINES CONFIG_TOUCH
)

pbl_clar_test(test_recognizer_lifecycle
  MODULES gtypes
  SOURCES
    pebble/applib/ui/layer.c
    pebble/applib/ui/window.c
    pebble/applib/ui/window_manager.c
    pebble/applib/ui/recognizer/recognizer.c
    pebble/applib/ui/recognizer/recognizer_manager.c
    tests/pebble/ui/recognizer/test_recognizer_impl.c
  DEFINES CONFIG_TOUCH
)

pbl_clar_test(test_touch_nav
  SOURCES
    pebble/applib/graphics/gtypes.c
    pebble/applib/ui/layer.c
    pebble/applib/ui/recognizer/recognizer.c
    pebble/applib/ui/recognizer/recognizer_manager.c
    pebble/applib/ui/recognizer/pan.c
    pebble/applib/ui/recognizer/swipe.c
    pebble/applib/ui/recognizer/tap.c
    pebble/applib/ui/recognizer/touch_nav.c
    tests/fakes/fake_rtc.c
  DEFINES CONFIG_TOUCH
)
