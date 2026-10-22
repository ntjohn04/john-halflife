/***
 *
 *	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
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

// HUD drawing in HUD units: positions and sizes are multiplied by the HUD scale (see cl_util.h).
// At scale 1 everything goes straight to the engine, exactly as before.

#include "hud.h"
#include "triangleapi.h"
#include "com_model.h"

namespace
{
// The engine doesn't report the sprite and color set by SPR_Set, but scaled drawing needs both.
HSPRITE g_CurrentSprite = 0;
RGB24 g_CurrentColor{};

bool IsScaled()
{
	return gHUD.m_flHudScale != 1;
}

int ToRealPixels(int hudUnits)
{
	return static_cast<int>(std::lround(hudUnits * gHUD.m_flHudScale));
}

int ToHudUnits(int realPixels)
{
	return static_cast<int>(std::ceil(realPixels / gHUD.m_flHudScale));
}

void DrawScaledSprite(int frame, int x, int y, const Rect* prc, int renderMode)
{
	if (0 == g_CurrentSprite)
	{
		return;
	}

	const int frameWidth = gEngfuncs.pfnSPR_Width(g_CurrentSprite, frame);
	const int frameHeight = gEngfuncs.pfnSPR_Height(g_CurrentSprite, frame);

	if (frameWidth <= 0 || frameHeight <= 0)
	{
		return;
	}

	Rect rect = prc ? *prc : Rect{0, frameWidth, 0, frameHeight};

	rect.left = std::clamp(rect.left, 0, frameWidth);
	rect.right = std::clamp(rect.right, rect.left, frameWidth);
	rect.top = std::clamp(rect.top, 0, frameHeight);
	rect.bottom = std::clamp(rect.bottom, rect.top, frameHeight);

	if (rect.left == rect.right || rect.top == rect.bottom)
	{
		return;
	}

	// Sample half a texel in from the edges so filtering doesn't bleed in neighboring sprites on the sheet.
	const float left = (rect.left + 0.5f) / frameWidth;
	const float right = (rect.right - 0.5f) / frameWidth;
	const float top = (rect.top + 0.5f) / frameHeight;
	const float bottom = (rect.bottom - 0.5f) / frameHeight;

	// Compute both edges in real pixels so neighboring sprites meet without gaps.
	const float x1 = static_cast<float>(ToRealPixels(x));
	const float y1 = static_cast<float>(ToRealPixels(y));
	const float x2 = static_cast<float>(ToRealPixels(x + rect.right - rect.left));
	const float y2 = static_cast<float>(ToRealPixels(y + rect.bottom - rect.top));

	auto TriAPI = gEngfuncs.pTriAPI;

	TriAPI->SpriteTexture(const_cast<model_t*>(gEngfuncs.GetSpritePointer(g_CurrentSprite)), frame);
	TriAPI->RenderMode(renderMode);
	TriAPI->Color4f(g_CurrentColor.Red / 255.0f, g_CurrentColor.Green / 255.0f, g_CurrentColor.Blue / 255.0f, 1.0f);

	TriAPI->Begin(TRI_QUADS);

	TriAPI->TexCoord2f(left, top);
	TriAPI->Vertex3f(x1, y1, 0);

	TriAPI->TexCoord2f(right, top);
	TriAPI->Vertex3f(x2, y1, 0);

	TriAPI->TexCoord2f(right, bottom);
	TriAPI->Vertex3f(x2, y2, 0);

	TriAPI->TexCoord2f(left, bottom);
	TriAPI->Vertex3f(x1, y2, 0);

	TriAPI->End();

	TriAPI->RenderMode(kRenderNormal);
}
}

void SPR_Set(HSPRITE hPic, const RGB24& color)
{
	g_CurrentSprite = hPic;
	g_CurrentColor = color;
	gEngfuncs.pfnSPR_Set(hPic, color.Red, color.Green, color.Blue);
}

void SPR_Draw(int frame, int x, int y, const Rect* prc)
{
	if (!IsScaled())
	{
		gEngfuncs.pfnSPR_Draw(frame, x, y, prc);
		return;
	}

	DrawScaledSprite(frame, x, y, prc, kRenderNormal);
}

void SPR_DrawHoles(int frame, int x, int y, const Rect* prc)
{
	if (!IsScaled())
	{
		gEngfuncs.pfnSPR_DrawHoles(frame, x, y, prc);
		return;
	}

	DrawScaledSprite(frame, x, y, prc, kRenderTransAlpha);
}

void SPR_DrawAdditive(int frame, int x, int y, const Rect* prc)
{
	if (!IsScaled())
	{
		gEngfuncs.pfnSPR_DrawAdditive(frame, x, y, prc);
		return;
	}

	DrawScaledSprite(frame, x, y, prc, kRenderTransAdd);
}

void FillRGBA(int x, int y, int width, int height, const RGB24& color, int a)
{
	const int realX = ToRealPixels(x);
	const int realY = ToRealPixels(y);

	gEngfuncs.pfnFillRGBA(realX, realY, ToRealPixels(x + width) - realX, ToRealPixels(y + height) - realY,
		color.Red, color.Green, color.Blue, a);
}

int TextMessageDrawChar(int x, int y, int number, int r, int g, int b)
{
	return ToHudUnits(gEngfuncs.pfnDrawCharacter(ToRealPixels(x), ToRealPixels(y), number, r, g, b));
}

int DrawConsoleString(int x, int y, const char* string)
{
	// Returns the x position after the string.
	return ToHudUnits(gEngfuncs.pfnDrawConsoleString(ToRealPixels(x), ToRealPixels(y), string));
}

void GetConsoleStringSize(const char* string, int* width, int* height)
{
	gEngfuncs.pfnDrawConsoleStringLen(string, width, height);

	*width = ToHudUnits(*width);
	*height = ToHudUnits(*height);
}
