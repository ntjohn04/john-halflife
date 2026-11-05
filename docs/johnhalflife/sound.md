# Sound

The Unified SDK replaces the engine's audio with its own OpenAL sound system (`src/game/client/sound/`).
It is cleaner than Steam Half-Life's software mixer and sounds noticeably different: purer, more spatial,
with reverb from long-retired EAX hardware presets. johnhalflife plays it the way the engine does.

## `jhl_snd_vanilla` (default 1)

One setting, in John Settings as "Sound: Half-Life's own mixing and room effects"; 0 gives the SDK's
OpenAL sound back. Each part below was checked against loopback recordings of vanilla.

| Part | What it does | Where |
|---|---|---|
| Mixing rate | Sounds are brought to the engine's 22050 Hz as they load, interpolating linearly, then converted to the device's rate as the system converts the engine's output: flat to 10.5 kHz, nothing above 11 kHz (a 64-tap windowed sinc; OpenAL's own resamplers roll off from 6 kHz, which muffled 22 kHz sounds such as the guns). OpenAL then plays them as they are. A scientist's 29 s speech stalled the frame it first played in for some 60 ms, and as long again at a new pitch: sounds a second or longer are now converted on several threads (up to 8), a mono sound's taps 4 at a time (SSE; within a millionth), and a map's sound files of 64 KB or more (about 3 s) are loaded and converted while it loads (`PreloadIfLong`). | `SoundCache::LoadSound`, `ToDeviceRate` |
| Pitch | Off a sound's own pitch (nearly everything: voices 100-103 a line, guns 98-102), it is pitched as the engine pitches it: stepped through at its own rate without interpolating, then brought to 22050 Hz. The uneven steps are the faint grain of vanilla's sound. Made once per sound and pitch; dialogue words too. | `SoundCache::GetPitched`, `SentencesSystem::SetSentenceWord` |
| Ear volumes | The engine's spatialization: each ear `(1 - distance × attenuation / 1000) × (1 ∓ side)`, the player's own sounds and sounds without attenuation full in both ears. Played through OpenAL's stereo panning inverted (measured on a loopback device), so each ear gets exactly that. | `ApplyVanillaSpatialization`, `StereoPanGains` |
| Output | Plain stereo, no limiter (loud overlaps clip, as the engine's), no HRTF. | `BuildDeviceAttributes` |
| Room types | The engine's software DSP presets (as in Xash3D's `s_dsp.c`) in place of the EAX ones: a reverb and an echo per room, on two effect sends, each room's level set to vanilla's as measured (every room within about 2 dB), feedback rounded as the engine stores it. The metal and tunnel rooms' (2-7) echoes 3-4 dB lower, matched band by band: OpenAL's echo damping can't take the highs off the way the engine's 22 kHz loop filter does, and the tunnels rang bright at 0.5-3 kHz. The water rooms (14-16; 14 under water) muffle the whole mix. | `HalfLifeSoundEffects.cpp` |

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

## Music in saves

`jhl_snd_musicsaves` (default 1; 0: loading a save stops the music, as vanilla).

Saves keep the music: loading one plays the track that was playing when it was saved, from that point, paused if
the map had paused it and still fading out if it was fading. In vanilla, loading a save stopped the music. The
Unified SDK already keeps music playing across level changes. Single player only (saves are).

* **Saving:** the client is in the server's process in single player, so the server asks the client's music system
  directly as it saves the player (`DispatchSave` → `JhlMusic_BeforeSave`, the client's exported `JhlMusic_GetState`,
  `jhl_musicstate.h`). What's saved is exact at that moment: the position is where the source is in the track
  (`MusicSystem::UpdatePosition`: OpenAL's offset in its queue, the buffers queued mirrored with where each starts in
  the track), and the game pausing pauses the music, so a save from the menu holds the second it was paused at.
* **Loading:** it's in the player's data (`CBasePlayer::m_JhlMusic*`, added to its data map: older saves load, with
  no music). The `JhlMusic` message sends it each time the player's HUD starts. Loading a save resets the
  connection, which stops the music (`ClientLibrary::RunFrame`); only then does the client play what was sent
  (`jhl_music.cpp`, `MusicSystem::PlayFrom`). A level change doesn't reset it, and the music plays on as it was.
* A track the map paused (`music pause`) stays paused when the game unpauses: the game's pause and the map's are
  kept apart (`MusicSystem::m_PausedByCommand`). Before, unpausing the game resumed it.
* The server finds the client library by name (`client.dll`, or `cl_dlls/client.so` on Linux, only if already
  loaded). If it can't, the player's music is saved as it was last.
