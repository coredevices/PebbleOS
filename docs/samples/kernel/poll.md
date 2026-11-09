# Poll groups

This sample is an event loop: one thread that waits on a queue of button
events and a queue of accelerometer samples at once, each with its own message
type, and handles whichever has something.

The wait returns the queue and leaves the message in it, so the loop receives
it with the type that queue carries.

```{pebbleos-sample}
:path: samples/kernel/poll
:sources: src/main.c
:output:
```
