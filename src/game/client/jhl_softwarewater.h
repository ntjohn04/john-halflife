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

// johnhalflife (jhl_visuals_softwarewater): water as Half-Life's software renderer drew it, on the OpenGL renderer.
//
// The software renderer's water (sw.dll) is a ripple simulation: a 128 x 128 field of heights, shared by all water,
// stepped as a wave (each cell: half its four neighbours, less what it was, losing 1/64), a drop of random size landing
// at random every tenth of a second, a quarter of it on each neighbour; it steps once 0.05 s have passed since the last
// (every 4th frame at 72 fps, so 18 times a second). Each step, the water's texture is drawn again into a 128 x 128 tile,
// each texel taken from as far off as its cell's height / 16 (right and up), tiled over the surface a texel a unit. OpenGL
// instead bends a sine wave through the corners of the water's polygons.
//
// Here: the same simulation, its tiles drawn from the engine's own textures (so as gamma corrected as the rest), the
// water's faces (from the map, jhl_bsp) drawn over the engine's: the world's on top of its own, water entities' in their
// place (the engine is told not to draw them), as see-through as they were.

struct cl_entity_t;

void JhlSoftwareWater_Initialize();

/**
 *	@brief The fog (red, green, blue, density) of the water drawn here last, as the engine would take it from that
 *	water's palette; false if none was drawn lately.
 */
bool JhlSoftwareWater_LastFog(int (&fog)[4]);

/**
 *	@brief An entity being added this frame (HUD_AddEntity). True if the engine shouldn't draw it: its water is drawn here.
 */
bool JhlSoftwareWater_Add(cl_entity_t* ent);

/**
 *	@brief The opaque world and entities are drawn (HUD_DrawNormalTriangles): steps the ripples, draws the world's water
 *	and opaque water entities.
 */
void JhlSoftwareWater_DrawOpaque();

/**
 *	@brief See-through things are being drawn (HUD_DrawTransparentTriangles): draws see-through water entities.
 */
void JhlSoftwareWater_DrawTransparent();
