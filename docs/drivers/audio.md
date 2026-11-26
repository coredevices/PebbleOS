# Audio

An audio device has a stream per direction it supports, playback and capture,
each between a ring of periods in memory and the hardware. The class calls the
client back as each period completes, and the client refills or releases it.
Codecs, digital audio interfaces and amplifiers are devices of their own,
chained by the devicetree.

A device is a card describing one link, which is full duplex: the DAI and the
codec on it serve both directions, under the lock of the card, and each call
names the direction it acts on. A board whose speaker and microphone share a
link has its `speaker` and `mic` roles on the same card, as the nRF5 I2S
interface wired to a DA7212 codec, which can record while it plays.

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
| ST STM32 SAI | `st,stm32-sai-sub-a`, `st,stm32-sai-sub-b` | `CONFIG_AUDIO_STM32_SAI` |
| QEMU audio device | `qemu,pebble-audio` | `CONFIG_AUDIO_QEMU` |
| Simulated microphone | — | `CONFIG_AUDIO_SIM_MIC` |
| Nordic nRF5 PDM | `nordic,nrf52832-pdm` | `CONFIG_AUDIO_NRF5_PDM` |
| Nordic nRF5 I2S | `nordic,nrf52832-i2s` | `CONFIG_AUDIO_NRF5_I2S` |
| Digital microphones | `dmic-codec` | `CONFIG_AUDIO_DMIC` |
| Dialog DA7212 codec | `dlg,da7212` | `CONFIG_AUDIO_DA7212` |
| Cirrus Logic WM8994 codec | `wlf,wm8994` | `CONFIG_AUDIO_WM8994` |
