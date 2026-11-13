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

#include <vector>

#include <AL/al.h>

namespace sound
{
// See https://usermanual.wiki/Pdf/Effects20Extension20Guide.90272296/view
// for more information on how to use the OpenAL EFX API.

constexpr int RoomEffectCount = 29;

void SetupEffect(ALuint effectId, unsigned int roomType);

// johnhalflife: @brief johnhalflife
void SetupVanillaReverb(ALuint effectId, unsigned int roomType);
void SetupVanillaEcho(ALuint effectId, unsigned int roomType);
float VanillaEchoGain(unsigned int roomType);

// johnhalflife: @brief johnhalflife
bool VanillaLowpass(unsigned int roomType);

// johnhalflife: @brief johnhalflife
constexpr int VanillaResponseRate = 22050;
std::vector<float> VanillaRoomResponse(unsigned int roomType);
}
