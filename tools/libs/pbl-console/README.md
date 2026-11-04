# pbl-console

Host side of the PebbleOS text console, described in `docs/subsystems/console.md`:

- `pblconsole.link`: `ConsoleLink` opens a port or `socket://` URL and separates log lines, shell
  output and frames.
- `pblconsole.protocol`: the Pebble protocol in frames, as a libpebble2 transport.
- `pblconsole.imaging`: writes firmware and resources to flash.

Run its tests with `pytest tools/libs/pbl-console/tests`; they need no watch.
