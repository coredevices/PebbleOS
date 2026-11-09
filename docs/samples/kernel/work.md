# Work queues

This sample defers work to a {doc}`work queue <../../kernel/work>`: first to
the system's, then to one it creates. The work item is part of the object it
acts on, a pretend sensor, and its function handles every event since its last
run.

Three submits made one run: an item is queued at most once, which is why the
function drains a count instead of assuming one run per event. The canceled
item never ran, so its event stays pending for the next submit.

```{pebbleos-sample}
:path: samples/kernel/work
:sources: src/main.c
:output:
```
