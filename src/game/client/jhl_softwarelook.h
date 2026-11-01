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

// johnhalflife: touches of Half-Life's software renderer on the OpenGL one, each its own setting.
//
// jhl_visuals_16bitcolor (default 0): the software renderer drew into a 16 bit frame, 5 bits of red, 6 of green, 5 of
// blue, each colour cut down (not rounded, not dithered): smooth gradients step, darks lose their last shades. Measured
// on screenshots of the same view: software's red and blue are multiples of 8, green of 4, from 0. Each frame, once the
// HUD is drawn, the client copies it and draws it back cut down the same way.

void JhlSoftwareLook_Initialize();

/**
 *	@brief The frame is drawn, HUD and all (HUD_Redraw, after it).
 */
void JhlSoftwareLook_FrameDrawn();
