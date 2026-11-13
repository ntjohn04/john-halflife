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

// johnhalflife: fixes for bugs in the engine itself (hw.dll/hw.so, and sw for the last), patched in memory as the client
// starts; each only where the engine's code is exactly as expected, otherwise left alone (logged). Windows finds the
// places by their code; Linux by the engine's symbol table (kept in hw.so), then their code.
//
// A sprite drawn bright white and opaque after a brush entity in rendermode 1 (Color): the brush leaves the second
// texture unit on and selected, and R_DrawSpriteModel sets its texture mode before turning it off, so on the wrong
// unit. Fixed: its first call (R_GetSpriteFrame) goes through ours, which turns the unit off first (the engine's own
// GL_DisableMultitexture, nothing if it's off).
//
// "AllocBlock: full" on a map change late in a long session: the engine keeps every model loaded this session in a
// table of 1024; full, it gives a new model the slot of an unused one (an old map's brush models first), copying in only
// its name. A sprite loaded there keeps the old map's surface count and surfaces, and the lightmap builder
// (GL_BuildLightmaps), taking every model in the map's list with a surface count, walks the freed surfaces: garbage
// sizes, the 64 lightmap blocks overrun. Fixed: it takes those of brush type instead (type 0; sprites 1, studio
// models 3), real brush models as before.
//
// A hang on a map change late in a long session: the same reused slot kept the old model's claim on a block of the
// model cache, so the engine later freed the wrong block and Hunk_AllocName could move the old one forever. Fixed:
// Mod_FindName's copy of the name goes through ours, which frees the old block (if still the slot's) and clears the
// slot first.

void JhlEngineFixes_Initialize();

/**
 *	@brief Whether the engine draws the world's water itself (R_DrawWorld), for see-through water drawn instead
 *	(jhl_visuals_wateropacity). False if it can't be left out (not found, or not on Windows yet).
 */
bool JhlEngineFixes_DrawWorldWater(bool drawn);
