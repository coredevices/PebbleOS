# Message queues

This sample passes samples from a producer thread to `main()` through a queue
of two messages. The producer blocks when the queue is full, so the consumer
sets the pace. It then shows what a full queue answers to a caller that does
not wait, and how an urgent message gets ahead of the others.

The consumer stops on the first `pbl_msgq_get()` that times out. Messages are
copied, so the producer builds each one on its stack.
{doc}`classic/bounded_buffer` builds the same thing from semaphores.

```{pebbleos-sample}
:path: samples/kernel/msgq
:sources: src/main.c
:output:
```
