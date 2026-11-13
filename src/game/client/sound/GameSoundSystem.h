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
#include <chrono>
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

	// johnhalflife: @brief The device's settings
	void BuildDeviceAttributes(std::vector<ALCint>& attribs);
	void AddHRTFAttributes(std::vector<ALCint>& attribs);
	void ResetDevice();
	void UpdateDeviceRate();

	// johnhalflife: @brief johnhalflife
	void ApplySourceMode(Channel& channel);

	// johnhalflife: @brief johnhalflife
	ALint GetResampler() const;

	// johnhalflife: @brief johnhalflife
	void ApplyVanillaSpatialization(Channel& channel);
	void GetVanillaEarVolumes(const Channel& channel, float& left, float& right) const;

	// johnhalflife: @brief johnhalflife
	void RestoreSpatialization(Channel& channel);

	void SetVolume();

	std::string_view GetSoundName(const SoundData& sound) const;

	Channel* CreateChannel();

	// johnhalflife: @brief johnhalflife
	Channel* StealChannel();

	// johnhalflife: @brief johnhalflife
	float GetChannelAudibility(const Channel& channel) const;

	// johnhalflife: @brief johnhalflife
	float GetChannelTimeLeft(const Channel& channel) const;

	// johnhalflife: @brief johnhalflife
	std::string DescribeChannels(std::size_t most) const;

	// johnhalflife: @brief johnhalflife
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

	void UpdateSourceEffect(Channel& channel);

	// johnhalflife: @brief johnhalflife
	bool ReplaceRoomSlot(ALuint response);

	// johnhalflife: @brief johnhalflife
	ALuint GetVanillaResponse(int roomType);

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

	// johnhalflife
	std::array<ALuint, RoomEffectCount> m_VanillaReverbEffects{};
	std::array<ALuint, RoomEffectCount> m_VanillaEchoEffects{};
	ALuint m_EchoEffectSlot{0};
	bool m_HasEchoSend{false};
	bool m_VanillaRoomsWork{true};
	bool m_PreviousVanilla{false};

	// johnhalflife
	ALuint m_ConvolutionEffect{0};
	std::array<ALuint, RoomEffectCount> m_VanillaResponses{};
	ALCint m_VanillaResponsesRate{0};
	bool m_ConvolutionRoom{false};

	ALuint m_AuxiliaryEffectSlot{0};

	ALuint m_Filter{0};

	// johnhalflife
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

	// johnhalflife
	static constexpr ALCint RequestedMonoSources = 1024;
	static constexpr ALCint RequestedStereoSources = 64;
	float m_NextStealWarning{0};
	int m_StolenSinceWarning{0};

	// johnhalflife: the engine's mixing (jhl_snd_vanilla).
	static constexpr ALCint VanillaMixFrequency = 22050;
	cvar_t* m_PreloadAllCvar = nullptr; // johnhalflife
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

	// johnhalflife
	struct PendingStart
	{
		int EntityIndex;
		int ChannelIndex;
		SoundIndex Sound;			  // A sound,
		std::size_t Sentence = SIZE_MAX; // or a sentence.
		Vector Origin;
		float Volume;
		float Attenuation;
		int Pitch;
		int Flags;
		std::chrono::steady_clock::time_point Since;
	};

	std::vector<PendingStart> m_PendingStarts;

	void StartPendingSounds();

	// johnhalflife: @brief johnhalflife
	bool IsReady(const PendingStart& start, bool request);
};
}
