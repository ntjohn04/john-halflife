# Sound

The Unified SDK replaces the engine's audio with its own OpenAL sound system (`src/game/client/sound/`).
It is cleaner than Steam Half-Life's software mixer and sounds noticeably different: purer, more spatial,
with reverb from long-retired EAX hardware presets. johnhalflife plays it the way the engine does.

## `jhl_snd_vanilla` (default 1)

One setting, in John Settings as "Sound: Half-Life's own mixing and room effects"; 0 gives the SDK's
OpenAL sound back. Each part below was checked against loopback recordings of vanilla.

| Part | What it does | Where |
|---|---|---|
| Mixing rate | Sounds are brought to the engine's 22050 Hz as they load, interpolating linearly, then OpenAL takes them to the device's rate with its best sinc, as the system takes the engine's output. Nothing above 11 kHz, as vanilla. | `SoundCache::LoadSound` |
| Pitch | Off a sound's own pitch (nearly everything: voices 100-103 a line, guns 98-102), it is pitched as the engine pitches it: stepped through at its own rate without interpolating, then brought to 22050 Hz. The uneven steps are the faint grain of vanilla's sound. Made once per sound and pitch; dialogue words too. | `SoundCache::GetPitched`, `SentencesSystem::SetSentenceWord` |
| Ear volumes | The engine's spatialization: each ear `(1 - distance × attenuation / 1000) × (1 ∓ side)`, the player's own sounds and sounds without attenuation full in both ears. Played through OpenAL's stereo panning inverted (measured on a loopback device), so each ear gets exactly that. | `ApplyVanillaSpatialization`, `StereoPanGains` |
| Output | Plain stereo, no limiter (loud overlaps clip, as the engine's), no HRTF. | `BuildDeviceAttributes` |
| Room types | The engine's software DSP presets (as in Xash3D's `s_dsp.c`) in place of the EAX ones: a reverb and an echo per room, on two effect sends, each room's level set to vanilla's as measured (every room within about 2 dB), feedback rounded as the engine stores it. The water rooms (14-16; 14 under water) muffle the whole mix. | `HalfLifeSoundEffects.cpp` |

Switching it live resets the audio device; sounds already loaded keep how they were loaded until the next
map load.

**Not reproduced** (OpenAL's limits):
* The reverb's texture: OpenAL's reverb is smoother than the engine's two delay lines, at the same level.
* Echoes longer than 0.207 s (outdoors and caverns, 0.3-0.38 s in vanilla) repeat at 0.207 s, dying away
  as fast.
* The engine's left channel delay (2-40 ms in many rooms): an effect send can't delay the dry sound.

## Sound sources

Every sound playing holds an OpenAL source. OpenAL Soft allows 256 unless asked, and the SDK neither asked
nor checked, so past that new sounds (gunfire, explosions) played silently until enough ended. Not gated:
it only acts where a sound would otherwise be silent.

* 1024 mono and 64 stereo sources are asked for.
* With none left, a one-shot sound out of earshot, else the one closest to ending, is taken over, as the
  engine did with its channels full. Loops and sentences are never taken.
* When that happens, a console warning (at most every 5 s) lists the sounds playing most.

`cl_snd_channels` prints every sound playing, the device's real settings (rate, output mode, limiter,
HRTF, resampler) and, for the loudest sounds, each ear's volume and what OpenAL was given.
