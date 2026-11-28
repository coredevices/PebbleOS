# NimBLE: hands-free calls

This sample takes the calls of a phone, as the Hands-Free side of the
Hands-Free Profile over Bluetooth Classic, while it advertises and accepts
connections over Low Energy: it needs a dual-mode controller, and a microphone
and a speaker for the audio of the call.

A phone pairs with it over Low Energy. The pairing uses Secure Connections and
asks for the Classic link key to be derived from its keys, so the phone then
connects over Classic and starts the hands-free service without pairing
again. The sample is connectable over Classic, but phones do not find it
there: `hfp discoverable` lets them, to pair over Classic instead.

The calls are driven from the shell:

| Command | Does |
| --- | --- |
| `hfp answer` | Answers the incoming call |
| `hfp hangup` | Rejects the incoming call, or ends the one in course |
| `hfp dial <number>` | Places a call |
| `hfp volume <0-15>` | Tells the phone the speaker gain |
| `hfp audio on`, `hfp audio off` | Takes the audio of the call, or gives it back to the phone |
| `hfp connect`, `hfp disconnect` | Connects to the phone seen last, or disconnects from it |
| `hfp status` | Shows the connections and the counters of the audio |

The audio of a call is 16-bit samples at 8 kHz both ways, passed between the
synchronous connection and the audio streams through two small queues. The
speaker is kept below the level where it distorts, and the microphone gain
below the one where the speaker saturates it. There is no echo cancellation.

A few more commands check the audio without anyone talking: `hfp tone <Hz>`
sends a tone in place of the microphone, `hfp beep <Hz>` plays one on the
speaker, `hfp keep received` and `hfp keep mic` keep half a second of what the
phone sends or of what the microphone captures, which `hfp dump` prints, and
`hfp gain` sets the gain of the speaker or of the microphone.

Bonds and link keys are kept in RAM: they are lost when the device resets,
and a phone that kept its own has to forget the device and pair again.

```{pebbleos-sample}
:path: samples/third_party/nimble/hfp_hf
:sources: src/main.c prj.conf
:output:
```
