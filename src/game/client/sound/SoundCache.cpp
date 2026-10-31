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

#include <AL/alext.h>

#include "cbase.h"
#include "RiffChunkReader.h"
#include "SoundCache.h"

namespace sound
{
SoundCache::SoundCache(std::shared_ptr<spdlog::logger> logger)
	: m_Logger(logger),
	  m_Loader(std::make_unique<nqr::NyquistIO>())
{
}

SoundIndex SoundCache::FindName(const RelativeFilename& fileName)
{
	if (const auto lookup = m_SoundLookup.find(ToStringView(fileName)); lookup != m_SoundLookup.end())
	{
		return SoundIndex(*lookup + 1);
	}

	if (m_Sounds.size() >= static_cast<std::size_t>(std::numeric_limits<int>::max()))
	{
		// Chances are you'll run out of memory before this happens.
		m_Logger->warn("Out of sound slots, cannot load {} (maximum {})",
			fileName.c_str(), std::numeric_limits<int>::max());
		return {};
	}

	// Store as lowercase to ensure that extension-based lookups succeed.
	RelativeFilename copy = fileName;

	copy.make_lower();

	m_Sounds.emplace_back(std::move(copy));

	const std::size_t index = m_Sounds.size() - 1;

	m_SoundLookup.insert(index);

	return SoundIndex{static_cast<int>(index) + 1};
}

Sound* SoundCache::GetSound(SoundIndex index)
{
	const int i = index.Index - 1;

	if (i < 0 || static_cast<std::size_t>(i) >= m_Sounds.size())
	{
		return nullptr;
	}

	return &m_Sounds[i];
}

bool SoundCache::LoadSound(Sound& sound)
{
	if (sound.Buffer.IsValid())
	{
		return true;
	}

	m_Logger->trace("Loading sound {}", sound.Name.c_str());

	if (sound.Name.empty())
	{
		m_Logger->error("Sound has no name");
		return false;
	}

	Filename completeFileName = SoundDirectoryName;

	if (sound.Name.front() != '/')
	{
		completeFileName += '/';
	}

	completeFileName.append(sound.Name.begin(), sound.Name.end());

	std::string absolutePath;
	absolutePath.resize(MAX_PATH_LENGTH);

	if (!g_pFileSystem->GetLocalPath(completeFileName.c_str(), absolutePath.data(), absolutePath.size()))
	{
		// TODO: add support for EASTL types to fmt formatting.
		m_Logger->error("Could not find sound file {}", sound.Name.c_str());
		return false;
	}

	// Trim the size to the actual string's size.
	absolutePath.resize(strlen(absolutePath.c_str()));

	nqr::AudioData data;

	try
	{
		m_Loader->Load(&data, absolutePath);
	}
	catch (const std::exception& e)
	{
		m_Logger->error("Error loading sound file {}: {}", sound.Name.c_str(), e.what());
		return false;
	}

	// Clear error state.
	alGetError();

	const ALenum format = data.channelCount == 1 ? AL_FORMAT_MONO_FLOAT32 : AL_FORMAT_STEREO_FLOAT32;

	sound.Buffer = OpenALBuffer::Create();

	m_Logger->trace("Loading sound {} into buffer {}", absolutePath, sound.Buffer.Id);

	// See https://openal-soft.org/openal-extensions/SOFT_loop_points.txt
	auto cuePoints = TryLoadCuePoints(absolutePath, data.samples.size(), data.channelCount);

	// johnhalflife (jhl_snd_vanilla): the engine mixes at 22050 Hz, a sound at its own pitch brought to it interpolated
	// linearly: done here, once, so OpenAL takes it on to the device's rate filtered, as the system takes the engine's
	// output. The sound as loaded is kept, for its pitched versions (GetPitched).
	if (m_EngineRate && data.sampleRate > 0 && data.channelCount > 0)
	{
		sound.NativeSamples = data.samples;
		sound.NativeRate = data.sampleRate;
		sound.ChannelCount = data.channelCount;

		if (cuePoints)
		{
			sound.NativeLoopStart = std::get<0>(*cuePoints);
			sound.NativeLoopEnd = std::get<1>(*cuePoints);
		}

		if (data.sampleRate != EngineMixRate)
		{
			const double step = static_cast<double>(data.sampleRate) / EngineMixRate;
			auto stepped = Resample(data.samples, static_cast<std::size_t>(data.channelCount), step, true);

			if (cuePoints)
			{
				cuePoints = ScaleLoop(std::get<0>(*cuePoints), std::get<1>(*cuePoints), step,
					stepped.size() / static_cast<std::size_t>(data.channelCount));
			}

			data.samples = std::move(stepped);
			data.sampleRate = EngineMixRate;
		}
	}

	alBufferData(sound.Buffer.Id, format,
		data.samples.data(), data.samples.size() * sizeof(float), data.sampleRate);

	if (cuePoints)
	{
		const ALint loopPoints[2] = {std::get<0>(*cuePoints), std::get<1>(*cuePoints)};
		alBufferiv(sound.Buffer.Id, AL_LOOP_POINTS_SOFT, loopPoints);
	}

	if (const auto error = alGetError(); error != AL_NO_ERROR)
	{
		m_Logger->error("OpenAL error {} ({}) while initializing buffer for \"{}\"",
			alGetString(error), error, absolutePath);
		sound.Buffer.Delete();
		return false;
	}

	sound.IsLooping = cuePoints.has_value();
	sound.Format = format;
	// Cache the samples for future use.
	sound.Samples = std::move(data.samples);

	return true;
}

std::vector<float> SoundCache::Resample(const std::vector<float>& samples, std::size_t channels, double step, bool interpolate)
{
	const std::size_t frames = samples.size() / channels;

	if (frames == 0 || step <= 0)
	{
		return {};
	}

	const std::size_t outFrames = std::max<std::size_t>(static_cast<std::size_t>(frames / step), 1);
	std::vector<float> out(outFrames * channels);

	for (std::size_t i = 0; i < outFrames; ++i)
	{
		const double position = i * step;
		const std::size_t from = std::min(static_cast<std::size_t>(position), frames - 1);
		const std::size_t next = std::min(from + 1, frames - 1);
		const float fraction = interpolate ? static_cast<float>(position - static_cast<double>(from)) : 0.f;

		for (std::size_t c = 0; c < channels; ++c)
		{
			const float a = samples[from * channels + c];
			const float b = samples[next * channels + c];
			out[i * channels + c] = a + (b - a) * fraction;
		}
	}

	return out;
}

std::tuple<ALint, ALint> SoundCache::ScaleLoop(ALint start, ALint end, double step, std::size_t frames)
{
	const auto scale = [&](ALint frame)
	{ return static_cast<ALint>(std::min<std::size_t>(static_cast<std::size_t>(frame / step), frames)); };

	// OpenAL refuses a loop that doesn't start before it ends.
	const ALint scaledEnd = std::max<ALint>(scale(end), 1);
	return {std::min(scale(start), scaledEnd - 1), scaledEnd};
}

const PitchedSound* SoundCache::GetPitched(Sound& sound, int pitch)
{
	if (auto it = sound.Pitched.find(pitch); it != sound.Pitched.end())
	{
		return &it->second;
	}

	if (sound.NativeSamples.empty() || sound.NativeRate <= 0 || pitch <= 0)
	{
		return nullptr;
	}

	// The engine pitches a sound by stepping through it at its own rate without interpolating (fixed point, truncated),
	// and only then brings it to its 22050 Hz: off the sound's own pitch the uneven steps scatter a faint distortion
	// through the whole band, the grain of vanilla's sound (nearly everything plays a little off pitch: voices 100-103,
	// guns 98-102). A sound faster than 22050 Hz is pitched from its 22050 Hz version.
	const std::size_t channels = static_cast<std::size_t>(sound.ChannelCount);
	const double pitchStep = pitch / 100.0;
	const bool slower = sound.NativeRate <= EngineMixRate;

	std::vector<float> samples = Resample(slower ? sound.NativeSamples : sound.Samples, channels, pitchStep, false);

	if (slower && sound.NativeRate != EngineMixRate)
	{
		samples = Resample(samples, channels, static_cast<double>(sound.NativeRate) / EngineMixRate, true);
	}

	// Native frames to these: through the pitch, and to 22050 Hz.
	const double totalStep = pitchStep * sound.NativeRate / EngineMixRate;

	PitchedSound pitched;
	pitched.Buffer = OpenALBuffer::Create();

	alGetError();

	alBufferData(pitched.Buffer.Id, sound.Format, samples.data(), samples.size() * sizeof(float), EngineMixRate);

	if (sound.NativeLoopStart >= 0)
	{
		const auto loop = ScaleLoop(sound.NativeLoopStart, sound.NativeLoopEnd, totalStep, samples.size() / channels);
		const ALint loopPoints[2] = {std::get<0>(loop), std::get<1>(loop)};
		alBufferiv(pitched.Buffer.Id, AL_LOOP_POINTS_SOFT, loopPoints);
	}

	if (const auto error = alGetError(); error != AL_NO_ERROR)
	{
		m_Logger->error("OpenAL error {} ({}) while making \"{}\" at pitch {}", alGetString(error), error, sound.Name.c_str(), pitch);
		return nullptr;
	}

	pitched.Samples = std::move(samples);

	return &sound.Pitched.emplace(pitch, std::move(pitched)).first->second;
}

const std::vector<float>& SoundCache::GetSamples(const Sound& sound, int pitch)
{
	if (pitch != PITCH_NORM)
	{
		if (auto it = sound.Pitched.find(pitch); it != sound.Pitched.end())
		{
			return it->second.Samples;
		}
	}

	return sound.Samples;
}

void SoundCache::ClearBuffers()
{
	for (auto& sound : m_Sounds)
	{
		sound.Samples.clear();
		sound.Buffer.Delete();
		sound.NativeSamples.clear();
		sound.Pitched.clear();
	}
}

void SoundCache::Clear()
{
	m_SoundLookup.clear();
	m_Sounds.clear();
}

std::optional<std::tuple<ALint, ALint>> SoundCache::TryLoadCuePoints(
	const std::string& fileName, ALint sampleCount, int channelCount)
{
	m_Logger->trace("Trying to load loop info from file \"{}\"", fileName);

	if (!fileName.ends_with(CuePointExtension))
	{
		m_Logger->trace("File \"{}\" does not have the {} extension, ignoring", fileName, CuePointExtension);
		return {};
	}

	std::unique_ptr<FILE, decltype(fclose)*> file{fopen(fileName.c_str(), "rb"), &fclose};

	if (!file)
	{
		m_Logger->error("Couldn't open file \"{}\" for reading", fileName);
		return {};
	}

	fseek(file.get(), 0, SEEK_END);
	const long size = ftell(file.get());

	if (size < 0)
	{
		m_Logger->error("Error seeking file \"{}\"", fileName);
		return {};
	}

	fseek(file.get(), 0, SEEK_SET);

	std::vector<std::byte> data;

	data.resize(size);

	if (fread(data.data(), 1, size, file.get()) != static_cast<std::size_t>(size))
	{
		m_Logger->error("Error reading file \"{}\"", fileName);
		return {};
	}

	file.reset();

	RiffFileReader fileReader{data.data(), data.size()};

	if (!fileReader)
	{
		m_Logger->error("Not a wave file: \"{}\"", fileName);
		return {};
	}

	auto chunkReader = fileReader.GetChunkReader();

	// Find the fmt chunk so we can get the original bit depth.
	const auto fmtChunk = chunkReader.FindChunk("fmt ");

	if (!fmtChunk)
	{
		// Should never happen since the file was already loaded before.
		m_Logger->error("Wave file \"{}\" missing fmt chunk", fileName);
		return {};
	}

	if (fmtChunk->Size < 16)
	{
		m_Logger->error("Wave file \"{}\" has bad fmt chunk", fileName);
		return {};
	}

	const std::int16_t bitDepth = *reinterpret_cast<const std::int16_t*>(fmtChunk->Data + 14);

	if (bitDepth < 0)
	{
		m_Logger->error("Wave file \"{}\" has bad bit depth {}", fileName, bitDepth);
		return {};
	}

	const int width = (bitDepth / 8) * channelCount;

	chunkReader = fileReader.GetChunkReader();

	// Search for a cue chunk.
	// See https://www.recordingblogs.com/wiki/cue-chunk-of-a-wave-file for more information.
	const auto cueChunk = chunkReader.FindChunk("cue ");

	if (!cueChunk)
	{
		m_Logger->debug("No cue chunk in file \"{}\"", fileName);
		return {};
	}

	const int numberOfCuePoints = *reinterpret_cast<const int*>(cueChunk->Data);

	// Note: this check does not exist in Quake and GoldSource.
	if (numberOfCuePoints <= 0)
	{
		return {};
	}

	const auto convertRawSampleCount = [&](int rawSampleCount)
	{
		return rawSampleCount / width;
	};

	// There is at least one cue point in this file. Use the first one to denote the loop start point.
	const int cuePositionInBytes = *reinterpret_cast<const int*>(cueChunk->Data + 4 + 20);

	// Convert loop start from bytes to 32 bit float samples.
	std::tuple<ALint, ALint> loopPoints = {convertRawSampleCount(cuePositionInBytes), sampleCount / channelCount};

	m_Logger->trace("Loaded loop start point {} from file \"{}\"", std::get<0>(loopPoints), fileName);

	// See if there's a list chunk containing an associated data list following the cue chunk.
	// See https://www.recordingblogs.com/wiki/associated-data-list-chunk-of-a-wave-file for more information.
	const auto listChunk = chunkReader.FindChunk("LIST");

	if (!listChunk)
	{
		return loopPoints;
	}

	if (0 != strncmp("adtl", reinterpret_cast<const char*>(listChunk->Data), 4))
	{
		return loopPoints;
	}

	RiffChunkReader adtlChunkReader{listChunk->Data + 4, listChunk->Size};

	// Note: Quake and GoldSource assume this chunk exists and is present as the first sub chunk.
	auto ltxtChunk = adtlChunkReader.FindChunk("ltxt");

	if (!ltxtChunk)
	{
		return loopPoints;
	}

	if (ltxtChunk->Size < 12)
	{
		m_Logger->error("Wave file \"{}\" has bad ltxt subchunk", fileName);
		return loopPoints;
	}

	const std::string_view purposeId{reinterpret_cast<const char*>(ltxtChunk->Data + 8), 4};

	if (purposeId != "mark")
	{
		m_Logger->debug("Wave file \"{}\" has ltxt subchunk with purpose ID \"{}\" (expected \"mark\")", fileName, purposeId);
		return loopPoints;
	}

	const int remainingSampleCount = *reinterpret_cast<const int*>(ltxtChunk->Data + 4);
	const int sampleEndInBytes = cuePositionInBytes + remainingSampleCount;

	int loopSampleCount = convertRawSampleCount(sampleEndInBytes);

	if (loopSampleCount == 0)
	{
		m_Logger->debug("Wave file \"{}\" has bad loop length value {}, falling back to total sample length",
			fileName, remainingSampleCount);
		loopSampleCount = sampleCount - convertRawSampleCount(cuePositionInBytes);
	}

	std::get<1>(loopPoints) = loopSampleCount;

	m_Logger->trace("Loaded loop end point {} from file \"{}\"", std::get<1>(loopPoints), fileName);

	return loopPoints;
}
}
