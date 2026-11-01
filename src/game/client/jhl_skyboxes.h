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

// johnhalflife (jhl_visuals_skyfiltering, default 1): the sky filtered as everything else is. The engine uploads the
// sky's six images (gfx/env/<sky><rt bk lf ft up dn>.tga) smooth, without mipmaps, and gl_texturemode only reaches
// textures with mipmaps (GL_Texturemode_f), so with texture filtering off the world is pixelated and the sky smooth;
// the software renderer point sampled the sky as it did all else. Here the engine's sky textures are found (the GL
// textures of the sky images' size whose picture is the image's) and given the world's filtering, put back if off.

void JhlSkyboxes_Initialize();

/**
 *	@brief The engine is drawing (HUD_DrawNormalTriangles): finds the sky textures when the sky changes, and gives them the
 *	world's filtering when it changes.
 */
void JhlSkyboxes_Update();
