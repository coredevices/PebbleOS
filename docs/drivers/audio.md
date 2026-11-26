# Audio

An audio device streams one direction, playback or capture, between a ring of
periods in memory and the hardware. The class calls the client back as each
period completes, and the client refills or releases it. Codecs, digital
audio interfaces and amplifiers are devices of their own, chained by the
devicetree.

| | |
| --- | --- |
| Directory | `drivers/audio` |
| Header | `pbl/drivers/audio.h` |
| Kconfig | `CONFIG_AUDIO`, enabled by the application |
| Role | `MIC`, `SPEAKER` |
| Sample | {doc}`../samples/drivers/mic`, {doc}`../samples/drivers/speaker` |
| API | {apidoc}`API reference <drivers_audio>` |

## Drivers

| Driver | Compatible | Kconfig |
| --- | --- | --- |
| GPIO amplifier | `awinic,aw8155` | `CONFIG_AUDIO_AMP_GPIO` |
| GPIO amplifier | `simple-audio-amplifier` | `CONFIG_AUDIO_AMP_GPIO` |
| SiFli SF32LB52 audio codec | `sifli,sf32lb52-audcodec` | `CONFIG_AUDIO_SF32LB_AUDCODEC` |
| SiFli SF32LB57 audio codec | `sifli,sf32lb57-audcodec` | `CONFIG_AUDIO_SF32LB_AUDCODEC` |
| SiFli SF32LB52 PDM | `sifli,sf32lb52-pdm` | `CONFIG_AUDIO_SF32LB52_PDM` |
| SiFli SF32LB57 PDM | `sifli,sf32lb57-pdm` | `CONFIG_AUDIO_SF32LB52_PDM` |
| QEMU audio device | `qemu,pebble-audio` | `CONFIG_AUDIO_QEMU` |
| Simulated microphone | — | `CONFIG_AUDIO_SIM_MIC` |
| Nordic nRF5 PDM | `nordic,nrf52832-pdm` | `CONFIG_AUDIO_NRF5_PDM` |
| Nordic nRF5 I2S | `nordic,nrf52832-i2s` | `CONFIG_AUDIO_NRF5_I2S` |
| Digital microphones | `dmic-codec` | `CONFIG_AUDIO_DMIC` |
| Dialog DA7212 codec | `dlg,da7212` | `CONFIG_AUDIO_DA7212` |
