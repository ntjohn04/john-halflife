# Sound

The Unified SDK replaces the engine's audio with OpenAL (`client/sound/`). It sounds cleaner and different from
vanilla. John Half-Life plays it the way the engine did.

## Vanilla sound (`jhl_snd_vanilla`)

0 gives the SDK's OpenAL sound back.

* **22 kHz:** sounds converted to the engine's 22050 Hz on load, then to the device's rate (flat to 10.5 kHz).
* **Pitch:** pitched the engine's way, without interpolating: vanilla's faint grain.
* **Ear volumes:** the engine's formula, `(1 - distance × attenuation / 1000) × (1 ∓ side)`.
* **Output:** plain stereo, no limiter, no HRTF.
* **Rooms:** the engine's software DSP rooms, played exactly through OpenAL's convolution reverb
  (`HalfLifeSoundEffects.cpp`). Water rooms muffle the mix.
* **No hitches:** long sounds converted on several threads; big ones loaded with the map; a sound not ready yet
  starts a frame or two later instead of stalling.
* Not reproduced: the left channel delay.

`jhl_perf_preloadsounds` (default 0) loads every precached sound with the map.

## Sound sources

* OpenAL allowed 256 sounds; past that, new sounds went silent.
* Now 1024 mono and 64 stereo. With none left, a one-shot out of earshot (or the one nearest its end) is taken over.
* `cl_snd_channels` prints every sound playing and the device's settings.

## Music in saves (`jhl_snd_musicsaves`)

* Loading a save plays the track that was playing, from that point, paused or fading as it was.
* Vanilla stopped the music on load.
* Saved in the player's data (`CBasePlayer::m_JhlMusic*`); sent by the `JhlMusic` message.
* A track the map paused stays paused when the game unpauses.
