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

// johnhalflife (jhl_visuals_flushfaces): brush entities built flush against the world, a face on the very plane of a
// world face it covers (signs, posters, panels, gears, rails: about 2,700 faces in Half-Life's maps). Nothing in the
// engine separates the two (gl_polyoffset is only used for decals): in OpenGL they fight for each pixel, in software
// the entity's face is lost in the wall. Found once a map, from its BSP; each frame such an entity is drawn moved out
// of the wall along the face's normal, by just more than the depth buffer can tell apart at that distance, the way
// gl_polyoffset lifts a decal. Put back once the view is drawn, so nothing else sees it moved.

struct cl_entity_t;

void JhlFlushFaces_Initialize();

/**
 *	@brief A brush entity being added this frame (HUD_AddEntity): moved out of the wall if it's flush with it.
 */
void JhlFlushFaces_Add(cl_entity_t* ent);

/**
 *	@brief The view is drawn (HUD_Redraw): puts the entities moved back where they are.
 */
void JhlFlushFaces_Restore();
