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

#include "hud.h"
#include "event_api.h"

/**
 *	@brief A sprite or model only johnhalflife's client effects use. The server precaches them last, only while the map
 *	has room (W_Precache: the engine's 512 models a map are shared with its brush entities), and the client can't load
 *	one the server didn't (CL_LoadModel gives -1). 0 where it isn't precached: the effect is left out.
 */
inline int JhlEffectModel(const char* name)
{
	return gEngfuncs.pEventAPI->EV_FindModelIndex(name);
}
