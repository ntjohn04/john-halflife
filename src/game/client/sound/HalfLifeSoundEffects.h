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

#include <AL/al.h>

namespace sound
{
// See https://usermanual.wiki/Pdf/Effects20Extension20Guide.90272296/view
// for more information on how to use the OpenAL EFX API.

constexpr int RoomEffectCount = 29;

void SetupEffect(ALuint effectId, unsigned int roomType);

/**
 *	@brief johnhalflife: a room's reverb as the engine's own software DSP has it (Steam Half-Life's; the table above is
 *	the long gone EAX one's), and its echo (an echo effect, on a second send), with that echo's level.
 */
void SetupVanillaReverb(ALuint effectId, unsigned int roomType);
void SetupVanillaEcho(ALuint effectId, unsigned int roomType);
float VanillaEchoGain(unsigned int roomType);

/**
 *	@brief johnhalflife: whether the engine lowpasses the whole sound in a room (its water rooms, 14-16: room 14 is
 *	the one under water), the direct sound too.
 */
bool VanillaLowpass(unsigned int roomType);
}
