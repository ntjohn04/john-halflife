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

// johnhalflife's blast ring (jhl_fx_particlesystem): a faint ring facing the view, the outline of a blast's sphere,
// growing out to its reach as it fades, so how far it reaches can be read (the spore launcher's burst, CSpore).

class BufferReader;

/**
 *	@brief A blast ring sent by the server (the TempEntity message's TE_BEAMDISK: its centre, and its radius).
 */
void JhlBlastRing_Parse(BufferReader& reader);

/**
 *	@brief Forgets the rings that are done (HUD_TempEntUpdate).
 */
void JhlBlastRing_Update(double time);

/**
 *	@brief Draws the rings (HUD_DrawTransparentTriangles).
 */
void JhlBlastRing_Draw();

/**
 *	@brief Forgets them (HUD_VidInit).
 */
void JhlBlastRing_Reset();
