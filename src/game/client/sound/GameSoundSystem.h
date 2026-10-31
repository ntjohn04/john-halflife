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

#pragma once

#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <spdlog/logger.h>

#include "HalfLifeSoundEffects.h"
#include "IGameSoundSystem.h"
#include "SentencesSystem.h"
#include "SoundCache.h"
#include "SoundDefs.h"
#include "OpenALUtils.h"

struct cvar_t;

namespace sound
{
class GameSoundSystem final : public IGameSoundSystem
{
public:
	~GameSoundSystem() override;

	bool Create(std::shared_ptr<spdlog::logger> logger);

	void OnBeginNetworkDataProcessing() override;

	void HandleNetworkDataBlock(NetworkDataBlock& block) override;

	void Update();

	void Block();

	void Unblock();

	void Pause();

	void Resume();

	void StartSound(int entityIndex, int channelIndex, const char* soundOrSentence,
		const Vector& origin, float volume, float attenuation, int flags, int pitch) override;

	void StartSound(int entityIndex, int channelIndex, SoundData&& sound,
		const Vector& origin, float volume, float attenuation, int pitch, int flags) override;

	void StopAllSounds() override;

	void MsgFunc_EmitSound(const char* pszName, BufferReader& reader) override;

private:
	bool MakeCurrent();

	void PrintHRTFImplementations();

	/**
	 *	@brief The device's settings: HRTF (cl_snd_hrtf_enabled) and, johnhalflife, the sources and sends asked for and
	 *	the engine's mixing (jhl_snd_vanilla: plain stereo, no limiter, no HRTF).
	 */
	void BuildDeviceAttributes(std::vector<ALCint>& attribs);
	void AddHRTFAttributes(std::vector<ALCint>& attribs);
	void ResetDevice();

	/**
	 *	@brief johnhalflife: a channel's resampler and spatialization, as jhl_snd_vanilla has them.
	 */
	void ApplySourceMode(Channel& channel);

	/**
	 *	@brief johnhalflife: the resampler sounds play with: OpenAL's default, or with the engine's mixing the one
	 *	taking them from 22050 Hz to the device's rate.
	 */
	ALint GetResampler() const;

	/**
	 *	@brief johnhalflife: the engine's spatialization (SND_Spatialize): each ear (1 - distance) * (1 -+ the source's
	 *	side), its own sounds and sounds without attenuation full in both, played through OpenAL's panning inverted.
	 */
	void ApplyVanillaSpatialization(Channel& channel);
	void GetVanillaEarVolumes(const Channel& channel, float& left, float& right) const;

	/**
	 *	@brief johnhalflife: back to OpenAL's own spatialization.
	 */
	void RestoreSpatialization(Channel& channel);

	void SetVolume();

	std::string_view GetSoundName(const SoundData& sound) const;

	Channel* CreateChannel();

	/**
	 *	@brief johnhalflife: with no source left, a one-shot sound to take over (one out of earshot, else the one
	 *	closest to ending), cleared; null if only loops and sentences are playing.
	 */
	Channel* StealChannel();

	/**
	 *	@brief johnhalflife: how loud a channel is where the listener is (its volume, after distance), 0-1.
	 */
	float GetChannelAudibility(const Channel& channel) const;

	/**
	 *	@brief johnhalflife: seconds left of a channel's sound.
	 */
	float GetChannelTimeLeft(const Channel& channel) const;

	/**
	 *	@brief johnhalflife: the sounds playing most, by name, with how many of each.
	 */
	std::string DescribeChannels(std::size_t most) const;

	/**
	 *	@brief johnhalflife: @c cl_snd_channels, every sound playing.
	 */
	void PrintChannels();

	/**
	 *	@brief Clears a channel in preparation to be reused.
	 */
	void ClearChannel(Channel& channel);

	void RemoveChannel(Channel& channel);

	template <typename FilterFunction>
	Channel* FindDynamicChannel(int entityIndex, FilterFunction&& filterFunction);

	Channel* FindOrCreateChannel(int entityIndex, int channelIndex);

	bool SetupChannel(Channel& channel, int entityIndex, int channelIndex,
		SoundData&& sound, const Vector& origin, float volume, int pitch, float attenuation, bool isRelative);

	bool AlterChannel(int entityIndex, int channelIndex, const SoundData& sound, float volume, int pitch, int flags);

	void UpdateRoomEffect();

	void UpdateSourceEffect(OpenALSource& source);

	void SetDirectLowpass(bool lowpass);

	void UpdateSounds();

	bool UpdateSound(Channel& channel);

	void Spatialize(Channel& channel, int messagenum);

private:
	std::shared_ptr<spdlog::logger> m_Logger;
	std::shared_ptr<spdlog::logger> m_CacheLogger;
	std::shared_ptr<spdlog::logger> m_SentencesLogger;

	cvar_t* m_Volume{};
	cvar_t* m_RoomOff{};
	cvar_t* m_RoomType{};
	cvar_t* m_WaterRoomType{};

	cvar_t* m_HRTFEnabled{};
	cvar_t* m_HRTFImplementation{};

	bool m_SupportsHRTF{false};
	bool m_CachedHRTFEnabled{false};

	std::unique_ptr<ALCdevice, DeleterWrapper<alcCloseDevice>> m_Device;
	std::unique_ptr<ALCcontext, DeleterWrapper<alcDestroyContext>> m_Context;

	std::array<ALuint, RoomEffectCount> m_RoomEffects{};

	// johnhalflife: room types as the engine's software DSP has them (jhl_snd_vanilla): its reverb on the first
	// send, its echo on a second.
	std::array<ALuint, RoomEffectCount> m_VanillaReverbEffects{};
	std::array<ALuint, RoomEffectCount> m_VanillaEchoEffects{};
	ALuint m_EchoEffectSlot{0};
	bool m_HasEchoSend{false};
	bool m_VanillaRoomsWork{true};
	bool m_PreviousVanilla{false};

	ALuint m_AuxiliaryEffectSlot{0};

	ALuint m_Filter{0};

	// johnhalflife: the engine's water rooms' lowpass on the direct sound (jhl_snd_vanilla).
	static constexpr float VanillaLowpassHFGain = 0.1f;
	ALuint m_DirectFilter{0};
	bool m_DirectLowpass{false};

	int m_CurrentGameFrame{0};

	// Force a room type update on startup.
	bool m_PreviousRoomOn{true};
	int m_PreviousRoomType{-1};
	float m_PreviousHFGain{-1.f};

	std::unique_ptr<SoundCache> m_SoundCache;
	std::unique_ptr<SentencesSystem> m_Sentences;
	std::vector<Channel> m_Channels;

	bool m_Blocked{false};
	bool m_Paused{false};

	float m_LastKnownVolume{-1};

	// johnhalflife: sources asked of OpenAL (its default is 256 in all), and when running out was last said.
	static constexpr ALCint RequestedMonoSources = 1024;
	static constexpr ALCint RequestedStereoSources = 64;
	float m_NextStealWarning{0};
	int m_StolenSinceWarning{0};

	// johnhalflife: the engine's mixing (jhl_snd_vanilla).
	static constexpr ALCint VanillaMixFrequency = 22050;
	cvar_t* m_VanillaCvar{};
	bool m_VanillaMix{false};
	ALCint m_DefaultFrequency{0};
	bool m_SupportsOutputMode{false};
	bool m_SupportsLimiter{false};
	bool m_SupportsResampler{false};
	ALint m_VanillaResampler{-1};
	ALint m_DefaultResampler{0};
	float m_ListenerOrigin[3]{};
	float m_ListenerRight[3]{};

	std::vector<SoundIndex> m_PrecacheMap;
};
}
