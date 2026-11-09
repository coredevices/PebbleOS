# Sleeping barber

This sample has a barber who sleeps on a semaphore until a customer arrives,
and a waiting room of three chairs. Five customers arrive at once: three sit
down, two find no chair and leave.

It is the shape of a server with a bounded backlog: looking for a free chair
and taking it happen under one lock, and a caller that finds none is turned
away instead of waiting. Which two leave depends on the order the kernel runs
the customers in, which have the same priority.

```{pebbleos-sample}
:path: samples/kernel/classic/sleeping_barber
:sources: src/main.c
:output:
```
