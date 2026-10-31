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

#include <algorithm>
#include <cassert>
#include <cmath>

#include <AL/al.h>
#define AL_ALEXT_PROTOTYPES
#include <AL/alext.h>

#include <AL/efx-presets.h>

#include "cbase.h"
#include "HalfLifeSoundEffects.h"

namespace sound
{
constexpr EFXEAXREVERBPROPERTIES EAXREVERB_PRESET_GENERIC = EFX_REVERB_PRESET_GENERIC;
constexpr EFXEAXREVERBPROPERTIES EAXREVERB_PRESET_ROOM = EFX_REVERB_PRESET_ROOM;
constexpr EFXEAXREVERBPROPERTIES EAXREVERB_PRESET_BATHROOM = EFX_REVERB_PRESET_BATHROOM;
constexpr EFXEAXREVERBPROPERTIES EAXREVERB_PRESET_STONEROOM = EFX_REVERB_PRESET_STONEROOM;
constexpr EFXEAXREVERBPROPERTIES EAXREVERB_PRESET_CONCERTHALL = EFX_REVERB_PRESET_CONCERTHALL;
constexpr EFXEAXREVERBPROPERTIES EAXREVERB_PRESET_ARENA = EFX_REVERB_PRESET_ARENA;
constexpr EFXEAXREVERBPROPERTIES EAXREVERB_PRESET_STONECORRIDOR = EFX_REVERB_PRESET_STONECORRIDOR;
constexpr EFXEAXREVERBPROPERTIES EAXREVERB_PRESET_SEWERPIPE = EFX_REVERB_PRESET_SEWERPIPE;
constexpr EFXEAXREVERBPROPERTIES EAXREVERB_PRESET_UNDERWATER = EFX_REVERB_PRESET_UNDERWATER;
constexpr EFXEAXREVERBPROPERTIES EAXREVERB_PRESET_DIZZY = EFX_REVERB_PRESET_DIZZY;

struct RoomEffect
{
	const EFXEAXREVERBPROPERTIES* Properties;
	float Volume;
	float DecayTime;
	float Damping;
};

constexpr RoomEffect RoomEffects[RoomEffectCount] =
	{
		// Note: the decay time and damping for generic are 0 in the engine. The legal minimum is 0.1.
		{&EAXREVERB_PRESET_GENERIC, 0.0, 0.1, 0.1},
		{&EAXREVERB_PRESET_ROOM, 0.417, 0.40000001, 0.66600001},
		{&EAXREVERB_PRESET_BATHROOM, 0.30000001, 1.499, 0.16599999},
		{&EAXREVERB_PRESET_BATHROOM, 0.40000001, 1.499, 0.16599999},
		{&EAXREVERB_PRESET_BATHROOM, 0.60000002, 1.499, 0.16599999},
		{&EAXREVERB_PRESET_SEWERPIPE, 0.40000001, 2.8859999, 0.25},
		{&EAXREVERB_PRESET_SEWERPIPE, 0.60000002, 2.8859999, 0.25},
		{&EAXREVERB_PRESET_SEWERPIPE, 0.80000001, 2.8859999, 0.25},
		{&EAXREVERB_PRESET_STONEROOM, 0.5, 2.309, 0.88800001},
		{&EAXREVERB_PRESET_STONEROOM, 0.64999998, 2.309, 0.88800001},
		{&EAXREVERB_PRESET_STONEROOM, 0.80000001, 2.309, 0.88800001},
		{&EAXREVERB_PRESET_STONECORRIDOR, 0.30000001, 2.697, 0.63800001},
		{&EAXREVERB_PRESET_STONECORRIDOR, 0.5, 2.697, 0.63800001},
		{&EAXREVERB_PRESET_STONECORRIDOR, 0.64999998, 2.697, 0.63800001},
		// Note: the damping for underwater types is 0 in the engine.
		{&EAXREVERB_PRESET_UNDERWATER, 1.0, 1.499, 0.1},
		{&EAXREVERB_PRESET_UNDERWATER, 1.0, 2.4990001, 0.1},
		{&EAXREVERB_PRESET_UNDERWATER, 1.0, 3.4990001, 0.1},
		{&EAXREVERB_PRESET_GENERIC, 0.64999998, 1.493, 0.5},
		{&EAXREVERB_PRESET_GENERIC, 0.85000002, 1.493, 0.5},
		{&EAXREVERB_PRESET_GENERIC, 1.0, 1.493, 0.5},
		{&EAXREVERB_PRESET_ARENA, 0.40000001, 7.2839999, 0.33199999},
		{&EAXREVERB_PRESET_ARENA, 0.55000001, 7.2839999, 0.33199999},
		{&EAXREVERB_PRESET_ARENA, 0.69999999, 7.2839999, 0.33199999},
		{&EAXREVERB_PRESET_CONCERTHALL, 0.5, 3.961, 0.5},
		{&EAXREVERB_PRESET_CONCERTHALL, 0.69999999, 3.961, 0.5},
		{&EAXREVERB_PRESET_CONCERTHALL, 1.0, 3.961, 0.5},
		{&EAXREVERB_PRESET_DIZZY, 0.2, 17.233999, 0.66600001},
		{&EAXREVERB_PRESET_DIZZY, 0.30000001, 17.233999, 0.66600001},
		{&EAXREVERB_PRESET_DIZZY, 0.40000001, 17.233999, 0.66600001}};

void SetupEffect(ALuint effectId, unsigned int roomType)
{
	if (roomType >= RoomEffectCount)
	{
		assert(!"Invalid room type");
		roomType = 0;
	}

	// Based on http://openal.org/pipermail/openal/2014-March/000083.html

	const auto& effectData = RoomEffects[roomType];

	auto reverb = *effectData.Properties;

	reverb.flDecayTime = effectData.DecayTime;
	reverb.flDecayHFRatio = effectData.Damping;
	// According to the above mail this should be assigned to flLateReverbGain but the original effects work better with this.
	reverb.flGain = effectData.Volume;

	alEffecti(effectId, AL_EFFECT_TYPE, AL_EFFECT_EAXREVERB);

	alEffectf(effectId, AL_EAXREVERB_DENSITY, reverb.flDensity);
	alEffectf(effectId, AL_EAXREVERB_DIFFUSION, reverb.flDiffusion);

	alEffectf(effectId, AL_EAXREVERB_GAIN, reverb.flGain);
	alEffectf(effectId, AL_EAXREVERB_GAINHF, reverb.flGainHF);
	alEffectf(effectId, AL_EAXREVERB_GAINLF, reverb.flGainLF);

	alEffectf(effectId, AL_EAXREVERB_DECAY_TIME, reverb.flDecayTime);
	alEffectf(effectId, AL_EAXREVERB_DECAY_HFRATIO, reverb.flDecayHFRatio);
	alEffectf(effectId, AL_EAXREVERB_DECAY_LFRATIO, reverb.flDecayLFRatio);

	alEffectf(effectId, AL_EAXREVERB_REFLECTIONS_GAIN, reverb.flReflectionsGain);
	alEffectf(effectId, AL_EAXREVERB_REFLECTIONS_DELAY, reverb.flReflectionsDelay);
	alEffectfv(effectId, AL_EAXREVERB_REFLECTIONS_PAN, reverb.flReflectionsPan);

	alEffectf(effectId, AL_EAXREVERB_LATE_REVERB_GAIN, reverb.flLateReverbGain);
	alEffectf(effectId, AL_EAXREVERB_LATE_REVERB_DELAY, reverb.flLateReverbDelay);
	alEffectfv(effectId, AL_EAXREVERB_LATE_REVERB_PAN, reverb.flLateReverbPan);

	alEffectf(effectId, AL_EAXREVERB_ECHO_TIME, reverb.flEchoTime);
	alEffectf(effectId, AL_EAXREVERB_ECHO_DEPTH, reverb.flEchoDepth);

	alEffectf(effectId, AL_EAXREVERB_MODULATION_TIME, reverb.flModulationTime);
	alEffectf(effectId, AL_EAXREVERB_MODULATION_DEPTH, reverb.flModulationDepth);

	alEffectf(effectId, AL_EAXREVERB_AIR_ABSORPTION_GAINHF, reverb.flAirAbsorptionGainHF);

	alEffectf(effectId, AL_EAXREVERB_HFREFERENCE, reverb.flHFReference);
	alEffectf(effectId, AL_EAXREVERB_LFREFERENCE, reverb.flLFReference);

	alEffectf(effectId, AL_EAXREVERB_ROOM_ROLLOFF_FACTOR, reverb.flRoomRolloffFactor);

	alEffecti(effectId, AL_EAXREVERB_DECAY_HFLIMIT, reverb.iDecayHFLimit);
}

// johnhalflife: the engine's software DSP presets (Steam Half-Life plays room types through these, not EAX), as
// Xash3D's s_dsp.c has them (rgsxpre). Its reverb: two delay lines fed back on themselves, size and 0.71 size long
// (at most 0.1 s), feedback refl, a two-tap lowpass in the loop if rvblp, summed in at about 0.029 each (/6, *11/64);
// its echo: one delay line (at most 0.4 s), feedback, a three-tap lowpass in the loop if dlylp, in at a quarter; then
// a left channel delay (left; not done here: it delays the dry sound, which an effect send can't). lp lowpasses the
// whole sound (water: the SDK's underwater filter already), mod modulates its loudness.
struct VanillaRoom
{
	float Lowpass, Modulate, Size, Reflectivity, ReverbLowpass, Delay, Feedback, DelayLowpass, Left;
};

constexpr VanillaRoom VanillaRooms[RoomEffectCount] = {
	{0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 2.0, 0.0},		  // 0 off
	{0.0, 0.0, 0.0, 0.0, 1.0, 0.065, 0.1, 0.0, 0.01},	  // 1 generic
	{0.0, 0.0, 0.0, 0.0, 1.0, 0.02, 0.75, 0.0, 0.01},	  // 2 metallic
	{0.0, 0.0, 0.0, 0.0, 1.0, 0.03, 0.78, 0.0, 0.02},	  // 3
	{0.0, 0.0, 0.0, 0.0, 1.0, 0.06, 0.77, 0.0, 0.03},	  // 4
	{0.0, 0.0, 0.05, 0.85, 1.0, 0.008, 0.96, 2.0, 0.01},  // 5 tunnel
	{0.0, 0.0, 0.05, 0.88, 1.0, 0.01, 0.98, 2.0, 0.02},	  // 6
	{0.0, 0.0, 0.05, 0.92, 1.0, 0.015, 0.995, 2.0, 0.04}, // 7
	{0.0, 0.0, 0.05, 0.84, 1.0, 0.0, 0.0, 2.0, 0.012},	  // 8 chamber
	{0.0, 0.0, 0.05, 0.9, 1.0, 0.0, 0.0, 2.0, 0.008},	  // 9
	{0.0, 0.0, 0.05, 0.95, 1.0, 0.0, 0.0, 2.0, 0.004},	  // 10
	{0.0, 0.0, 0.05, 0.7, 0.0, 0.0, 0.0, 2.0, 0.012},	  // 11 bright
	{0.0, 0.0, 0.055, 0.78, 0.0, 0.0, 0.0, 2.0, 0.008},	  // 12
	{0.0, 0.0, 0.05, 0.86, 0.0, 0.0, 0.0, 2.0, 0.002},	  // 13
	{1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 2.0, 0.01},		  // 14 water
	{1.0, 0.0, 0.0, 0.0, 1.0, 0.06, 0.85, 2.0, 0.02},	  // 15
	{1.0, 0.0, 0.0, 0.0, 1.0, 0.2, 0.6, 2.0, 0.05},		  // 16
	{0.0, 0.0, 0.05, 0.8, 1.0, 0.0, 0.48, 2.0, 0.016},	  // 17 concrete
	{0.0, 0.0, 0.06, 0.9, 1.0, 0.0, 0.52, 2.0, 0.01},	  // 18
	{0.0, 0.0, 0.07, 0.94, 1.0, 0.3, 0.6, 2.0, 0.008},	  // 19
	{0.0, 0.0, 0.0, 0.0, 1.0, 0.3, 0.42, 2.0, 0.0},		  // 20 outside
	{0.0, 0.0, 0.0, 0.0, 1.0, 0.35, 0.48, 2.0, 0.0},	  // 21
	{0.0, 0.0, 0.0, 0.0, 1.0, 0.38, 0.6, 2.0, 0.0},		  // 22
	{0.0, 0.0, 0.05, 0.9, 1.0, 0.2, 0.28, 0.0, 0.0},	  // 23 cavern
	{0.0, 0.0, 0.07, 0.9, 1.0, 0.3, 0.4, 0.0, 0.0},		  // 24
	{0.0, 0.0, 0.09, 0.9, 1.0, 0.35, 0.5, 0.0, 0.0},	  // 25
	{0.0, 1.0, 0.01, 0.9, 0.0, 0.0, 0.0, 2.0, 0.05},	  // 26 weirdo
	{0.0, 0.0, 0.0, 0.0, 1.0, 0.009, 0.999, 2.0, 0.04},	  // 27
	{0.0, 0.0, 0.001, 0.999, 0.0, 0.2, 0.8, 2.0, 0.05},	  // 28
};

// Levels, for tuning by ear (1: as worked out from the engine's mix). OpenAL's reverb isn't two delay lines, so how
// loud it comes out is the least certain.
constexpr float VanillaReverbLevel = 1;
constexpr float VanillaEchoLevel = 1;

// Each room's reverb and echo set to vanilla's, measured three times (the last with these): a click played in every room type in vanilla and here
// (jhl_roomtest.cfg, loopback recordings), each room's response against room 0's dry click, its energy by time after
// the click compared (dB; 0: matched as worked out). The reverb, two delay lines here an EAX reverb, came out 7-16 dB
// under vanilla's; the echoes matched in the metallic rooms (2-4) but outdoors (20-22) and in room 1 were well over it.
constexpr float VanillaReverbTrim[RoomEffectCount] = {
	0, 0, 0, 0, 0,		// 0-4: no reverb
	0, 0, 0,			// 5-7 tunnel: matched
	12, 10, 7,			// 8-10 chamber
	21, 18, 18,			// 11-13 bright
	0, 0, 0,			// 14-16 water: no reverb
	15, 9, 10,			// 17-19 concrete
	0, 0, 0,			// 20-22 outside: no reverb
	10, 11, 11,			// 23-25 cavern
	30, 0, 24,			// 26-28 weirdo (27: none; 28: its feedback, rounded as the engine's, had run away)
};

constexpr float VanillaEchoTrim[RoomEffectCount] = {
	0, -15, 0, 0, 0,	// 0-4: room 1's faint 65 ms echo barely there in vanilla
	0, 0, 0,			// 5-7 tunnel
	0, 0, 0,			// 8-10: no echo
	0, 0, 0,			// 11-13: no echo
	0, 0, -3,			// 14-16 water
	0, 0, -8,			// 17-19 concrete (19: as outdoors)
	-11, -11, -9,		// 20-22 outside
	0, 0, 0,			// 23-25 cavern
	0, -6, 0,			// 26-28
};

static float DecibelsToGain(float decibels)
{
	return std::pow(10.f, decibels / 20);
}

// The engine keeps a feedback as an integer, (int)(255 * value), and applies it shifted down 8 (out of 256): 0.999
// is 0.992, and a delay of 9 ms (room 27) dies away at about 8 dB a second, as measured, not ringing on.
static float EngineFeedback(float value)
{
	return std::floor(255 * std::clamp(value, 0.f, 1.f)) / 256;
}

// The longest echo delay OpenAL takes (AL_ECHO_MAX_DELAY): longer ones (outdoors, caverns: 0.3-0.38 s) are made this,
// their feedback lowered to die away as fast.
constexpr float MaxEchoDelay = 0.207f;

// A two-tap average's gain at 5 kHz (the effects' high frequency reference), at 44.1 kHz.
constexpr float TwoTapAverageHF = 0.937f;

static const VanillaRoom& GetVanillaRoom(unsigned int roomType)
{
	return VanillaRooms[roomType < RoomEffectCount ? roomType : 0];
}

void SetupVanillaReverb(ALuint effectId, unsigned int roomType)
{
	const auto& room = GetVanillaRoom(roomType);

	EFXEAXREVERBPROPERTIES reverb = EFX_REVERB_PRESET_GENERIC;

	const float size = std::min(room.Size, 0.1f);
	const float feedback = EngineFeedback(room.Reflectivity);

	if (size > 0 && feedback > 0)
	{
		// Each line's decay to -60 dB, 3 * length / -log10(feedback); the two averaged.
		const float decay = 3 * size * (1 + 0.71f) * 0.5f / -std::log10(feedback);
		reverb.flDecayTime = std::clamp(decay, 0.1f, 20.f);

		// The lowpass in the loop takes away highs a little each time round.
		reverb.flDecayHFRatio = room.ReverbLowpass != 0
									? std::clamp(std::log(feedback) / std::log(feedback * TwoTapAverageHF), 0.1f, 2.f)
									: 1.f;

		// The shorter line's first repeat, then the longer's.
		reverb.flReflectionsDelay = size * 0.71f;
		reverb.flLateReverbDelay = std::clamp(size * 0.29f, 0.f, 0.1f);

		// Two lines (their loudness, broadband: 1 / sqrt(1 - feedback^2) each) at about 0.029 each.
		const float level = 0.0286f * std::sqrt(2.f) / std::sqrt(1 - feedback * feedback) * VanillaReverbLevel *
							DecibelsToGain(VanillaReverbTrim[roomType < RoomEffectCount ? roomType : 0]);

		// Past what its overall gain takes (1), the rest on its late reverb (to 10).
		reverb.flGain = std::min(level, 1.f);
		reverb.flLateReverbGain = std::min(reverb.flLateReverbGain * std::max(level, 1.f), 10.f);

		// Two delay lines are sparse and grainy beside a full reverb.
		reverb.flDiffusion = 0.5f;
	}
	else
	{
		reverb.flGain = 0;
		reverb.flDecayTime = 0.1f;
	}

	if (room.Modulate != 0)
	{
		reverb.flModulationDepth = 1;
	}

	alEffecti(effectId, AL_EFFECT_TYPE, AL_EFFECT_EAXREVERB);

	alEffectf(effectId, AL_EAXREVERB_DENSITY, reverb.flDensity);
	alEffectf(effectId, AL_EAXREVERB_DIFFUSION, reverb.flDiffusion);
	alEffectf(effectId, AL_EAXREVERB_GAIN, reverb.flGain);
	alEffectf(effectId, AL_EAXREVERB_GAINHF, reverb.flGainHF);
	alEffectf(effectId, AL_EAXREVERB_GAINLF, reverb.flGainLF);
	alEffectf(effectId, AL_EAXREVERB_DECAY_TIME, reverb.flDecayTime);
	alEffectf(effectId, AL_EAXREVERB_DECAY_HFRATIO, reverb.flDecayHFRatio);
	alEffectf(effectId, AL_EAXREVERB_DECAY_LFRATIO, reverb.flDecayLFRatio);
	alEffectf(effectId, AL_EAXREVERB_REFLECTIONS_GAIN, reverb.flReflectionsGain);
	alEffectf(effectId, AL_EAXREVERB_REFLECTIONS_DELAY, reverb.flReflectionsDelay);
	alEffectfv(effectId, AL_EAXREVERB_REFLECTIONS_PAN, reverb.flReflectionsPan);
	alEffectf(effectId, AL_EAXREVERB_LATE_REVERB_GAIN, reverb.flLateReverbGain);
	alEffectf(effectId, AL_EAXREVERB_LATE_REVERB_DELAY, reverb.flLateReverbDelay);
	alEffectfv(effectId, AL_EAXREVERB_LATE_REVERB_PAN, reverb.flLateReverbPan);
	alEffectf(effectId, AL_EAXREVERB_ECHO_TIME, reverb.flEchoTime);
	alEffectf(effectId, AL_EAXREVERB_ECHO_DEPTH, reverb.flEchoDepth);
	alEffectf(effectId, AL_EAXREVERB_MODULATION_TIME, reverb.flModulationTime);
	alEffectf(effectId, AL_EAXREVERB_MODULATION_DEPTH, reverb.flModulationDepth);
	alEffectf(effectId, AL_EAXREVERB_AIR_ABSORPTION_GAINHF, reverb.flAirAbsorptionGainHF);
	alEffectf(effectId, AL_EAXREVERB_HFREFERENCE, reverb.flHFReference);
	alEffectf(effectId, AL_EAXREVERB_LFREFERENCE, reverb.flLFReference);
	alEffectf(effectId, AL_EAXREVERB_ROOM_ROLLOFF_FACTOR, reverb.flRoomRolloffFactor);
	alEffecti(effectId, AL_EAXREVERB_DECAY_HFLIMIT, reverb.iDecayHFLimit);
}

void SetupVanillaEcho(ALuint effectId, unsigned int roomType)
{
	const auto& room = GetVanillaRoom(roomType);

	float delay = room.Delay;
	float feedback = std::min(EngineFeedback(room.Feedback), AL_ECHO_MAX_FEEDBACK);

	if (delay > MaxEchoDelay)
	{
		feedback = std::pow(feedback, MaxEchoDelay / delay);
		delay = MaxEchoDelay;
	}

	alEffecti(effectId, AL_EFFECT_TYPE, AL_EFFECT_ECHO);

	// Both taps together: one echo, every delay, fed back from the second.
	alEffectf(effectId, AL_ECHO_DELAY, std::clamp(delay, AL_ECHO_MIN_DELAY, AL_ECHO_MAX_DELAY));
	alEffectf(effectId, AL_ECHO_LRDELAY, 0);
	alEffectf(effectId, AL_ECHO_FEEDBACK, std::clamp(feedback, 0.f, AL_ECHO_MAX_FEEDBACK));

	// A three-tap average's loss at 5 kHz, each time round.
	alEffectf(effectId, AL_ECHO_DAMPING, room.DelayLowpass != 0 ? 0.2f : 0.f);

	// The engine adds it to both channels alike.
	alEffectf(effectId, AL_ECHO_SPREAD, 0);
}

bool VanillaLowpass(unsigned int roomType)
{
	return GetVanillaRoom(roomType).Lowpass != 0;
}

float VanillaEchoGain(unsigned int roomType)
{
	// In at a quarter; its two taps, together, are twice one.
	return GetVanillaRoom(roomType).Delay > 0
			   ? 0.25f * 0.5f * VanillaEchoLevel * DecibelsToGain(VanillaEchoTrim[roomType < RoomEffectCount ? roomType : 0])
			   : 0.f;
}
}
