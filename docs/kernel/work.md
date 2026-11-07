# Work queues

A work queue is a kernel thread that runs functions on behalf of interrupts and
other threads. It is how the system defers anything that cannot run where it was
triggered: an ISR handing off to thread context, a timeout that needs to take a
lock, or a slow job that should not hold up its caller.

## Work items

A work item is a `struct pbl_work`, owned by the caller and usually embedded in
the object it acts on, which its function recovers with `container_of`:

```c
struct session {
  struct pbl_work tx;
  ...
};

static void prv_tx(struct pbl_work *work) {
  struct session *session = container_of(work, struct session, tx);
  ...
}

pbl_work_init(&session->tx, prv_tx);
pbl_work_submit(&pbl_sys_workq, &session->tx);
```

Because the queue links the items themselves, submitting allocates nothing and
cannot fail, from any context. The price is that an item is queued at most once:
submitting a pending item does nothing and returns false, so its function runs
once for any number of submits. Functions should therefore drain whatever state
triggered them, not assume one run per event.

Items run one at a time, in submission order. An item is off the queue by the
time its function runs, so the function may submit it again or free it.

## Delayed work

A `struct pbl_delayed_work` adds a {doc}`kernel timer <time>` in front of the
queue. The timer expires in the tick interrupt and submits the item, so the
function runs in thread context and may block.

- `pbl_delayed_work_schedule()` keeps a delay that is already running.
- `pbl_delayed_work_reschedule()` replaces it, for timeouts pushed back on each
  event.

There is no periodic mode. A function that needs to repeat schedules itself,
with `pbl_delayed_work_schedule_next()` to count from its last expiry so that it
does not drift. Periods missed by a slow function are skipped, not run in a burst.

## Calls with a payload

`pbl_work_call()` runs `fn(data)` on a queue for work where several payloads may
be pending at once, which a single work item cannot express. Its slots come from
a `struct pbl_work_pool`, so it can fail: without waiting it returns `-EBUSY`,
and it returns `-EAGAIN` when the timeout expires. What to do about that is the
caller's decision. Each set of callers that must not starve the others gets its
own pool.

The queue's own thread never waits for a slot, because only it can release them.

## Waiting for work

| Call | Effect |
| --- | --- |
| `pbl_work_cancel()` | Removes a pending item; a running function carries on. |
| `pbl_work_cancel_sync()` | Also waits for a running function to return. |
| `pbl_work_flush()` | Waits for an item to finish, without canceling it. |
| `pbl_workq_flush()` | Waits for everything submitted so far. |

`pbl_work_cancel_sync()` and `pbl_delayed_work_cancel_sync()` make it safe to
free the object an item is embedded in, including when the function re-arms
itself. A function canceling itself does not wait. Flushing from the queue's own
thread would never return, and asserts.

## System queues

| Queue | Thread | Use |
| --- | --- | --- |
| `pbl_sys_workq` | `KernelBG`, low priority | Background work; may block and take a while. |
| `pbl_sys_workq_high` | `NewTimer`, highest priority | Short, time sensitive work and timeouts. |

Work on `pbl_sys_workq_high` delays every lower priority thread and every other
item on the queue, so it must be brief.

A queue's thread is created by whoever owns its stack, with `pbl_workq_thread` as
the entry and the queue as the argument. Work submitted before the thread starts
waits for it. The hooks in `struct pbl_workq_ops` let the owner attach policy,
such as a watchdog, around the work it runs.

## Choosing a mechanism

| Need | Use |
| --- | --- |
| Run something brief in the tick interrupt | `struct pbl_timer` |
| Defer from an interrupt to a thread | `struct pbl_work` |
| Run something later, in a thread | `struct pbl_delayed_work` |
| Several pending payloads | `pbl_work_call()` |
| Own priority, or long blocking work | A dedicated `struct pbl_workq` |
