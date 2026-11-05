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
#include "utils/filesystem_utils.h"

/**
 *	@brief A sprite or model only the client's effects use, loaded by the client (CL_LoadModel) instead of precached by
 *	the server: the engine's 512 models a map are shared with the map's brush entities, and with johnhalflife's
 *	effects precached of4a4 came within 4 of them (op4ctf_crash went over). Asked for only in play, once the server's
 *	models are all in the client's list, so a client-loaded one never takes a slot of theirs. 0 if it can't be had
 *	(not installed, or the client's list full): the effect is left out.
 */
inline int JhlEffectModel(const char* name)
{
	int index = gEngfuncs.pEventAPI->EV_FindModelIndex(name);

	if (index == 0 && g_pFileSystem && g_pFileSystem->FileExists(name))
	{
		gEngfuncs.CL_LoadModel(name, &index);
	}

	return index;
}
