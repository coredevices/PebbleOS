# Local microphone capture

SDK revision 111 adds `mic_capture_start`, `mic_capture_stop` and
`mic_capture_is_active`. A watchapp receives signed 16-bit mono PCM at
16 kHz on its app task, in 320-sample (20 ms) callbacks. No audio is sent
to the phone by this API. The sample pointer is valid only for the callback.
A slow app drops frames once the two-frame kernel queue fills.

```c
static void on_audio(const int16_t *samples, size_t count, void *context) {
  // Consume or copy samples here.
}

static void on_stopped(MicCaptureStopReason reason, void *context) {
  // Capture has ended; a new start is required.
}

MicCaptureStartResult result = mic_capture_start((MicCaptureHandlers) {
  .data = on_audio,
  .stopped = on_stopped,
}, NULL);
// Later: mic_capture_stop();
```

Declare `manifestVersion: 2` and `usesPermissions: ["WatchMicrophone"]` in
the `pebble` block of `package.json`. The updated phone app syncs only
explicitly allowed Microphone decisions for installed apps while its
experimental permission system is enabled. Permission granted automatically
by a policy does not authorize local capture: enable the permission on the
app's page. Before the first sync, start returns
`MicCaptureStartErrPermissionDenied` and the hardware remains stopped.

The grant is stored on the watch, so capture works after disconnecting the
phone or rebooting the watch. An offline watch cannot receive a new grant or
revocation until it reconnects. A connected revocation discards queued samples,
stops capture and reports `MicCaptureStopReasonPermissionRevoked`.
Disabling the permission system on the phone resyncs app metadata with microphone access denied.

Only foreground watchapps may capture. Watchfaces and workers are rejected.
The OS displays its Listening banner; focus loss, app exit and dictation
preemption stop capture. Permission updates that retain the current app's
grant do not interrupt it. Local capture and phone streaming share the
microphone manager, so only one may run at a time: either start returns busy
while the other runs, including while a stream waits for the phone.

## Synchronization

Firmware advertises capability bit 27, `app_permissions_support`. CoreApp
keeps the watch microphone grant in its existing locker entry and sends it
through the normal AppDB (`0x02`) sync, with the same app selection, sync limits
and deletion handling. No separate permission database or UUID snapshot exists.

The optional wire extension is a little-endian `uint32_t` appended after the
96-byte app name: bit 0 authorizes the microphone. Only extended metadata
(130 bytes) sets or clears the grant; legacy metadata (126 bytes), as an older
companion sends, leaves the stored grant unchanged. Apps without a stored grant
read as denied. CoreApp sends the extension only to capable firmware. The watch
stores the grant in its own AppDB record keyed by the app UUID, keeping the app
record at the legacy size that older firmware expects after a downgrade.
Permission-only updates, extended metadata whose first 126 bytes match the
stored record, preserve the app install ID and cached binary, so toggling the
permission does not close or reinstall the app. Any other insert, including a
resend of identical metadata, is handled as a reinstall. Metadata deletion
revokes the grant.

Streaming and local capture share the same explicit Microphone decision.
Automatic approval policies do not grant either API access. Decisions remain
in the phone's existing permission store; the locker field is its watch-sync
projection, updated transactionally with the app's sync hash.

### Trust

The AppDB permissions field is the only consent point for local capture: any
extended insert with bit 0 set grants capture to that UUID, with no prompt on
the watch beyond the Listening banner while the microphone runs. The watch
keeps it out of apps' reach (BlobDB is private to the system session,
PebbleKit sessions cannot reach it, and no app syscall writes the AppDB), but it
cannot check what the companion does. A companion must only ever set the field
from its own consent UI, never from an API an app can reach, including the
app's own PebbleKit JS.

The grant belongs to the app UUID, which the developer chooses, and survives
reinstalls and upgrades. The companion is responsible for asking again when a
different binary arrives under a granted UUID, e.g. a sideloaded app reusing it.

The AppDB extension is phone-controlled and not writable by an app syscall.
Capture syscalls check task, app UUID and session token, validate app buffers,
and copy PCM into the app's memory. Events contain session tokens, never
pointers into kernel memory. A stale event cannot dispatch into a later
capture session.

The firmware and SDK must ship together: an older firmware does not contain
the revision 111 function table entries. The existing
`mic_stream_to_phone_*` API remains unchanged. Frozen SDK platforms expose
unavailable stubs: start returns `MicCaptureStartErrUnavailable`, stop is a
no-op and is-active returns false.

## In the emulator

The QEMU boards capture what the host sends over QEMU protocol 14
(`QemuProtocol_MicAudio`, 16 kHz mono signed 16-bit little-endian PCM),
and silence when it sends nothing. The firmware queues up to 250 ms of it
while an app captures or streams, and drops it otherwise.
`pebble emu-mic` sends a tone, white noise or a WAV file, paced to real time.

The SDK emulator's phone, pypkjs, stands in for CoreApp. It grants the
microphone to every app that declares `WatchMicrophone` with manifestVersion 2,
as if the user had allowed it, and refuses it to the rest with a log line
saying why. It sends the grant in the app's AppDB entry at install and
answers streams to the running app's PebbleKit JS with the same
`audiostreamstart`, `audiostream` and `audiostreamstop` events.
`pebble emu-permission WatchMicrophone --deny` refuses it as a user would,
including mid-capture or mid-stream, and `--reset` undoes that. A watch
emulator without pypkjs, such as `pbl qemu` with `pebble install --qemu`,
gets no grant and has no phone to stream to.
