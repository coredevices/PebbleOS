# Samples

A sample is a small {doc}`application <../development/build_system/applications>`
that shows one part of PebbleOS in use: a kernel object, a subsystem, a
driver class. They live in `samples/`, and are meant to be read, run and
copied from.

```{toctree}
:maxdepth: 1
:caption: Kernel

kernel/threads.md
kernel/mutex.md
kernel/semaphore.md
kernel/msgq.md
kernel/poll.md
kernel/timer.md
kernel/work.md
kernel/delayed_work.md
kernel/heap.md
kernel/time.md
```

```{toctree}
:maxdepth: 1
:caption: Classic synchronization problems

kernel/classic/dining_philosophers.md
kernel/classic/bounded_buffer.md
kernel/classic/readers_writers.md
kernel/classic/sleeping_barber.md
kernel/classic/priority_inversion.md
kernel/classic/barrier.md
```

```{toctree}
:maxdepth: 1
:caption: Input

input/events.md
```

```{toctree}
:maxdepth: 1
:caption: Benchmarks

benchmarks/kernel.md
```

`samples/hello_world` is the smallest one, described in
{doc}`../development/build_system/applications`.

## Running one

A sample is configured, built and run as any application:

```shell
pbl configure --board qemu_emery samples/kernel/work
pbl build
pbl run
```

What it prints goes to the console ({doc}`../pbl/console`).

{doc}`../pbl/samples` does all of it at once, without a window, and checks
the output:

```shell
pbl samples samples/kernel/work
```

```text
PASS  kernel/work (3.6 s)
qemu_emery: 1 passed
```

## Samples are tests

Every sample states what it prints in its `sample.yml`, and CI runs them all
on the emulator, so a sample that stops building or printing what its page
shows fails a pull request.

```yaml
description: A heap over a buffer of its own, its usage and what fragmentation does to it
expect:
  - 'heap: empty: 0 used, \d+ free, largest free block \d+'
  - 'heap: 200 bytes from a fragmented heap: refused'
  - 'Sample done'
```

Single quotes keep YAML from reading the backslashes.

| Key | Meaning |
| --- | --- |
| `description` | One line on what the sample shows, as listed by `pbl samples --list` |
| `expect` | Regular expressions the console has to match, each on a later line than the one before; other lines may come in between, but not a second boot, so a sample that panics fails. Its page shows them as its output |
| `timeout` | Seconds the sample gets to print them (default: 10) |
| `boards` | The boards the sample is for, when it needs hardware not all have (default: every board) |
| `skip` | The boards the sample is not for, when few lack what it needs |

## Writing a sample

```text
samples/<area>/<name>/
  CMakeLists.txt
  sample.yml
  prj.conf           what it enables, at least the console and logging
  boards/<board>.conf
  src/main.c
```

- **One subject.** A sample shows one API, in the way it is meant to be
  used, in about a hundred lines. What needs two belongs in two samples.
- **It ends, and says so.** A sample logs what it does as it goes and
  finishes with `Sample done`, the last line of `expect`.
- **The same output every time.** `expect` only names lines whose order the
  code guarantees, by priorities or by waiting on each other, never by how
  long something happens to take: the emulator's clock can run late and then
  catch up, so two sleeps that end close to each other may end together.
- **Plain log formats.** The firmware hashes its logs on watches, which
  takes `%s`, `%d` and the like without widths or padding, so samples keep
  to those for their code to work there too.
- **Code to copy.** Errors are checked and returned as in the rest of the
  tree, and logged before `main()` returns one, so a failing run says why.
  What threads share is guarded, even a counter: threads of equal priority
  are time-sliced. Nothing is specific to a board: what differs between
  boards goes in `boards/<board>.conf`.
- **A page here.** `docs/samples/<area>/<name>.md` says what the sample
  shows, and the `pebbleos-sample` directive adds the rest from the sample
  itself, so the page cannot drift from it: how to run it, the boards it is
  for, the sources named in `:sources:` and, with `:output:`, its `expect`.

````markdown
# Mutexes

This sample has two threads add to a balance in two steps...

```{pebbleos-sample}
:path: samples/kernel/mutex
:sources: src/main.c
:output:
```
````
