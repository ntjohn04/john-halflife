/***
 *
 *	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
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

class BufferReader;

/**
 *	@brief A Xen spire ring's strand (UTIL_JhlBeamStrand's pattern on c4a1's and of3a1b's ring env_beams): a ring
 *	between the same two entities' midpoint, @p scale times as wide, kept on them every frame by JhlRings_Update.
 *	The SDK's own temp entity message's TE_BEAMRING.
 */
void JhlRings_ParseSpireRing(BufferReader& reader);

/**
 *	@brief Moves the strands onto their entities (HUD_TempEntUpdate, before the engine draws beams).
 */
void JhlRings_Update(double time);

/**
 *	@brief Forgets the strands: a new map frees the engine's beams (HUD_VidInit).
 */
void JhlRings_Reset();
