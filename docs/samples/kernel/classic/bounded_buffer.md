# Bounded buffer

This sample builds a queue of four items by hand, with a semaphore counting
the free slots, another counting the items and a mutex for the indices, and
has two producers and two slower consumers go through a hundred items.

This is what {doc}`a message queue <../msgq>` is inside, and the queue is what
to use. Note the order: wait for the slot first, lock second. The other way
round, a producer waiting with the lock held stops every consumer.

```{pebbleos-sample}
:path: samples/kernel/classic/bounded_buffer
:sources: src/main.c
:output:
```
