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

#include <cstdio>

#include "hud.h"
#include "parsemsg.h"
#include "jhl_music.h"
#include "jhl_musicstate.h"
#include "networking/ClientUserMessages.h"
#include "sound/IMusicSystem.h"
#include "sound/ISoundSystem.h"

namespace
{
cvar_t* g_MusicSaves = nullptr;
bool g_ConnectionReset = false;
bool g_Received = false;
sound::MusicState g_ReceivedState;

void MsgFunc_JhlMusic(const char* pszName, BufferReader& reader)
{
	sound::MusicState state;
	state.FileName = reader.ReadString();
	const int flags = reader.ReadByte();
	state.Looping = (flags & 1) != 0;
	state.Paused = (flags & 2) != 0;
	state.Position = reader.ReadFloat();
	state.FadeRemaining = reader.ReadFloat();
	state.FadeDuration = reader.ReadFloat();

	g_ReceivedState = std::move(state);
	g_Received = true;
}
}

void JhlMusic_Initialize()
{
	g_MusicSaves = gEngfuncs.pfnRegisterVariable("jhl_snd_musicsaves", "1", FCVAR_ARCHIVE);
	g_ClientUserMessages.RegisterHandler("JhlMusic", &MsgFunc_JhlMusic);
}

void JhlMusic_ConnectionReset()
{
	g_ConnectionReset = true;
}

void JhlMusic_RunFrame()
{
	if (!g_Received)
	{
		return;
	}

	g_Received = false;

	if (!g_ConnectionReset)
	{
		return;
	}

	g_ConnectionReset = false;

	if (!g_ReceivedState.FileName.empty() && 0 != g_MusicSaves->value)
	{
		sound::g_SoundSystem->GetMusicSystem()->PlayFrom(g_ReceivedState);
	}
}

extern "C" DLLEXPORT bool JhlMusic_GetState(JhlMusicState* state)
{
	if (!state || !sound::g_SoundSystem)
	{
		return false;
	}

	const sound::MusicState music = sound::g_SoundSystem->GetMusicSystem()->GetState();

	*state = {};

	// A name too long to save is saved as nothing playing.
	if (music.FileName.size() < sizeof(state->FileName))
	{
		std::snprintf(state->FileName, sizeof(state->FileName), "%s", music.FileName.c_str());
		state->Looping = music.Looping;
		state->Paused = music.Paused;
		state->Position = music.Position;
		state->FadeRemaining = music.FadeRemaining;
		state->FadeDuration = music.FadeDuration;
	}

	return true;
}
