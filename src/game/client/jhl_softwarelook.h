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

// jhl_visuals_renderfixes (default 1), model lighting: both renderers light the world texture x light / 192, so the brightest
// light makes a surface 4/3 as bright as its texture; the software renderer lit models the same way, OpenGL never let a
// model's light go past its texture (measured on screenshots of the same view: software's models 1.33 times as bright,
// at every brightness, its world the same). While the engine draws a model's polygons, the second texture unit
// multiplies their lit colour by 4/3, clamped as software clamps.

// jhl_visuals_renderfixes too, sampling: the menu's texture filtering off sets gl_texturemode GL_NEAREST, point
// sampled without mipmaps, so distant textures sparkle; software point sampled with mipmaps. With filtering off, the
// mode is made GL_NEAREST_MIPMAP_NEAREST (filtering turned on sets its own mode as ever; only this change is undone).

void JhlSoftwareLook_Initialize();

/**
 *	@brief Every client frame: the texture mode for software sampling.
 */
void JhlSoftwareLook_RunFrame();

/**
 *	@brief Around the engine drawing a studio model's polygons (StudioDrawPoints): gives them software's light, if on.
 *	True if it did; then call JhlSoftwareLook_EndModel after.
 */
bool JhlSoftwareLook_BeginModel(int renderMode, int renderFx);
void JhlSoftwareLook_EndModel();

// jhl_visuals_softwarewater (default 1), with the water: under water, the software renderer drew the view into its warp buffer,
// 320 x 200 (Quake's), and stretched it over the screen: big, not quite square pixels, the HUD sharp on top (measured on
// a screenshot: 4.5 x 5.4 pixels at 1440 x 1080). The view, once drawn, is drawn again from the middles of those blocks.

// jhl_visuals_softwarewater too: under water, software fogged its view as it stretched it: each pixel mixed
// toward the water's colour by a blend picked by its depth (64 of them, by 32768 / distance), never less than 32 of 256,
// colour and density from the water's palette as the engine's own fog takes them. OpenGL fogs linearly from the eye to
// 1536 - 4 x density instead. The engine's fog is kept off (see JhlSoftwareLook_SetupView) and software's put on, by
// the view's depth.

/**
 *	@brief The view is set up, not yet drawn (V_CalcRefdef, at its end): keeps the engine's fog off under water, for the
 *	underwater fog.
 */
void JhlSoftwareLook_SetupView();

/**
 *	@brief The engine is drawing the world (HUD_DrawNormalTriangles): its fog and projection, for the underwater fog.
 */
void JhlSoftwareLook_CaptureView();

/**
 *	@brief The view is drawn, not yet the HUD (HUD_Redraw, before it).
 */
void JhlSoftwareLook_ViewDrawn();

/**
 *	@brief The frame is drawn, HUD and all (HUD_Redraw, after it).
 */
void JhlSoftwareLook_FrameDrawn();
