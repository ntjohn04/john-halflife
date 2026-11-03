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

class CBasePlayer;

// johnhalflife: music saved with the game, in the player's data (single player; jhl_musicstate.h).

/**
 *	@brief The player's about to be saved (DispatchSave): records the music playing now, asking the client's music
 *	system (in this process in single player). Left as it was if it can't be asked.
 */
void JhlMusic_BeforeSave(CBasePlayer* player);

/**
 *	@brief The player's HUD is starting (a save loaded, a level changed, a new game): sends the music as saved. The
 *	client plays it only if loading stopped its music (a level change keeps playing).
 */
void JhlMusic_SendToClient(CBasePlayer* player);
