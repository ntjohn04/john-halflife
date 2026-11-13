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

#include <future>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include <spdlog/logger.h>

#include <libnyquist/Decoders.h>

#include "IGameSoundSystem.h"
#include "OpenALUtils.h"
#include "SoundDefs.h"
#include "utils/string_utils.h"

namespace sound
{
// johnhalflife: @brief johnhalflife
struct ConvertedSound
{
	bool Ok = false;
	std::string Path;
	ALenum Format = 0;
	int Rate = 0;
	std::vector<float> Samples;
	std::optional<std::tuple<ALint, ALint>> Loop;
	std::vector<float> NativeSamples;
	int NativeRate = 0;
	int ChannelCount = 1;
	int NativeLoopStart = -1;
	int NativeLoopEnd = -1;
};

// johnhalflife: @brief johnhalflife
struct ConvertedPitch
{
	bool Ok = false;
	int Rate = 0;
	std::vector<float> Samples;
	std::optional<std::tuple<ALint, ALint>> Loop;
};

/**
 *	@brief Maintains a cache of sounds and provides a means of loading them.
 */
class SoundCache final
{
private:
	// Comparer that allows us to access the sound name in the set without making copies.
	struct LookupComparer
	{
		using is_transparent = void;

		explicit LookupComparer(std::vector<Sound>* cache)
			: m_Cache(cache)
		{
		}

		bool operator()(std::size_t lhs, std::size_t rhs) const
		{
			return UTIL_CompareI(ToStringView((*m_Cache)[lhs].Name), ToStringView((*m_Cache)[rhs].Name)) < 0;
		}

		bool operator()(std::string_view lhs, std::size_t rhs) const
		{
			return UTIL_CompareI(lhs, ToStringView((*m_Cache)[rhs].Name)) < 0;
		}

		bool operator()(std::size_t lhs, std::string_view rhs) const
		{
			return UTIL_CompareI(ToStringView((*m_Cache)[lhs].Name), rhs) < 0;
		}

	private:
		std::vector<Sound>* const m_Cache;
	};

public:
	explicit SoundCache(std::shared_ptr<spdlog::logger> logger);

	SoundIndex FindName(const RelativeFilename& fileName);

	Sound* GetSound(SoundIndex index);

	bool LoadSound(Sound& sound);

	// johnhalflife: @brief johnhalflife
	bool IsReady(SoundIndex index, int pitch);

	// johnhalflife: @brief johnhalflife
	void Request(SoundIndex index, int pitch);

	// johnhalflife: @brief johnhalflife
	void Poll();

	// johnhalflife: @brief johnhalflife
	void Preload(SoundIndex index, bool all);

	// About 3 seconds of a 16-bit 11025 Hz sound.
	static constexpr int PreloadBytes = 64 * 1024;

	void ClearBuffers();

	void Clear();

	// johnhalflife: @brief johnhalflife
	void SetEngineRate(bool engineRate)
	{
		m_EngineRate = engineRate;
	}

	bool IsEngineRate() const { return m_EngineRate; }

	// johnhalflife: @brief johnhalflife
	void SetDeviceRate(int deviceRate)
	{
		m_DeviceRate = deviceRate;
	}

	// johnhalflife: @brief johnhalflife
	std::vector<float> ToDeviceRate(const std::vector<float>& samples, std::size_t channels, ALint loopStart, ALint loopEnd) const;

	// johnhalflife
	const PitchedSound* GetPitched(Sound& sound, int pitch);

	// johnhalflife: @brief johnhalflife
	static const std::vector<float>& GetSamples(const Sound& sound, int pitch);

	static std::vector<float> Resample(const std::vector<float>& samples, std::size_t channels, double step, bool interpolate);
	static std::tuple<ALint, ALint> ScaleLoop(ALint start, ALint end, double step, std::size_t frames);

	static constexpr int EngineMixRate = 22050;

private:
	std::optional<std::tuple<ALint, ALint>> TryLoadCuePoints(
		const std::string& fileName, ALint sampleCount, int channelCount);

	// johnhalflife
	bool FindFile(const Sound& sound, std::string& absolutePath);
	ConvertedSound Convert(const std::string& absolutePath, nqr::NyquistIO& loader);
	bool Upload(Sound& sound, ConvertedSound&& converted);
	ConvertedPitch ConvertPitch(const std::vector<float>& nativeSamples, int nativeRate, int channelCount,
		int loopStart, int loopEnd, int pitch) const;
	const PitchedSound* UploadPitch(Sound& sound, int pitch, ConvertedPitch&& converted);

	struct Conversion
	{
		ConvertedSound Base;
		bool HasBase = false;
		ConvertedPitch Pitched;
		bool HasPitch = false;
	};

	struct Job
	{
		SoundIndex Index;
		int Pitch;
		std::future<Conversion> Result;
	};

private:
	std::shared_ptr<spdlog::logger> m_Logger;
	std::vector<Sound> m_Sounds;

	std::set<std::size_t, LookupComparer> m_SoundLookup{LookupComparer{&m_Sounds}};

	std::unique_ptr<nqr::NyquistIO> m_Loader;
	std::vector<Job> m_Jobs;

	bool m_EngineRate{false};
	int m_DeviceRate{0};
};
}
