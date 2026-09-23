# App permissions and the Microphone API

Third-party apps can ask for capabilities the user has to grant. The first
one is live microphone access. This page describes the moving parts across
the firmware and the phone app so the two stay in sync.

Nothing that predates the permission system is gated by it: dictation
sessions keep working for every app, because the app only ever receives a
transcription. Permissions only guard APIs that hand raw data to the app.

## Declaring a permission

An app declares what it needs in its manifest:

```json
"capabilities": ["microphone"]
```

The SDK build turns that into `PROCESS_INFO_USES_MICROPHONE` (bit 11 of the
binary header flags, see `pebble_process_info.h`), so the firmware can tell
"not declared" from "denied" without talking to the phone. The phone reads
the same array from `appinfo.json` in the `.pbw` and from the web locker.

## Grant records

The phone is the source of truth. It pushes one record per app into the
`AppPermissions` BlobDB (id `0x0D`), keyed by the 16-byte app UUID:

| Offset | Size | Field           | Notes                              |
|-------:|-----:|-----------------|------------------------------------|
| 0      | 1    | `version`       | 1                                  |
| 1      | 3    | reserved        | 0                                  |
| 4      | 4    | `granted_mask`  | little-endian, bit 0 = microphone  |
| 8      | 4    | `declared_mask` | informational, same bit layout     |

The record is deleted when the app is uninstalled and kept across upgrades,
so grants persist across ordinary updates. The watch advertises
`app_permissions_support` (protocol capability bit 25) so the phone knows it
can push records; enforcement never depends on that bit.

## Resolving a permission on the watch

`app_permissions_get_state_for_current_app()` in
`src/fw/services/app_permissions/` combines the header flag and the record:

- not declared in the header → `AppPermissionStateNotDeclared`, whatever the
  record says;
- system apps → `AppPermissionStateGranted`;
- declared, no record (old phone app, or nothing pushed yet) →
  `AppPermissionStateDenied`. SDK shell builds (`CONFIG_SHELL_SDK`) return
  `Granted` here so `pbl install` works without the phone;
- declared with a record → the bit in `granted_mask`.

When the running app's record changes the service emits
`PEBBLE_APP_PERMISSION_EVENT`, which apps observe through
`app_permission_service_subscribe()`, and tells the capture service to stop
if the microphone was revoked.

Console commands for development:

```
perm list
perm grant <install id> mic
perm revoke <install id> mic
```

## Microphone capture

Watchfaces can never record: the SDK build rejects `microphone` in a
watchface manifest, the phone never creates a grant for one, and the capture
service refuses them (`MicCaptureStartErrWatchface`) regardless of the record.

`mic_data_service_subscribe()` (applib) starts capture through
`mic_capture_service` (kernel). Capture is only served to the app task, only
while the app is in focus, and only with the permission granted. The kernel
owns a 320 ms ring buffer that the app drains through syscalls; when the
app falls behind the newest chunk is dropped and the next batch carries an
`overrun` flag.

The microphone itself is shared through `mic_manager`: dictation always
wins and preempts an app, and an app cannot start while dictation runs.

Capture stops, with a reason delivered to the app's `stopped` handler:

| Trigger                                   | `MicDataStopReason`  |
|-------------------------------------------|----------------------|
| any focusable modal (notification, call…) | `FocusLost`          |
| dictation takes the microphone            | `Interrupted`        |
| the grant is revoked                      | `PermissionRevoked`  |
| the app exits or is killed                | (no callback)        |

While capturing, the OS shows a "Listening" banner (`src/fw/popups/mic_banner.c`)
as a transparent, unfocusable modal at discreet priority. It reserves its strip
through the unobstructed area service, composing with Timeline Peek, so
`layer_get_unobstructed_bounds()` shrinks for the app. Any modal that could
hide the banner also takes focus, which is what stops capture.

## Streaming to the phone

`mic_stream_to_phone_start()` (applib) skips the app entirely: the capture
service opens the Speex encoder, sets up an audio endpoint transfer and asks
the phone for a `VoiceEndpointSessionTypeAudioStream` session over the voice
endpoint, tagged with the app UUID (untagged for built-in apps). Once the
phone accepts, the mic starts and every frame is encoded on KernelBG and sent
over the audio endpoint (10000), exactly like dictation but with no result
expected. The app is told through `started`, and through `stopped` with
`MicDataStopReasonPhone` if the phone refuses, stops, or never answers (8 s).
Dictation preempts a stream the same way it preempts capture.

On the phone, `VoiceSessionManager` answers the session, decodes the frames
and publishes them as PCM to the app's PebbleKit JS runtime as
`audiostream` events (see `WatchAudioStreams`).

## Encoding

Raw PCM is too much for the Bluetooth link. `audio_encoder_open()` (applib)
gives the app the firmware's speech encoder, one frame at a time; the
returned `AudioEncoderInfo` tells the phone how to decode. Speex wideband is
the only codec today; the service is codec-agnostic so another backend can
be added later. Dictation uses the same service as the system owner and
takes precedence over an app.

## Phone side (CoreApp)

- `LockerAppPermission` rows track each declared permission per app, with
  `decided = false` until the user answers the prompt shown on the home
  screen. Grants can be changed later from the app's detail screen.
- `LockerPermissions` reconciles the rows whenever an app is installed,
  updated or removed, and mirrors them into the `AppPermissionsEntry`
  BlobDB entity that syncs to the watch.
- An update that adds a permission installs with the permission denied and
  prompts right away; the install itself is never blocked.
