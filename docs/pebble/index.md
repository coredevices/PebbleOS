# Pebble Firmware

The Pebble watch firmware, under `pebble/`, is the largest application built
on PebbleOS. It is built like any other application (see
{doc}`../development/build_system/applications`), and adds on top of
PebbleOS:

{doc}`Services <../services/index>`
: The watch's features: they own shared resources, persist state, talk to
  the phone and feed events to apps.

Application framework
: `pebble/applib` is the API watch apps are written against, from windows
  and layers to graphics and text. Its exported part is the
  {doc}`Application SDK <../sdk/index>`.

Watch apps
: System apps (launcher, settings, notifications, health, …) are built
  into the firmware from `pebble/apps`; third-party watchfaces, apps and
  background workers are installed from the phone. They run unprivileged;
  {doc}`../sdk/how_it_works` describes the boundary.

Its main thread runs the event loop: services and drivers post events to
it, and it forwards what concerns the app to the app's own thread. Only one
app and one worker run at a time.

It builds in two variants: **normal**, the full firmware, and **recovery
(PRF)**, a minimal one kept in its own slot, from which the phone can
reinstall the normal one. A **platform** is what watch apps see: display
size, shape and colors, and capabilities; boards of the same watch model
share one.

The {doc}`../playground` runs it in the browser.
