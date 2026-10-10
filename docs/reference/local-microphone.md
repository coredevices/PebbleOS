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

Declare `manifestVersion: 2` and `usesPermissions: ["Microphone"]` in
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
microphone manager, so only one may run at a time.

## Synchronization

Firmware advertises capability bit 27, `local_microphone_support`. CoreApp
keeps the watch microphone grant in its existing locker entry and sends it
through the normal AppDB (`0x02`) sync, with the same app selection, sync limits
and deletion handling. No separate permission database or UUID snapshot exists.

The optional wire extension is a little-endian `uint32_t` appended after the
96-byte app name: bit 0 authorizes the microphone. Legacy metadata is 126 bytes
and carries no grant; extended metadata is 130 bytes. Older stored records
read as denied. CoreApp sends the extension only to capable firmware. Permission-only
updates preserve the app install ID and cached binary, so toggling the permission
does not close or reinstall the app. Metadata deletion revokes the grant.

Streaming and local capture share the same explicit Microphone decision.
Automatic approval policies do not grant either API access. Decisions remain
in the phone's existing permission store; the locker field is its watch-sync
projection, updated transactionally with the app's sync hash.

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
