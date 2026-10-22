/***
 *
 *	Copyright (c) 1999, Valve LLC. All rights reserved.
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
//
// cl_util.h
//

#pragma once

#include <string>

#include "cvardef.h"

#include "Platform.h"
#include "palette.h"
#include "utils/ConCommandSystem.h"
#include "utils/shared_utils.h"

/**
 *	@brief Helper function to register client side commands that were originally registered with @c HOOK_COMMAND.
 */
template <typename T>
void RegisterClientCommand(std::string_view name, void (T::*handler)(), T* instance)
{
	g_ConCommands.CreateCommand(
		name, [instance, handler](const auto&)
		{ (instance->*handler)(); },
		CommandLibraryPrefix::No);
}

inline cvar_t* CVAR_CREATE(const char* cv, const char* val, const int flags) { return gEngfuncs.pfnRegisterVariable(cv, val, flags); }

inline HSPRITE SPR_Load(const char* spriteName)
{
	return gEngfuncs.pfnSPR_Load(spriteName);
}

inline HSPRITE SPR_Load(const std::string& spriteName)
{
	return gEngfuncs.pfnSPR_Load(spriteName.c_str());
}

// HUD scale (jhl_hud_scale): the sprite HUD is laid out on a virtual screen of HudWidth x HudHeight
// (the real size divided by the scale) and the drawing functions below scale it back up to real pixels.
// Engine text can't be scaled, so text keeps its size and is only positioned in HUD units.
// VGUI and other code that works in real pixels keeps using ScreenWidth/ScreenHeight.

void SPR_Set(HSPRITE hPic, const RGB24& color);

#define SPR_Frames (*gEngfuncs.pfnSPR_Frames)

// SPR_Draw  draws a the current sprite as solid
void SPR_Draw(int frame, int x, int y, const Rect* prc);
// SPR_DrawHoles  draws the current sprites,  with color index255 not drawn (transparent)
void SPR_DrawHoles(int frame, int x, int y, const Rect* prc);
// SPR_DrawAdditive  adds the sprites RGB values to the background  (additive transulency)
void SPR_DrawAdditive(int frame, int x, int y, const Rect* prc);

// SPR_EnableScissor  sets a clipping rect for HUD sprites.  (0,0) is the top-left hand corner of the screen.
#define SPR_EnableScissor (*gEngfuncs.pfnSPR_EnableScissor)
// SPR_DisableScissor  disables the clipping rect
#define SPR_DisableScissor (*gEngfuncs.pfnSPR_DisableScissor)
//
void FillRGBA(int x, int y, int width, int height, const RGB24& color, int a);

// ScreenHeight returns the height of the screen, in pixels
#define ScreenHeight (gHUD.m_scrinfo.iHeight)
// ScreenWidth returns the width of the screen, in pixels
#define ScreenWidth (gHUD.m_scrinfo.iWidth)

// Size of the virtual screen the sprite HUD is laid out on (see HUD scale above)
#define HudHeight (gHUD.m_HudScreenInfo.iHeight)
#define HudWidth (gHUD.m_HudScreenInfo.iWidth)

#define BASE_XRES 640.f

// use this to project world coordinates to screen coordinates
#define XPROJECT(x) ((1.0f + (x)) * ScreenWidth * 0.5f)
#define YPROJECT(y) ((1.0f - (y)) * ScreenHeight * 0.5f)
#define HUD_XPROJECT(x) ((1.0f + (x)) * HudWidth * 0.5f)
#define HUD_YPROJECT(y) ((1.0f - (y)) * HudHeight * 0.5f)

#define XRES(x) ((x) * ((float)ScreenWidth / 640))
#define YRES(y) ((y) * ((float)ScreenHeight / 480))

#define GetScreenInfo (*gEngfuncs.pfnGetScreenInfo)
#define ServerCmd (*gEngfuncs.pfnServerCmd)
#define EngineClientCmd (*gEngfuncs.pfnClientCmd)


// Gets the height & width of a sprite,  at the specified frame
inline int SPR_Height(HSPRITE x, int f) { return gEngfuncs.pfnSPR_Height(x, f); }
inline int SPR_Width(HSPRITE x, int f) { return gEngfuncs.pfnSPR_Width(x, f); }

inline client_textmessage_t* TextMessageGet(const char* pName) { return gEngfuncs.pfnTextMessageGet(pName); }
// Text drawing in HUD units (see HUD scale above).
int TextMessageDrawChar(int x, int y, int number, int r, int g, int b);
int DrawConsoleString(int x, int y, const char* string);
void GetConsoleStringSize(const char* string, int* width, int* height);

inline int ConsoleStringLen(const char* string)
{
	int _width, _height;
	GetConsoleStringSize(string, &_width, &_height);
	return _width;
}

inline void ConsolePrint(const char* string)
{
	gEngfuncs.pfnConsolePrint(string);
}

inline void CenterPrint(const char* string)
{
	gEngfuncs.pfnCenterPrint(string);
}


inline char* safe_strcpy(char* dst, const char* src, int len_dst)
{
	if (len_dst <= 0)
	{
		return nullptr; // this is bad
	}

	strncpy(dst, src, len_dst);
	dst[len_dst - 1] = '\0';

	return dst;
}

inline int safe_sprintf(char* dst, int len_dst, const char* format, ...)
{
	if (len_dst <= 0)
	{
		return -1; // this is bad
	}

	va_list v;

	va_start(v, format);

	vsnprintf(dst, len_dst, format, v);

	va_end(v);

	dst[len_dst - 1] = '\0';

	return 0;
}

inline bool UTIL_IsMapLoaded()
{
	const auto levelName = gEngfuncs.pfnGetLevelName();

	return levelName != nullptr && levelName[0] != '\0';
}
