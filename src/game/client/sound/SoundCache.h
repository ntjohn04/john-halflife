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
/**
 *	@brief johnhalflife: a sound read and converted, ready to be handed to OpenAL (SoundCache::Convert, Upload): the
 *	work, apart from OpenAL, so it can be done on another thread.
 */
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

/**
 *	@brief johnhalflife: a sound at a pitch, converted (SoundCache::ConvertPitch, UploadPitch).
 */
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

	/**
	 *	@brief johnhalflife: whether a sound can start at once: loaded, and at @p pitch (jhl_snd_vanilla) made.
	 */
	bool IsReady(SoundIndex index, int pitch);

	/**
	 *	@brief johnhalflife: a sound not ready read and converted on another thread (a scripted speech's first play
	 *	took 20-30 ms in the frame); Poll hands it to OpenAL when done.
	 */
	void Request(SoundIndex index, int pitch);

	/**
	 *	@brief johnhalflife: work Request started that's done, handed to OpenAL (each frame).
	 */
	void Poll();

	/**
	 *	@brief johnhalflife: loads a sound now, so its first play doesn't stall a frame reading and converting it (a
	 *	map's sounds, as the map loads): with @p all every one (jhl_perf_preloadsounds, as the engine loads its
	 *	precached sounds), else only a long one (PreloadBytes or more: a long one took several milliseconds).
	 */
	void Preload(SoundIndex index, bool all);

	// About 3 seconds of a 16-bit 11025 Hz sound.
	static constexpr int PreloadBytes = 64 * 1024;

	void ClearBuffers();

	void Clear();

	/**
	 *	@brief johnhalflife: sounds loaded from now on stepped to the engine's mixing rate (jhl_snd_vanilla).
	 */
	void SetEngineRate(bool engineRate)
	{
		m_EngineRate = engineRate;
	}

	bool IsEngineRate() const { return m_EngineRate; }

	/**
	 *	@brief johnhalflife: the device's mixing rate, which sounds at the engine's rate are converted to (0: none).
	 */
	void SetDeviceRate(int deviceRate)
	{
		m_DeviceRate = deviceRate;
	}

	/**
	 *	@brief johnhalflife: samples at the engine's 22050 Hz to the device's rate, as the system converts the engine's
	 *	output (flat to near 11 kHz); a loop (frames, -1: none) is wrapped round so its seam stays seamless.
	 */
	std::vector<float> ToDeviceRate(const std::vector<float>& samples, std::size_t channels, ALint loopStart, ALint loopEnd) const;

	/**
	 *	@brief johnhalflife (jhl_snd_vanilla): the sound at a pitch, pitched as the engine pitches it, made the first
	 *	time it's asked for; null if it can't be (not loaded at the engine's rate).
	 */
	const PitchedSound* GetPitched(Sound& sound, int pitch);

	/**
	 *	@brief johnhalflife: the samples a sound plays at a pitch (its pitched version's, if made), for mouths.
	 */
	static const std::vector<float>& GetSamples(const Sound& sound, int pitch);

	static std::vector<float> Resample(const std::vector<float>& samples, std::size_t channels, double step, bool interpolate);
	static std::tuple<ALint, ALint> ScaleLoop(ALint start, ALint end, double step, std::size_t frames);

	static constexpr int EngineMixRate = 22050;

private:
	std::optional<std::tuple<ALint, ALint>> TryLoadCuePoints(
		const std::string& fileName, ALint sampleCount, int channelCount);

	// johnhalflife: loading split so the work can be done on another thread: the file found (the engine's file system,
	// the main thread), read and converted (any thread), handed to OpenAL (the main thread).
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
