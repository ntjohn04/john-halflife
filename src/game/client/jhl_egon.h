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

struct BEAM;
struct pmtrace_t;

/**
 *	@brief johnhalflife's gluon gun (jhl_fx_particlesystem), for the local player's beam (UpdateBeams, hl_objects.cpp):
 *	draws its two beams bent as if dragged through the air (their middle trailing where the hit point was a moment
 *	ago, by a quarter of a second; straight when still), scorches what it burns, and leaks sparks from the barrel.
 *	@param beam, beam2 The engine's beams (hidden while these are drawn).
 *	@param hit Where it hits this frame.
 */
void JhlEgon_Update(BEAM* beam, BEAM* beam2, const pmtrace_t& hit);

/**
 *	@brief The beam's let go (EV_EgonStop): its bent beams go, and a few blue specks fizzle off where it was hitting.
 */
void JhlEgon_Stop();

/**
 *	@brief Forgets the beams: a new map frees the engine's (HUD_VidInit).
 */
void JhlEgon_Reset();
