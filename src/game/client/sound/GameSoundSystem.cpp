/***
 *
 *	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
 *
 *	This product contains software technology licensed from Id
 *	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
 *	All Rights Reserved.
 *
 *   Use, distribution, and modification of this source code and/or resulting
 *   object code is restricted to non-commercial enhancements to products from
 *   Valve LLC.  All other use, distribution, or modification is prohibited
 *   without written permission from Valve LLC.
 *
 ****/

#include <exception>
#include <numbers>
#include <type_traits>

#include <fmt/format.h>

#define AL_ALEXT_PROTOTYPES
#include <AL/alext.h>

#include "cbase.h"
#include "com_model.h"
#include "event_api.h"
#include "ISoundSystem.h"
#include "GameSoundSystem.h"
#include "view.h"

namespace sound
{
// johnhalflife: positions kept as plain floats in Channel (SoundDefs.h has no Vector).
static void StoreVector(const Vector& from, float to[3])
{
	to[0] = from.x;
	to[1] = from.y;
	to[2] = from.z;
}

static Vector LoadVector(const float from[3])
{
	return {from[0], from[1], from[2]};
}
GameSoundSystem::~GameSoundSystem()
{
	if (MakeCurrent())
	{
		// Destroy all sources first so none of the effects are still referenced.
		m_Channels.clear();
		m_Sentences.reset();
		m_SoundCache.reset();

		alDeleteFilters(1, &m_Filter);
		m_Filter = 0;

		alDeleteFilters(1, &m_DirectFilter);
		m_DirectFilter = 0;

		alDeleteAuxiliaryEffectSlots(1, &m_AuxiliaryEffectSlot);
		m_AuxiliaryEffectSlot = 0;

		if (m_EchoEffectSlot != 0)
		{
			alDeleteAuxiliaryEffectSlots(1, &m_EchoEffectSlot);
			m_EchoEffectSlot = 0;
		}

		alDeleteEffects(m_RoomEffects.size(), m_RoomEffects.data());
		std::memset(m_RoomEffects.data(), 0, m_RoomEffects.size() * sizeof(ALuint));

		alDeleteEffects(m_VanillaReverbEffects.size(), m_VanillaReverbEffects.data());
		m_VanillaReverbEffects.fill(0);
		alDeleteEffects(m_VanillaEchoEffects.size(), m_VanillaEchoEffects.data());
		m_VanillaEchoEffects.fill(0);
	}

	g_Logging.RemoveLogger(m_SentencesLogger);
	m_SentencesLogger.reset();

	g_Logging.RemoveLogger(m_CacheLogger);
	m_CacheLogger.reset();
}

bool GameSoundSystem::Create(std::shared_ptr<spdlog::logger> logger)
{
	m_Logger = logger;
	m_CacheLogger = g_Logging.CreateLogger("sound.cache");
	m_SentencesLogger = g_Logging.CreateLogger("sound.sentences");

	m_Volume = gEngfuncs.pfnGetCvarPointer("volume");
	m_RoomOff = gEngfuncs.pfnGetCvarPointer("room_off");
	m_RoomType = gEngfuncs.pfnGetCvarPointer("room_type");
	m_WaterRoomType = gEngfuncs.pfnGetCvarPointer("waterroom_type");

	if (!m_Volume)
	{
		m_Logger->error("Couldn't get volume cvar");
		return false;
	}

	if (!m_RoomOff)
	{
		m_Logger->error("Couldn't get room_off cvar");
		return false;
	}

	if (!m_RoomType)
	{
		m_Logger->error("Couldn't get room_type cvar");
		return false;
	}

	if (!m_WaterRoomType)
	{
		m_Logger->error("Couldn't get waterroom_type cvar");
		return false;
	}

	m_Device.reset(alcOpenDevice(nullptr));

	if (!m_Device)
	{
		m_Logger->error("Couldn't open OpenAL device for GameSoundSystem");
		return false;
	}

	m_Logger->trace("OpenAL device opened for GameSoundSystem");

	if (!CheckOpenALContextExtension(m_Device.get(), "ALC_EXT_EFX", *m_Logger))
	{
		return false;
	}

	// johnhalflife: Steam Half-Life's own mixing and room effects (jhl_snd_vanilla), from the start; the device's own
	// rate to go back to.
	m_VanillaCvar = gEngfuncs.pfnRegisterVariable("jhl_snd_vanilla", "1", FCVAR_ARCHIVE);
	m_VanillaMix = m_VanillaCvar->value != 0;
	m_SupportsOutputMode = alcIsExtensionPresent(m_Device.get(), "ALC_SOFT_output_mode") != ALC_FALSE;
	m_SupportsLimiter = alcIsExtensionPresent(m_Device.get(), "ALC_SOFT_output_limiter") != ALC_FALSE;
	alcGetIntegerv(m_Device.get(), ALC_FREQUENCY, 1, &m_DefaultFrequency);

	if (m_DefaultFrequency <= 0)
	{
		m_DefaultFrequency = 48000;
	}

	std::vector<ALCint> attribs;
	BuildDeviceAttributes(attribs);

	m_Context.reset(alcCreateContext(m_Device.get(), attribs.data()));

	if (!m_Context)
	{
		m_Logger->error("Couldn't create OpenAL context");
		return false;
	}

	if (ALC_FALSE == alcMakeContextCurrent(m_Context.get()))
	{
		m_Logger->error("Couldn't make OpenAL context current");
		return false;
	}

	// An active context is required to query these.
	{
		const auto getOpenALString = [](ALenum param)
		{
			if (const auto result = alGetString(param); result)
			{
				return result;
			}

			return "Unknown";
		};

		m_Logger->trace("OpenAL vendor: {}", getOpenALString(AL_VENDOR));
		m_Logger->trace("OpenAL version: {}", getOpenALString(AL_VERSION));
		m_Logger->trace("OpenAL renderer: {}", getOpenALString(AL_RENDERER));
	}

	ALCint auxSendsCount = 0;
	alcGetIntegerv(m_Device.get(), ALC_MAX_AUXILIARY_SENDS, 1, &auxSendsCount);

	if (auxSendsCount < 1)
	{
		m_Logger->error("OpenAL does not provide enough auxiliary send slots for EFX use (requested {}, got {})", attribs[1], auxSendsCount);
		return false;
	}

	m_HasEchoSend = auxSendsCount >= 2;

	if (!m_HasEchoSend)
	{
		m_Logger->warn("OpenAL provides one auxiliary send: the engine's room echo is left out");
	}

	{
		ALCint monoSources = 0;
		ALCint stereoSources = 0;
		alcGetIntegerv(m_Device.get(), ALC_MONO_SOURCES, 1, &monoSources);
		alcGetIntegerv(m_Device.get(), ALC_STEREO_SOURCES, 1, &stereoSources);
		m_Logger->debug("OpenAL sources: {} mono, {} stereo (requested {}, {})",
			monoSources, stereoSources, RequestedMonoSources, RequestedStereoSources);
	}

	if (!CheckOpenALExtension("AL_SOFT_loop_points", *m_Logger))
	{
		return false;
	}

	// johnhalflife (jhl_snd_vanilla): sounds come to the engine's 22050 Hz as they load (SoundCache); OpenAL takes them
	// from there to the device's rate with its best sinc, as the system takes the engine's output (cleanly: nothing
	// above 11 kHz).
	m_SupportsResampler = alIsExtensionPresent("AL_SOFT_source_resampler") != AL_FALSE;

	if (m_SupportsResampler)
	{
		m_DefaultResampler = alGetInteger(AL_DEFAULT_RESAMPLER_SOFT);

		for (const std::string_view name : {"23rd order Sinc", "11th order Sinc"})
		{
			for (ALint i = 0; i < alGetInteger(AL_NUM_RESAMPLERS_SOFT) && m_VanillaResampler < 0; ++i)
			{
				if (name == alGetStringiSOFT(AL_RESAMPLER_NAME_SOFT, i))
				{
					m_VanillaResampler = i;
				}
			}
		}
	}

	alGenEffects(m_RoomEffects.size(), m_RoomEffects.data());

	if (alGetError() != AL_NO_ERROR)
	{
		m_Logger->error("Error generating effects");
		return false;
	}

	alGenAuxiliaryEffectSlots(1, &m_AuxiliaryEffectSlot);

	if (alGetError() != AL_NO_ERROR)
	{
		m_Logger->error("Error generating auxiliary effect slot");
		return false;
	}

	alGenFilters(1, &m_Filter);

	if (alGetError() != AL_NO_ERROR)
	{
		m_Logger->error("Error generating filter");
		return false;
	}

	alFilteri(m_Filter, AL_FILTER_TYPE, AL_FILTER_LOWPASS);

	// johnhalflife: the engine's water rooms (14-16; 14 under water) lowpass the whole mix, the direct sound too (a
	// five-tap average at 22050 Hz: about a tenth left by 4-5 kHz).
	alGenFilters(1, &m_DirectFilter);
	alFilteri(m_DirectFilter, AL_FILTER_TYPE, AL_FILTER_LOWPASS);
	alFilterf(m_DirectFilter, AL_LOWPASS_GAINHF, VanillaLowpassHFGain);

	for (unsigned int roomType = 0; auto& effect : m_RoomEffects)
	{
		SetupEffect(effect, roomType++);
	}

	if (alGetError() != AL_NO_ERROR)
	{
		m_Logger->error("Error setting up room effects");
		return false;
	}

	// johnhalflife: the engine's software DSP rooms (jhl_snd_vanilla).
	alGenEffects(m_VanillaReverbEffects.size(), m_VanillaReverbEffects.data());
	alGenEffects(m_VanillaEchoEffects.size(), m_VanillaEchoEffects.data());

	if (m_HasEchoSend)
	{
		alGenAuxiliaryEffectSlots(1, &m_EchoEffectSlot);
	}

	for (unsigned int roomType = 0; roomType < RoomEffectCount; ++roomType)
	{
		SetupVanillaReverb(m_VanillaReverbEffects[roomType], roomType);
		SetupVanillaEcho(m_VanillaEchoEffects[roomType], roomType);
	}

	// Not worth all sound: without them, the SDK's rooms.
	if (const ALenum error = alGetError(); error != AL_NO_ERROR)
	{
		m_Logger->error("Error setting up the engine's room effects ({:#x}): using the SDK's", error);
		m_VanillaRoomsWork = false;
	}

	// Immediately initialize the slot to a valid effect.
	UpdateRoomEffect();

	alDistanceModel(AL_LINEAR_DISTANCE);

	// GoldSource uses units, where 16 units == 1 foot.
	// See https://developer.valvesoftware.com/wiki/Dimensions
	constexpr double feetPerMeter = 3.281;
	constexpr double footInMeters = 1 / feetPerMeter;
	constexpr double metersPerUnit = footInMeters / 16;
	alListenerf(AL_METERS_PER_UNIT, static_cast<float>(metersPerUnit));

	alDopplerFactor(0.f);

	m_SoundCache = std::make_unique<SoundCache>(m_CacheLogger);
	m_SoundCache->SetEngineRate(m_VanillaMix);
	UpdateDeviceRate();
	m_Sentences = std::make_unique<SentencesSystem>(m_SentencesLogger, m_SoundCache.get());

	m_HRTFEnabled = g_ConCommands.CreateCVar("snd_hrtf_enabled", "0", FCVAR_ARCHIVE);
	m_HRTFImplementation = g_ConCommands.CreateCVar("snd_hrtf_implementation", "", FCVAR_ARCHIVE);
	g_ConCommands.CreateCommand("snd_hrtf_list_implementations", [this](const auto&)
		{ PrintHRTFImplementations(); });
	g_ConCommands.CreateCommand("snd_channels", [this](const auto&)
		{ PrintChannels(); });

	m_SupportsHRTF = alcIsExtensionPresent(m_Device.get(), "ALC_SOFT_HRTF") != ALC_FALSE;

	if (m_SupportsHRTF)
	{
		m_Logger->trace("HRTF is supported");
	}
	else
	{
		m_Logger->trace("HRTF is not supported");
	}

	return true;
}

void GameSoundSystem::OnBeginNetworkDataProcessing()
{
	// Clear all references to sounds and sentences.
	StopAllSounds();

	// Remove all sentences so we can load the incoming list.
	m_Sentences->Clear();

	// Clear the entire cache so we start fresh.
	m_SoundCache->Clear();

	m_PrecacheMap.clear();
}

void GameSoundSystem::HandleNetworkDataBlock(NetworkDataBlock& block)
{
	if (block.Name == "SoundList")
	{
		// The empty name, shouldn't be used.
		m_PrecacheMap.push_back(SoundIndex{});

		for (const auto& fileName : block.Data)
		{
			// TODO: avoid constructing multiple strings here
			m_PrecacheMap.push_back(m_SoundCache->FindName(fileName.get<std::string>().c_str()));
		}
	}
	else if (block.Name == "Sentences")
	{
		m_Sentences->HandleNetworkDataBlock(block);
	}
}

void GameSoundSystem::Update()
{
	if (!MakeCurrent())
	{
		return;
	}

	++m_CurrentGameFrame;

	if (m_SupportsHRTF)
	{
		if (const bool hrtfEnabled = m_HRTFEnabled->value != 0; m_CachedHRTFEnabled != hrtfEnabled)
		{
			m_CachedHRTFEnabled = hrtfEnabled;
			ResetDevice();
		}
	}

	// johnhalflife: the engine's sound switched on or off (jhl_snd_vanilla; its rooms follow in UpdateRoomEffect).
	if (const bool vanillaMix = m_VanillaCvar->value != 0; m_VanillaMix != vanillaMix)
	{
		m_VanillaMix = vanillaMix;
		ResetDevice();

		// Sounds already loaded keep how they were loaded (they're playing) until the next map load clears the cache.
		m_SoundCache->SetEngineRate(vanillaMix);

		for (auto& channel : m_Channels)
		{
			ApplySourceMode(channel);
		}
	}

	Vector origin, forward, right, up;

	if (UTIL_IsMapLoaded())
	{
		// Use view entity values here.
		origin = v_origin;
		AngleVectors(v_client_aimangles, forward, right, up);
	}
	else
	{
		origin = vec3_origin;
		forward = vec3_forward;
		right = vec3_right;
		up = vec3_up;
	}

	const ALfloat orientation[6] =
		{
			forward[0], forward[1], forward[2],
			up[0], up[1], up[2]};

	alListenerfv(AL_POSITION, origin);
	alListenerfv(AL_ORIENTATION, orientation);

	StoreVector(origin, m_ListenerOrigin);
	StoreVector(right, m_ListenerRight);

	// Update volume if changed. If we're currently blocked then volume is 0, so don't update it.
	if (!m_Blocked && m_LastKnownVolume != m_Volume->value)
	{
		m_LastKnownVolume = m_Volume->value;
		SetVolume();
	}

	UpdateSounds();

	UpdateRoomEffect();
}

void GameSoundSystem::Block()
{
	if (!MakeCurrent())
	{
		return;
	}

	m_Blocked = true;

	alListenerf(AL_GAIN, 0);
}

void GameSoundSystem::Unblock()
{
	if (!MakeCurrent())
	{
		return;
	}

	m_Blocked = false;

	// Unblock to user-set volume immediately.
	SetVolume();
}

void GameSoundSystem::Pause()
{
	if (!MakeCurrent())
	{
		return;
	}

	if (m_Paused)
	{
		return;
	}

	m_Paused = true;

	for (auto& channel : m_Channels)
	{
		alSourcePause(channel.Source.Id);
	}
}

void GameSoundSystem::Resume()
{
	if (!MakeCurrent())
	{
		return;
	}

	if (!m_Paused)
	{
		return;
	}

	m_Paused = false;

	for (auto& channel : m_Channels)
	{
		alSourcePlay(channel.Source.Id);
	}
}

void GameSoundSystem::StartSound(
	int entityIndex, int channelIndex,
	const char* soundOrSentence, const Vector& origin, float volume, float attenuation, int pitch, int flags)
{
	SoundData sound;

	if (soundOrSentence[0] != '!')
	{
		// Skip stream prefix.
		if (soundOrSentence[0] == '*')
		{
			++soundOrSentence;
		}

		sound = m_SoundCache->FindName(soundOrSentence);
	}
	else
	{
		sound = SentenceChannel{m_Sentences->FindSentence(soundOrSentence + 1), 0};
	}

	StartSound(entityIndex, channelIndex, std::move(sound), origin, volume, attenuation, pitch, flags);
}

void GameSoundSystem::StartSound(int entityIndex, int channelIndex, SoundData&& sound,
	const Vector& origin, float volume, float attenuation, int pitch, int flags)
{
	if (!MakeCurrent())
	{
		return;
	}

	if (!std::visit([&, this](auto&& sound)
			{
			using T = std::decay_t<decltype(sound)>;

			if constexpr (std::is_same_v<T, SoundIndex>)
			{
				if (!sound.IsValid())
				{
					return false;
				}

				auto& soundData = *m_SoundCache->GetSound(sound);

				return m_SoundCache->LoadSound(soundData);
			}
			else if constexpr (std::is_same_v<T, SentenceChannel>)
			{
				return m_Sentences->GetSentence(sound.Sentence) != nullptr;
			}
			else
			{
				static_assert(always_false_v<T>, "StartSound does not handle all sound types");
			}

			return false; },
			sound))
	{
		return;
	}

	if ((flags & (SND_STOP | SND_CHANGE_VOL | SND_CHANGE_PITCH)) != 0)
	{
		if (AlterChannel(entityIndex, channelIndex, sound, volume, pitch, flags))
		{
			return;
		}

		if ((flags & SND_STOP) != 0)
		{
			return;
		}
	}

	if (pitch == 0)
	{
		m_Logger->warn("StartSound ignored, called with pitch 0");
		return;
	}

	Channel* newChannel = FindOrCreateChannel(entityIndex, channelIndex);

	if (!newChannel)
	{
		return;
	}

	const std::string_view soundOrSentence = GetSoundName(sound);

	if (SetupChannel(*newChannel, entityIndex, channelIndex, std::move(sound), origin, volume, pitch, attenuation, false))
	{
		m_Logger->trace("Playing \"{}\": Entity {}, channel {}", soundOrSentence, entityIndex, channelIndex);

		if (!m_Paused || (flags & SND_PLAY_WHEN_PAUSED) != 0)
		{
			alSourcePlay(newChannel->Source.Id);
		}
	}
	else
	{
		RemoveChannel(*newChannel);
	}
}

void GameSoundSystem::StopAllSounds()
{
	m_Channels.clear();
}

void GameSoundSystem::MsgFunc_EmitSound(const char* pszName, BufferReader& reader)
{
	const int flags = reader.ReadByte();

	const float volume = (flags & SND_VOLUME) != 0 ? reader.ReadByte() / 255.f : 1;
	const float attenuation = (flags & SND_ATTENUATION) != 0 ? reader.ReadByte() / 64.f : 1;
	const int channel = reader.ReadByte();
	const int entityIndex = reader.ReadShort();

	int soundIndex;

	if ((flags & SND_LARGE_INDEX) != 0)
	{
		// Cast the signed short back to an unsigned short so the index is correct.
		// See ServerSoundSystem::EmitSound for why this is needed.
		soundIndex = static_cast<std::uint16_t>(reader.ReadShort());
	}
	else
	{
		soundIndex = reader.ReadByte();
	}

	Vector origin;
	origin.x = reader.ReadCoord();
	origin.y = reader.ReadCoord();
	origin.z = reader.ReadCoord();

	const int pitch = (flags & SND_PITCH) != 0 ? reader.ReadByte() : 100;

	const auto entity = gEngfuncs.GetEntityByIndex(entityIndex);

	if (!entity)
	{
		m_Logger->error("MsgFunc_EmitSound: Entity index {} is invalid", entityIndex);
		return;
	}

	const std::size_t absoluteSoundIndex = soundIndex >= 0 ? static_cast<std::size_t>(soundIndex) : SentencesSystem::InvalidIndex;

	if ((flags & SND_SENTENCE) == 0)
	{
		if (absoluteSoundIndex >= m_PrecacheMap.size())
		{
			m_Logger->error("MsgFunc_EmitSound: Invalid sound index {}", soundIndex);
			return;
		}

		const SoundIndex actualSoundIndex = m_PrecacheMap[absoluteSoundIndex];

		StartSound(entityIndex, channel, actualSoundIndex, origin, volume, attenuation, pitch, flags);
	}
	else
	{
		StartSound(entityIndex, channel, SentenceChannel{absoluteSoundIndex, 0}, origin, volume, attenuation, pitch, flags);
	}
}

bool GameSoundSystem::MakeCurrent()
{
	if (ALC_FALSE == alcMakeContextCurrent(m_Context.get()))
	{
		m_Logger->error("Couldn't make OpenAL context current");
		return false;
	}

	return true;
}

void GameSoundSystem::PrintHRTFImplementations()
{
	ALCint numHRTF;
	alcGetIntegerv(m_Device.get(), ALC_NUM_HRTF_SPECIFIERS_SOFT, 1, &numHRTF);

	if (numHRTF == 0)
	{
		Con_Printf("No HRTF implementations found\n");
		return;
	}

	Con_Printf("%d HRTF implementations found\n", numHRTF);

	for (ALCint j = 0; j < numHRTF; ++j)
	{
		const ALCchar* name = alcGetStringiSOFT(m_Device.get(), ALC_HRTF_SPECIFIER_SOFT, j);

		Con_Printf("%d: %s\n", j, name);
	}
}

void GameSoundSystem::BuildDeviceAttributes(std::vector<ALCint>& attribs)
{
	attribs.clear();

	// johnhalflife: 2, the second for the engine's room echo (jhl_snd_vanilla); the SDK's rooms only need 1.
	attribs.insert(attribs.end(), {ALC_MAX_AUXILIARY_SENDS, 2});

	// johnhalflife: every sound playing takes a source, and OpenAL Soft allows 256 unless asked for more.
	attribs.insert(attribs.end(), {ALC_MONO_SOURCES, RequestedMonoSources, ALC_STEREO_SOURCES, RequestedStereoSources});

	// johnhalflife: the engine mixes at 22050 Hz, in plain stereo, clipping (CLIP16) where OpenAL would limit.
	attribs.insert(attribs.end(), {ALC_FREQUENCY, m_VanillaMix ? VanillaMixFrequency : m_DefaultFrequency});

	if (m_SupportsOutputMode)
	{
		attribs.insert(attribs.end(), {ALC_OUTPUT_MODE_SOFT, m_VanillaMix ? ALC_STEREO_BASIC_SOFT : ALC_ANY_SOFT});
	}

	if (m_SupportsLimiter)
	{
		attribs.insert(attribs.end(), {ALC_OUTPUT_LIMITER_SOFT, m_VanillaMix ? ALC_FALSE : ALC_DONT_CARE_SOFT});
	}

	if (m_SupportsHRTF)
	{
		AddHRTFAttributes(attribs);
	}

	attribs.push_back(0);
}

void GameSoundSystem::UpdateDeviceRate()
{
	// johnhalflife: the rate the device really mixes at (OpenAL may not give the one asked for), for sounds at the
	// engine's rate to be converted to.
	ALCint frequency = 0;
	alcGetIntegerv(m_Device.get(), ALC_FREQUENCY, 1, &frequency);

	if (m_SoundCache)
	{
		m_SoundCache->SetDeviceRate(frequency);
	}
}

void GameSoundSystem::ResetDevice()
{
	// alcResetDeviceSOFT comes with ALC_SOFT_HRTF.
	if (!m_SupportsHRTF)
	{
		m_Logger->warn("OpenAL can't reset the device: sound settings apply on restart");
		return;
	}

	std::vector<ALCint> attribs;
	BuildDeviceAttributes(attribs);

	if (ALC_FALSE == alcResetDeviceSOFT(m_Device.get(), attribs.data()))
	{
		m_Logger->error("Failed to reset device: {}", alcGetString(m_Device.get(), alcGetError(m_Device.get())));
	}

	UpdateDeviceRate();
}

void GameSoundSystem::AddHRTFAttributes(std::vector<ALCint>& attribs)
{
	// johnhalflife: never with the engine's mixing, which has none (and HRTF needs 44.1 or 48 kHz).
	const bool enabled = m_CachedHRTFEnabled && !m_VanillaMix;

	ALCint numHRTF;
	alcGetIntegerv(m_Device.get(), ALC_NUM_HRTF_SPECIFIERS_SOFT, 1, &numHRTF);

	if (numHRTF == 0)
	{
		m_Logger->debug("No HRTF implementations found");
		return;
	}

	attribs.insert(attribs.end(), {ALC_HRTF_SOFT, enabled ? ALC_TRUE : ALC_FALSE});

	if (enabled)
	{
		ALCint implementationIndex = -1;

		if (m_HRTFImplementation->string[0] != '\0')
		{
			for (ALCint j = 0; j < numHRTF; ++j)
			{
				const ALCchar* name = alcGetStringiSOFT(m_Device.get(), ALC_HRTF_SPECIFIER_SOFT, j);

				if (strcmp(m_HRTFImplementation->string, name) == 0)
				{
					implementationIndex = j;
					break;
				}
			}

			if (implementationIndex == -1)
			{
				m_Logger->error("HRTF implementation \"{}\" not found", m_HRTFImplementation->string);
			}
		}

		if (implementationIndex != -1)
		{
			m_Logger->debug("Using HRTF implementation \"{}\" (index {})", m_HRTFImplementation->string, implementationIndex);
			attribs.insert(attribs.end(), {ALC_HRTF_ID_SOFT, implementationIndex});
		}
		else
		{
			m_Logger->debug("Using default HRTF implementation");
		}
	}
}

void GameSoundSystem::SetVolume()
{
	alListenerf(AL_GAIN, m_Volume->value);
}

std::string_view GameSoundSystem::GetSoundName(const SoundData& sound) const
{
	return std::visit([&, this](auto&& sound)
		{
			using T = std::decay_t<decltype(sound)>;

			if constexpr (std::is_same_v<T, SoundIndex>)
			{
				const auto& soundData = *m_SoundCache->GetSound(sound);
				return std::string_view{soundData.Name.data(), soundData.Name.size()};
			}
			else if constexpr (std::is_same_v<T, SentenceChannel>)
			{
				const auto& sentence = *m_Sentences->GetSentence(sound.Sentence);
				return std::string_view{sentence.Name.data(), sentence.Name.size()};
			}
			else
			{
				static_assert(always_false_v<T>, "GetSoundName does not handle all sound types");
			}

			return std::string_view{}; },
		sound);
}

Channel* GameSoundSystem::CreateChannel()
{
	Channel channel;

	// Clear errors left by earlier calls so a failure here is this one's.
	alGetError();

	channel.Source = OpenALSource::Create();

	// johnhalflife: out of sources, the sound played on no source at all and was silent, and so was every sound after
	// it until enough of those playing ended. As the engine did with its channels full, one about to end is taken over.
	if (alGetError() != AL_NO_ERROR || !channel.Source.IsValid())
	{
		channel.Source = OpenALSource{};
		return StealChannel();
	}

	UpdateSourceEffect(channel.Source);

	// johnhalflife: its resampler now; its spatialization as it's set up.
	if (m_SupportsResampler)
	{
		alSourcei(channel.Source.Id, AL_SOURCE_RESAMPLER_SOFT,
			GetResampler());
	}

	m_Channels.push_back(std::move(channel));

	return &m_Channels.back();
}

// johnhalflife: OpenAL Soft 1.22's stereo panning (plain stereo, ALC_STEREO_BASIC_SOFT), measured by rendering a mono
// source on a loopback device: for a source 0-60 degrees to one side, its near and far ear's gain (past 60, all near).
// Inverted, it plays the engine's own ear volumes exactly.
constexpr float StereoPanGains[61][2] = {
	{0.59566f, 0.59566f}, {0.60872f, 0.58254f}, {0.62170f, 0.56936f}, {0.63460f, 0.55614f}, {0.64740f, 0.54287f},
	{0.66011f, 0.52958f}, {0.67270f, 0.51627f}, {0.68518f, 0.50294f}, {0.69753f, 0.48962f}, {0.70974f, 0.47630f},
	{0.72181f, 0.46299f}, {0.73373f, 0.44972f}, {0.74549f, 0.43647f}, {0.75708f, 0.42327f}, {0.76849f, 0.41012f},
	{0.77972f, 0.39704f}, {0.79076f, 0.38402f}, {0.80160f, 0.37109f}, {0.81223f, 0.35824f}, {0.82265f, 0.34549f},
	{0.83285f, 0.33285f}, {0.84281f, 0.32032f}, {0.85255f, 0.30791f}, {0.86204f, 0.29563f}, {0.87128f, 0.28350f},
	{0.88027f, 0.27151f}, {0.88900f, 0.25968f}, {0.89747f, 0.24802f}, {0.90566f, 0.23653f}, {0.91357f, 0.22521f},
	{0.92120f, 0.21409f}, {0.92854f, 0.20316f}, {0.93558f, 0.19244f}, {0.94233f, 0.18192f}, {0.94878f, 0.17163f},
	{0.95491f, 0.16156f}, {0.96074f, 0.15172f}, {0.96625f, 0.14212f}, {0.97144f, 0.13277f}, {0.97630f, 0.12366f},
	{0.98084f, 0.11482f}, {0.98505f, 0.10624f}, {0.98893f, 0.09793f}, {0.99248f, 0.08989f}, {0.99568f, 0.08214f},
	{0.99855f, 0.07467f}, {1.00107f, 0.06749f}, {1.00325f, 0.06061f}, {1.00509f, 0.05403f}, {1.00658f, 0.04776f},
	{1.00772f, 0.04180f}, {1.00852f, 0.03615f}, {1.00896f, 0.03082f}, {1.00906f, 0.02581f}, {1.00881f, 0.02112f},
	{1.00821f, 0.01676f}, {1.00726f, 0.01274f}, {1.00596f, 0.00905f}, {1.00432f, 0.00569f}, {1.00233f, 0.00268f},
	{1.00000f, 0.00000f}};

ALint GameSoundSystem::GetResampler() const
{
	if (!m_VanillaMix)
	{
		return m_DefaultResampler;
	}

	return m_VanillaResampler >= 0 ? m_VanillaResampler : m_DefaultResampler;
}

void GameSoundSystem::ApplySourceMode(Channel& channel)
{
	if (m_SupportsResampler)
	{
		alSourcei(channel.Source.Id, AL_SOURCE_RESAMPLER_SOFT,
			GetResampler());
	}

	if (m_VanillaMix)
	{
		ApplyVanillaSpatialization(channel);
	}
	else
	{
		RestoreSpatialization(channel);
	}
}

void GameSoundSystem::GetVanillaEarVolumes(const Channel& channel, float& left, float& right) const
{
	left = channel.Volume;
	right = channel.Volume;

	// Its own sounds, and sounds without attenuation (both played relative), full in both ears.
	if (!channel.Relative)
	{
		const Vector toSound = LoadVector(channel.Origin) - LoadVector(m_ListenerOrigin);
		const float distance = toSound.Length();
		const float falloff = 1 - distance * channel.Attenuation / NominalClippingDistance;
		const float side = distance > 0 ? DotProduct(toSound / distance, LoadVector(m_ListenerRight)) : 0;

		left = channel.Volume * falloff * (1 - side);
		right = channel.Volume * falloff * (1 + side);
	}

	// The engine's channel volumes are 0-255.
	left = std::clamp(left, 0.f, 1.f);
	right = std::clamp(right, 0.f, 1.f);
}

void GameSoundSystem::ApplyVanillaSpatialization(Channel& channel)
{
	float left, right;
	GetVanillaEarVolumes(channel, left, right);

	const float nearEar = std::max(left, right);
	const float farEar = std::min(left, right);

	float degrees = 0;
	float gain = 0;

	if (nearEar > 0)
	{
		// The angle whose far to near ear ratio is this one's (falling from 1 at 0 to 0 at 60), then the gain for the
		// near ear's volume.
		const float ratio = farEar / nearEar;
		int i = 0;

		while (i < 59 && StereoPanGains[i + 1][1] / StereoPanGains[i + 1][0] > ratio)
		{
			++i;
		}

		const float upper = StereoPanGains[i][1] / StereoPanGains[i][0];
		const float lower = StereoPanGains[i + 1][1] / StereoPanGains[i + 1][0];
		const float t = upper > lower ? std::clamp((upper - ratio) / (upper - lower), 0.f, 1.f) : 0.f;

		degrees = i + t;
		gain = nearEar / (StereoPanGains[i][0] + (StereoPanGains[i + 1][0] - StereoPanGains[i][0]) * t);
	}

	const float angle = (right >= left ? degrees : -degrees) * std::numbers::pi_v<float> / 180;
	const Vector position{std::sin(angle), 0, -std::cos(angle)};

	alSourcei(channel.Source.Id, AL_SOURCE_RELATIVE, AL_TRUE);
	alSourcefv(channel.Source.Id, AL_POSITION, position);
	alSourcef(channel.Source.Id, AL_ROLLOFF_FACTOR, 0);
	alSourcef(channel.Source.Id, AL_MAX_GAIN, 4);
	alSourcef(channel.Source.Id, AL_GAIN, gain);
}

void GameSoundSystem::RestoreSpatialization(Channel& channel)
{
	alSourcei(channel.Source.Id, AL_SOURCE_RELATIVE, channel.Relative ? AL_TRUE : AL_FALSE);
	const Vector position = channel.Relative ? vec3_origin : LoadVector(channel.Origin);
	alSourcefv(channel.Source.Id, AL_POSITION, position);
	alSourcef(channel.Source.Id, AL_ROLLOFF_FACTOR, channel.Attenuation);
	alSourcef(channel.Source.Id, AL_MAX_GAIN, 1);
	alSourcef(channel.Source.Id, AL_GAIN, channel.Volume);
}

float GameSoundSystem::GetChannelAudibility(const Channel& channel) const
{
	ALint isRelative = AL_FALSE;
	alGetSourcei(channel.Source.Id, AL_SOURCE_RELATIVE, &isRelative);

	ALfloat gain = 0;
	alGetSourcef(channel.Source.Id, AL_GAIN, &gain);

	if (isRelative != AL_FALSE)
	{
		return gain;
	}

	Vector position, listener;
	ALfloat rolloff = 0;
	alGetSourcefv(channel.Source.Id, AL_POSITION, position);
	alGetListenerfv(AL_POSITION, listener);
	alGetSourcef(channel.Source.Id, AL_ROLLOFF_FACTOR, &rolloff);

	// AL_LINEAR_DISTANCE, its reference distance 0 (SetupChannel).
	const float distance = std::min((position - listener).Length(), NominalClippingDistance);
	return gain * std::max(0.f, 1 - rolloff * distance / NominalClippingDistance);
}

float GameSoundSystem::GetChannelTimeLeft(const Channel& channel) const
{
	ALint buffer = 0, size = 0, bits = 0, channels = 0, frequency = 0, offset = 0;
	alGetSourcei(channel.Source.Id, AL_BUFFER, &buffer);

	if (buffer == 0)
	{
		return 0;
	}

	alGetBufferi(buffer, AL_SIZE, &size);
	alGetBufferi(buffer, AL_BITS, &bits);
	alGetBufferi(buffer, AL_CHANNELS, &channels);
	alGetBufferi(buffer, AL_FREQUENCY, &frequency);
	alGetSourcei(channel.Source.Id, AL_SAMPLE_OFFSET, &offset);

	if (bits <= 0 || channels <= 0 || frequency <= 0)
	{
		return 0;
	}

	const float samples = size * 8.f / (bits * channels);
	ALfloat pitch = 1;
	alGetSourcef(channel.Source.Id, AL_PITCH, &pitch);

	return std::max(0.f, samples - offset) / frequency / std::max(pitch, 0.01f);
}

Channel* GameSoundSystem::StealChannel()
{
	// johnhalflife: of the one-shot sounds playing (never a sentence, nor a looping sound: nothing would start it again),
	// one that can't be heard, else the one closest to ending.
	Channel* best = nullptr;
	bool bestSilent = false;
	float bestTimeLeft = 0;

	for (auto& channel : m_Channels)
	{
		if (!std::holds_alternative<SoundIndex>(channel.Sound) || !channel.Source.IsValid())
			continue;

		if (m_SoundCache->GetSound(std::get<SoundIndex>(channel.Sound))->IsLooping)
			continue;

		const bool silent = GetChannelAudibility(channel) < 0.01f;
		const float timeLeft = GetChannelTimeLeft(channel);

		if (!best || (silent && !bestSilent) || (silent == bestSilent && timeLeft < bestTimeLeft))
		{
			best = &channel;
			bestSilent = silent;
			bestTimeLeft = timeLeft;
		}
	}

	// Said at most every few seconds, with what's playing, so a log shows what filled them.
	++m_StolenSinceWarning;

	if (const float now = gEngfuncs.GetClientTime(); now >= m_NextStealWarning || now < m_NextStealWarning - 10)
	{
		m_NextStealWarning = now + 5;
		m_Logger->warn("Out of sound sources ({} sounds playing): {} sound(s) took over another's since the last warning{}. "
					   "Playing most: {}",
			m_Channels.size(), m_StolenSinceWarning, best ? "" : ", this one couldn't (only loops and sentences playing)",
			DescribeChannels(6));
		m_StolenSinceWarning = 0;
	}

	if (!best)
	{
		return nullptr;
	}

	m_Logger->trace("Taking over \"{}\": entity {}, channel {}", GetSoundName(best->Sound), best->EntityIndex, best->ChannelIndex);
	ClearChannel(*best);
	return best;
}

std::string GameSoundSystem::DescribeChannels(std::size_t most) const
{
	struct Count
	{
		std::string_view Name;
		int Playing = 0;
		bool Looping = false;
	};

	std::vector<Count> counts;

	for (const auto& channel : m_Channels)
	{
		const std::string_view name = GetSoundName(channel.Sound);
		const bool looping = std::holds_alternative<SoundIndex>(channel.Sound) &&
							 m_SoundCache->GetSound(std::get<SoundIndex>(channel.Sound))->IsLooping;

		auto it = std::find_if(counts.begin(), counts.end(), [&](const Count& c)
			{ return c.Name == name; });

		if (it == counts.end())
			counts.push_back({name, 1, looping});
		else
			++it->Playing;
	}

	std::sort(counts.begin(), counts.end(), [](const Count& a, const Count& b)
		{ return a.Playing > b.Playing; });

	std::string text;

	for (std::size_t i = 0; i < counts.size() && i < most; ++i)
	{
		text += fmt::format("{}{} x{}{}", i > 0 ? ", " : "", counts[i].Name, counts[i].Playing, counts[i].Looping ? " (looping)" : "");
	}

	if (counts.size() > most)
	{
		text += fmt::format(", and {} other sounds", counts.size() - most);
	}

	return text;
}

void GameSoundSystem::PrintChannels()
{
	if (!MakeCurrent())
	{
		return;
	}

	int looping = 0;
	int silent = 0;

	for (const auto& channel : m_Channels)
	{
		if (std::holds_alternative<SoundIndex>(channel.Sound) && m_SoundCache->GetSound(std::get<SoundIndex>(channel.Sound))->IsLooping)
			++looping;

		if (GetChannelAudibility(channel) < 0.01f)
			++silent;
	}

	ALCint monoSources = 0;
	ALCint stereoSources = 0;
	alcGetIntegerv(m_Device.get(), ALC_MONO_SOURCES, 1, &monoSources);
	alcGetIntegerv(m_Device.get(), ALC_STEREO_SOURCES, 1, &stereoSources);

	Con_Printf("%d sounds playing (%d looping, %d out of earshot), of %d sources\n",
		static_cast<int>(m_Channels.size()), looping, silent, monoSources + stereoSources);
	Con_Printf("%s\n", DescribeChannels(m_Channels.size()).c_str());

	// johnhalflife: how the device is really mixing, and what the loudest sounds are given.
	ALCint frequency = 0, outputMode = 0, limiter = -1, hrtf = 0;
	alcGetIntegerv(m_Device.get(), ALC_FREQUENCY, 1, &frequency);
	if (m_SupportsOutputMode)
		alcGetIntegerv(m_Device.get(), ALC_OUTPUT_MODE_SOFT, 1, &outputMode);
	if (m_SupportsLimiter)
		alcGetIntegerv(m_Device.get(), ALC_OUTPUT_LIMITER_SOFT, 1, &limiter);
	if (m_SupportsHRTF)
		alcGetIntegerv(m_Device.get(), ALC_HRTF_SOFT, 1, &hrtf);

	Con_Printf("Mixing: vanilla %s (jhl_snd_vanilla %s), %d Hz, output mode %#x (basic stereo %#x), limiter %d, HRTF %d, "
			   "resampler %d (vanilla %d, default %d)\n",
		m_VanillaMix ? "on" : "off", m_VanillaCvar->string, frequency, outputMode, ALC_STEREO_BASIC_SOFT, limiter, hrtf,
		m_Channels.empty() || !m_SupportsResampler ? -1 : [&]
		{ ALint r = -1; alGetSourcei(m_Channels.front().Source.Id, AL_SOURCE_RESAMPLER_SOFT, &r); return r; }(),
		GetResampler(), m_DefaultResampler);

	std::vector<const Channel*> loudest;

	for (const auto& channel : m_Channels)
		loudest.push_back(&channel);

	std::sort(loudest.begin(), loudest.end(), [this](const Channel* a, const Channel* b)
		{ return GetChannelAudibility(*a) > GetChannelAudibility(*b); });

	for (std::size_t i = 0; i < loudest.size() && i < 12; ++i)
	{
		const Channel& channel = *loudest[i];
		float left = 0, right = 0;
		GetVanillaEarVolumes(channel, left, right);

		ALint relative = 0;
		ALfloat gain = 0, rolloff = 0;
		Vector position;
		alGetSourcei(channel.Source.Id, AL_SOURCE_RELATIVE, &relative);
		alGetSourcef(channel.Source.Id, AL_GAIN, &gain);
		alGetSourcef(channel.Source.Id, AL_ROLLOFF_FACTOR, &rolloff);
		alGetSourcefv(channel.Source.Id, AL_POSITION, position);

		Con_Printf("  %s: volume %.2f attn %.2f%s | engine's ears L %.2f R %.2f | given gain %.2f, relative %d, "
				   "at %.2f %.2f %.2f, rolloff %.2f\n",
			std::string{GetSoundName(channel.Sound)}.c_str(), channel.Volume, channel.Attenuation,
			channel.Relative ? " (own/everywhere)" : "", left, right, gain, relative, position.x, position.y, position.z, rolloff);
	}
}

void GameSoundSystem::ClearChannel(Channel& channel)
{
	if (std::holds_alternative<SentenceChannel>(channel.Sound))
	{
		m_Sentences->CloseMouth(channel.EntityIndex, channel.ChannelIndex);
	}

	alSourceStop(channel.Source.Id);

	// Detach buffer in case this is a time compressed buffer.
	alSourcei(channel.Source.Id, AL_BUFFER, NullBuffer);
}

void GameSoundSystem::RemoveChannel(Channel& channel)
{
	ClearChannel(channel);

	m_Channels.erase(m_Channels.begin() + (&channel - m_Channels.data()));
}

template <typename FilterFunction>
Channel* GameSoundSystem::FindDynamicChannel(int entityIndex, FilterFunction&& filterFunction)
{
	for (auto& channel : m_Channels)
	{
		if (channel.ChannelIndex == CHAN_STATIC)
			continue;

		if (channel.EntityIndex != entityIndex)
			continue;

		if (filterFunction(channel))
			return &channel;
	}

	return nullptr;
}

Channel* GameSoundSystem::FindOrCreateChannel(int entityIndex, int channelIndex)
{
	// The engine's original behavior works like this:
	// There are 128 channels. 4 ambient, 8 dynamic, 116 static.
	// Ambient channels are left over from Quake 1 and are never used.
	// For dynamic channels it depends on the channel index.
	// CHAN_AUTO picks the first unused channel or the channel closest to finishing its sound.
	// CHAN_VOICE prefers channels assigned to the current entity's voice channel,
	// otherwise same as CHAN_AUTO.
	// All other channels will reuse channels assigned to the current entity if
	// they are the same channel or if the requested channel is -1, otherwise same as CHAN_AUTO.
	// For static channels it always tries to find an unused static channel.

	// This sound system allocates channels on demand so it tries to follow the old rules first,
	// falling back to allocating a channel if no in-use channels match.
	if (channelIndex != CHAN_STATIC && channelIndex != CHAN_AUTO)
	{
		Channel* channel = nullptr;

		if (channelIndex == CHAN_VOICE)
		{
			// Always override existing voice lines.
			channel = FindDynamicChannel(entityIndex, [&](Channel& channel)
				{ return channel.ChannelIndex == CHAN_VOICE; });
		}
		else if (channelIndex != -1)
		{
			channel = FindDynamicChannel(entityIndex, [&](Channel& channel)
				{ return channel.ChannelIndex == channelIndex; });
		}
		else
		{
			channel = FindDynamicChannel(entityIndex, [&](Channel& channel)
				{ return true; });
		}

		if (channel)
		{
			m_Logger->trace("Clearing \"{}\": entity {}, channel {}", GetSoundName(channel->Sound), entityIndex, channelIndex);
			ClearChannel(*channel);
			return channel;
		}
	}

	return CreateChannel();
}

bool GameSoundSystem::SetupChannel(Channel& channel, int entityIndex, int channelIndex,
	SoundData&& sound, const Vector& origin, float volume, int pitch, float attenuation, bool isRelative)
{
	channel.Sound = std::move(sound);
	channel.EntityIndex = entityIndex;
	channel.ChannelIndex = channelIndex;
	channel.Pitch = pitch;
	channel.CreatedOnFrame = m_CurrentGameFrame;

	// If attenuation is 0 then the sound will play everywhere.
	// If the entity is the current view entity then it should always sound like it's playing "here".
	// Make it relative to the listener to stop it from having 3D spatialization.
	if (!isRelative && (attenuation == 0 || entityIndex == g_ViewEntity))
	{
		isRelative = true;
		alSourcefv(channel.Source.Id, AL_POSITION, vec3_origin);
	}
	else
	{
		alSourcefv(channel.Source.Id, AL_POSITION, origin);
	}

	alSourcef(channel.Source.Id, AL_GAIN, volume);
	alSourcef(channel.Source.Id, AL_PITCH, pitch / 100.f);
	alSourcef(channel.Source.Id, AL_ROLLOFF_FACTOR, attenuation);
	alSourcef(channel.Source.Id, AL_REFERENCE_DISTANCE, 0);
	alSourcef(channel.Source.Id, AL_MAX_DISTANCE, NominalClippingDistance);
	alSourcei(channel.Source.Id, AL_SOURCE_RELATIVE, isRelative ? AL_TRUE : AL_FALSE);
	alSourcei(channel.Source.Id, AL_LOOPING, AL_FALSE);

	// johnhalflife: for the engine's spatialization (a sentence sets its own word's volume next).
	StoreVector(origin, channel.Origin);
	channel.Volume = volume;
	channel.Attenuation = attenuation;
	channel.Relative = isRelative;
	alSourcef(channel.Source.Id, AL_MAX_GAIN, 1);

	alSourcei(channel.Source.Id, AL_BUFFER, NullBuffer);

	const bool success = std::visit([&, this](auto&& sound)
		{
			using T = std::decay_t<decltype(sound)>;

			if constexpr (std::is_same_v<T, SoundIndex>)
			{
				auto& soundData = *m_SoundCache->GetSound(sound);

				// johnhalflife (jhl_snd_vanilla): off its own pitch, the sound pitched as the engine pitches it, played
				// at that pitch's own rate.
				channel.BufferPitch = PITCH_NORM;
				ALuint buffer = soundData.Buffer.Id;

				if (m_VanillaMix && pitch != PITCH_NORM)
				{
					if (const PitchedSound* pitched = m_SoundCache->GetPitched(soundData, pitch); pitched)
					{
						buffer = pitched->Buffer.Id;
						channel.BufferPitch = pitch;
						alSourcef(channel.Source.Id, AL_PITCH, 1);
					}
				}

				alSourcei(channel.Source.Id, AL_BUFFER, buffer);
				alSourcei(channel.Source.Id, AL_LOOPING, soundData.IsLooping ? AL_TRUE : AL_FALSE);
			}
			else if constexpr (std::is_same_v<T, SentenceChannel>)
			{
				m_Sentences->InitMouth(channel.EntityIndex, channel.ChannelIndex);

				// Can't just queue all of them because they don't always have the same format, which isn't supported by OpenAL.
				return m_Sentences->SetSentenceWord(channel, sound);
			}
			else
			{
				static_assert(always_false_v<T>, "SetupChannel does not handle all sound types");
			}

			return true; },
		channel.Sound);

	if (!success)
	{
		return false;
	}

	if (channelIndex != CHAN_STATIC)
	{
		// Offset play start position if there are other sounds with the same sound starting this frame.
		// This avoids the sounds stacking to essentially create a higher volume sound.
		// Does not apply to sentences since they are rarely playing at the same time and need to play as intended.
		if (std::holds_alternative<SoundIndex>(channel.Sound))
		{
			const auto& soundData = *m_SoundCache->GetSound(std::get<SoundIndex>(channel.Sound));

			for (auto& otherChannel : m_Channels)
			{
				if (&otherChannel == &channel)
				{
					continue;
				}

				if (otherChannel.Sound != channel.Sound)
				{
					continue;
				}

				if (otherChannel.CreatedOnFrame != channel.CreatedOnFrame)
				{
					continue;
				}

				// Don't query this; it causes performance issues with HRTF enabled.
				// ALint offset = -1;
				// alGetSourcei(otherChannel.Source.Id, AL_BYTE_OFFSET, &offset);

				// if (offset == 0)
				{
					// johnhalflife: the buffer it plays, which with the engine's mixing may be a pitched version.
					ALint otherBuffer = static_cast<ALint>(soundData.Buffer.Id);
					alGetSourcei(otherChannel.Source.Id, AL_BUFFER, &otherBuffer);

					ALint frequency = 0;
					alGetBufferi(otherBuffer, AL_FREQUENCY, &frequency);

					ALint channels = 0;
					alGetBufferi(otherBuffer, AL_CHANNELS, &channels);

					ALint size = 0;
					alGetBufferi(otherBuffer, AL_SIZE, &size);

					// Need to convert the frequency to bytes for this to work.
					const ALint sampleSizeInBytes = channels * sizeof(float);
					const ALint sampleRateInBytes = frequency * sampleSizeInBytes;

					ALint skip = static_cast<ALint>(gEngfuncs.pfnRandomLong(0, (int)(0.1 * sampleRateInBytes)));

					// If the sound is empty or really short it could be out of range.
					skip = std::clamp(skip, 0, std::max(0, size - sampleSizeInBytes));

					alSourcei(otherChannel.Source.Id, AL_BYTE_OFFSET, skip);
					break;
				}
			}
		}
	}

	if (m_VanillaMix)
	{
		ApplyVanillaSpatialization(channel);
	}

	return true;
}

bool GameSoundSystem::AlterChannel(int entityIndex, int channelIndex, const SoundData& sound, float volume, int pitch, int flags)
{
	decltype(m_Channels.begin()) existingChannel;

	if (std::holds_alternative<SentenceChannel>(sound))
	{
		existingChannel = std::find_if(m_Channels.begin(), m_Channels.end(), [&](const auto& candidate)
			{ return candidate.EntityIndex == entityIndex && candidate.ChannelIndex == channelIndex && std::holds_alternative<SentenceChannel>(candidate.Sound); });
	}
	else
	{
		existingChannel = std::find_if(m_Channels.begin(), m_Channels.end(), [&](const auto& candidate)
			{ return candidate.EntityIndex == entityIndex && candidate.ChannelIndex == channelIndex && candidate.Sound == sound; });
	}

	if (existingChannel == m_Channels.end())
	{
		return false;
	}

	auto& channelToAlter = *existingChannel;

	if ((flags & SND_CHANGE_VOL) != 0)
	{
		alSourcef(channelToAlter.Source.Id, AL_GAIN, volume);
		channelToAlter.Volume = volume;

		if (m_VanillaMix)
		{
			ApplyVanillaSpatialization(channelToAlter);
		}
	}

	if ((flags & SND_CHANGE_PITCH) != 0)
	{
		channelToAlter.Pitch = pitch;
		// johnhalflife: relative to the pitch its buffer was made at (jhl_snd_vanilla), cleanly from there.
		alSourcef(channelToAlter.Source.Id, AL_PITCH, pitch / static_cast<float>(channelToAlter.BufferPitch));
	}

	if ((flags & SND_STOP) != 0)
	{
		RemoveChannel(channelToAlter);
	}

	return true;
}

void GameSoundSystem::UpdateRoomEffect()
{
	// Allow users to disable room effects for the OpenAL version separately.
	const bool roomOn = g_cl_snd_room_off->value == 0 && m_RoomOff->value == 0;

	if (roomOn)
	{
		const auto isUnderwater = UTIL_IsMapLoaded() ? g_WaterLevel >= WaterLevel::Head : false;

		const auto cvar = isUnderwater ? m_WaterRoomType : m_RoomType;


		int roomType = static_cast<int>(cvar->value);

		if (roomType < 0 || roomType >= RoomEffectCount)
		{
			roomType = 0;
		}

		// johnhalflife: the engine's water rooms lowpass the whole mix, its effects too (jhl_snd_vanilla).
		float hfGain = isUnderwater ? UnderwaterLowPassHFGain : NormalLowPassHFGain;

		if (m_VanillaRoomsWork && m_VanillaCvar->value != 0 && VanillaLowpass(roomType))
		{
			hfGain = NormalLowPassHFGain * VanillaLowpassHFGain;
		}

		if (m_PreviousRoomOn != roomOn)
		{
			m_PreviousRoomOn = roomOn;

			// Force a reset if we're turning effects back on.
			m_PreviousRoomType = -1;
			m_PreviousHFGain = -1;
		}

		// johnhalflife: the engine's rooms or the SDK's, switched as the cvar changes.
		const bool vanilla = m_VanillaRoomsWork && m_VanillaCvar->value != 0;

		if (m_PreviousVanilla != vanilla)
		{
			m_PreviousVanilla = vanilla;
			m_PreviousRoomType = -1;
		}

		if (m_PreviousRoomType != roomType)
		{
			m_PreviousRoomType = roomType;
			alAuxiliaryEffectSloti(m_AuxiliaryEffectSlot, AL_EFFECTSLOT_EFFECT,
				static_cast<ALint>(vanilla ? m_VanillaReverbEffects[roomType] : m_RoomEffects[roomType]));

			if (m_HasEchoSend)
			{
				alAuxiliaryEffectSloti(m_EchoEffectSlot, AL_EFFECTSLOT_EFFECT,
					vanilla ? static_cast<ALint>(m_VanillaEchoEffects[roomType]) : AL_EFFECT_NULL);
				alAuxiliaryEffectSlotf(m_EchoEffectSlot, AL_EFFECTSLOT_GAIN, vanilla ? VanillaEchoGain(roomType) : 0.f);
			}

			SetDirectLowpass(vanilla && VanillaLowpass(roomType));
		}

		if (m_PreviousHFGain != hfGain)
		{
			m_PreviousHFGain = hfGain;
			alFilterf(m_Filter, AL_LOWPASS_GAINHF, hfGain);

			// Re-attach the filter to all active sources.
			for (auto& channel : m_Channels)
			{
				UpdateSourceEffect(channel.Source);
			}
		}
	}
	else if (m_PreviousRoomOn != roomOn)
	{
		m_PreviousRoomOn = roomOn;
		alAuxiliaryEffectSloti(m_AuxiliaryEffectSlot, AL_EFFECTSLOT_EFFECT, AL_EFFECT_NULL);

		if (m_HasEchoSend)
		{
			alAuxiliaryEffectSloti(m_EchoEffectSlot, AL_EFFECTSLOT_EFFECT, AL_EFFECT_NULL);
		}

		SetDirectLowpass(false);
	}
}

void GameSoundSystem::SetDirectLowpass(bool lowpass)
{
	if (m_DirectLowpass == lowpass)
	{
		return;
	}

	m_DirectLowpass = lowpass;

	for (auto& channel : m_Channels)
	{
		alSourcei(channel.Source.Id, AL_DIRECT_FILTER, static_cast<ALint>(lowpass ? m_DirectFilter : AL_FILTER_NULL));
	}
}

void GameSoundSystem::UpdateSourceEffect(OpenALSource& source)
{
	alSource3i(source.Id, AL_AUXILIARY_SEND_FILTER, static_cast<ALint>(m_AuxiliaryEffectSlot), 0, m_Filter);

	if (m_HasEchoSend)
	{
		alSource3i(source.Id, AL_AUXILIARY_SEND_FILTER, static_cast<ALint>(m_EchoEffectSlot), 1, m_Filter);
	}

	alSourcei(source.Id, AL_DIRECT_FILTER, static_cast<ALint>(m_DirectLowpass ? m_DirectFilter : AL_FILTER_NULL));
}

void GameSoundSystem::UpdateSounds()
{
	const auto localPlayer = UTIL_IsMapLoaded() ? gEngfuncs.GetLocalPlayer() : nullptr;

	const int messagenum = localPlayer ? localPlayer->curstate.messagenum : 0;

	// Update all sounds that are looping, clear finished sounds.
	for (std::size_t index = 0; index < m_Channels.size();)
	{
		auto& channel = m_Channels[index];

		const bool remove = std::visit([&, this](auto&& sound)
			{
				using T = std::decay_t<decltype(sound)>;

				if constexpr (std::is_same_v<T, SoundIndex>)
				{
					return UpdateSound(channel);
				}
				else if constexpr (std::is_same_v<T, SentenceChannel>)
				{
					return m_Sentences->UpdateSentence(channel);
				}
				else
				{
					static_assert(always_false_v<T>, "UpdateSounds does not handle all sound types");
				} },
			channel.Sound);

		if (remove)
		{
			RemoveChannel(channel);
		}
		else
		{
			Spatialize(channel, messagenum);

			++index;
		}
	}
}

bool GameSoundSystem::UpdateSound(Channel& channel)
{
	ALint state = AL_STOPPED;
	alGetSourcei(channel.Source.Id, AL_SOURCE_STATE, &state);

	if (state == AL_STOPPED)
	{
		return true;
	}

	m_Sentences->MoveMouth(channel, *m_SoundCache->GetSound(std::get<SoundIndex>(channel.Sound)));

	return false;
}

void GameSoundSystem::Spatialize(Channel& channel, int messagenum)
{
	// Update sound channel position.
	if (channel.EntityIndex > 0)
	{
		auto entity = gEngfuncs.GetEntityByIndex(channel.EntityIndex);

		// Entities without a model are not sent to the client so there is no point in updating their position.
		// Don't update relative sources (always vec3_origin).
		if (entity && entity->model && entity->curstate.messagenum == messagenum && !channel.Relative)
		{
			const Vector position = entity->model->type == mod_brush
										? entity->origin + (entity->model->mins + entity->model->maxs) * 0.5f
										: entity->origin;

			StoreVector(position, channel.Origin);

			if (!m_VanillaMix)
			{
				alSourcefv(channel.Source.Id, AL_POSITION, position);
			}
		}
	}

	// johnhalflife: the engine's spatialization follows the listener too, every frame.
	if (m_VanillaMix)
	{
		ApplyVanillaSpatialization(channel);
	}
}
}
