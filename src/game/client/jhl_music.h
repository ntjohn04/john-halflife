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

// johnhalflife: music saved with the game (jhl_musicstate.h). The server asks for what's playing as it saves
// (JhlMusic_GetState, exported), and sends what was saved each time the player's HUD starts (the JhlMusic message: a
// save loaded, a level changed, a new game). Only a save being loaded resets the connection, which stops the music
// (ClientLibrary::RunFrame), so only then is it played again, from where it was saved: a level change keeps playing.

void JhlMusic_Initialize();

/**
 *	@brief The connection was reset (a save loaded, a new game or map) and the music stopped: the next music the
 *	server sends is the music to restore.
 */
void JhlMusic_ConnectionReset();

/**
 *	@brief Each frame, after the connection's checked (ClientLibrary::RunFrame): plays what the server sent, if the
 *	connection was reset before it.
 */
void JhlMusic_RunFrame();
