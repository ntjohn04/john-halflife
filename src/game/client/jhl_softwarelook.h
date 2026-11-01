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

// jhl_visuals_modeloverbright (default 1): both renderers light the world texture x light / 192, so the brightest
// light makes a surface 4/3 as bright as its texture; the software renderer lit models the same way, OpenGL never let a
// model's light go past its texture (measured on screenshots of the same view: software's models 1.33 times as bright,
// at every brightness, its world the same). While the engine draws a model's polygons, the second texture unit
// multiplies their lit colour by 4/3, clamped as software clamps.

void JhlSoftwareLook_Initialize();

/**
 *	@brief Around the engine drawing a studio model's polygons (StudioDrawPoints): gives them software's light, if on.
 *	True if it did; then call JhlSoftwareLook_EndModel after.
 */
bool JhlSoftwareLook_BeginModel(int renderMode, int renderFx);
void JhlSoftwareLook_EndModel();

/**
 *	@brief The frame is drawn, HUD and all (HUD_Redraw, after it).
 */
void JhlSoftwareLook_FrameDrawn();
