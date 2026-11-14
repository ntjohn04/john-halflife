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

#include "cbase.h"
#include "UserMessages.h"
#include "jhl_music.h"
#include "jhl_musicstate.h"
#include "utils/filesystem_utils.h"

#ifdef WIN32
#include "PlatformHeaders.h"
#else
#include <dlfcn.h>
#endif

namespace
{
JhlMusicGetStateFn FindGetState()
{
#ifdef WIN32
	HMODULE client = GetModuleHandleA("client.dll");
	return client ? reinterpret_cast<JhlMusicGetStateFn>(GetProcAddress(client, JhlMusicGetStateName)) : nullptr;
#else
	const std::string path = FileSystem_GetGameDirectory() + "/" + FileSystem_GetModDirectoryName() + "/cl_dlls/client.so";
	void* client = dlopen(path.c_str(), RTLD_NOW | RTLD_NOLOAD);

	if (!client)
	{
		return nullptr;
	}

	auto getState = reinterpret_cast<JhlMusicGetStateFn>(dlsym(client, JhlMusicGetStateName));
	dlclose(client);
	return getState;
#endif
}
}

void JhlMusic_BeforeSave(CBasePlayer* player)
{
	if (!player || gpGlobals->maxClients != 1)
	{
		return;
	}

	const JhlMusicGetStateFn getState = FindGetState();
	JhlMusicState state{};

	if (!getState || !getState(&state))
	{
		return;
	}

	player->m_JhlMusicFile = state.FileName[0] != '\0' ? ALLOC_STRING(state.FileName) : string_t::Null;
	player->m_JhlMusicLooping = state.Looping;
	player->m_JhlMusicPaused = state.Paused;
	player->m_JhlMusicPosition = state.Position;
	player->m_JhlMusicFadeRemaining = state.FadeRemaining;
	player->m_JhlMusicFadeDuration = state.FadeDuration;
}

void JhlMusic_SendToClient(CBasePlayer* player)
{
	if (!player || gpGlobals->maxClients != 1)
	{
		return;
	}

	MESSAGE_BEGIN(MSG_ONE, gmsgJhlMusic, nullptr, player);
	WRITE_STRING(STRING(player->m_JhlMusicFile));
	WRITE_BYTE((player->m_JhlMusicLooping ? 1 : 0) | (player->m_JhlMusicPaused ? 2 : 0));
	WRITE_FLOAT(player->m_JhlMusicPosition);
	WRITE_FLOAT(player->m_JhlMusicFadeRemaining);
	WRITE_FLOAT(player->m_JhlMusicFadeDuration);
	MESSAGE_END();
}
