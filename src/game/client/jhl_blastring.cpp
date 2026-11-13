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

#include <algorithm>
#include <cmath>
#include <vector>

#include "hud.h"
#include "com_model.h"
#include "event_api.h"
#include "triangleapi.h"
#include "parsemsg.h"
#include "view.h"
#include "jhl_blastring.h"

namespace
{
constexpr int Points = 64;
constexpr float GrowTime = 0.265f;

float Curve(float p)
{
	return p;
}
constexpr std::size_t MaxRings = 4;

constexpr float Waver = 0.12f;
constexpr float LayerTurn = 0.35f;

struct Layer
{
	color24 Color;
	float WidthStart;
	float WidthEnd;
	float Brightness;
};

const Layer Layers[2] = {{{221, 255, 54}, 3, 12.8f, 0.235f}, {{240, 255, 170}, 1, 3.2f, 0.12f}};

struct Ring
{
	Vector Center;
	float Radius;
	float Start;
	float Phases[3];
};

std::vector<Ring> g_Rings;
float g_LastTime = 0;
} // namespace

void JhlBlastRing_Parse(BufferReader& reader)
{
	const Vector center = reader.ReadCoordVector();
	const float radius = reader.ReadShort();

	if (g_Rings.size() >= MaxRings)
		g_Rings.erase(g_Rings.begin());

	g_Rings.push_back({center, radius, gEngfuncs.GetClientTime(),
		{gEngfuncs.pfnRandomFloat(0, 6.2831853f), gEngfuncs.pfnRandomFloat(0, 6.2831853f), gEngfuncs.pfnRandomFloat(0, 6.2831853f)}});
}

void JhlBlastRing_Update(double time)
{
	const float now = static_cast<float>(time);

	// A new map restarts the clock.
	if (now < g_LastTime)
		g_Rings.clear();

	g_LastTime = now;

	std::erase_if(g_Rings, [&](const Ring& ring)
		{ return now - ring.Start >= GrowTime; });
}

void JhlBlastRing_Draw()
{
	if (g_Rings.empty())
		return;

	// The beams' texture: soft across its width.
	int index = 0;
	model_t* sprite = gEngfuncs.CL_LoadModel("sprites/laserbeam.spr", &index);

	if (!sprite)
		return;

	const float now = gEngfuncs.GetClientTime();

	Vector forward, right, up;
	AngleVectors(v_angles, forward, right, up);

	gEngfuncs.pTriAPI->SpriteTexture(sprite, 0);
	gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd);
	gEngfuncs.pTriAPI->CullFace(TRI_NONE);

	for (const Ring& ring : g_Rings)
	{
		const float age = now - ring.Start;
		const float p = std::clamp(age / GrowTime, 0.f, 1.f);
		const float c = Curve(p);
		const float radius = ring.Radius * c;
		const float fade = 1 - c;

		if (radius < 1)
			continue;

		const Vector center = ring.Center + (v_origin - ring.Center).Normalize() * 6;

		for (int l = 0; l < 2; ++l)
		{
			const Layer& layer = Layers[l];
			const float half = (layer.WidthStart + (layer.WidthEnd - layer.WidthStart) * c) / 2;
			const float turn = l * LayerTurn;

			gEngfuncs.pTriAPI->Color4f(layer.Color.r / 255.f, layer.Color.g / 255.f, layer.Color.b / 255.f, layer.Brightness * fade);
			gEngfuncs.pTriAPI->Begin(TRI_QUAD_STRIP);

			for (int i = 0; i <= Points; ++i)
			{
				const float angle = 6.2831853f * i / Points;
				const float a = angle + turn;
				const float waver = 1 + Waver * (0.45f * std::sin(2 * a + ring.Phases[0] + 5 * age) + 0.35f * std::sin(3 * a + ring.Phases[1] - 7 * age) + 0.2f * std::sin(5 * a + ring.Phases[2] + 9 * age));
				const float r = radius * waver;
				const Vector out = right * std::cos(angle) + up * std::sin(angle);
				const float along = static_cast<float>(i) / Points * 8;

				gEngfuncs.pTriAPI->TexCoord2f(0, along);
				gEngfuncs.pTriAPI->Vertex3fv(center + out * std::max(r - half, 0.f));
				gEngfuncs.pTriAPI->TexCoord2f(1, along);
				gEngfuncs.pTriAPI->Vertex3fv(center + out * (r + half));
			}

			gEngfuncs.pTriAPI->End();
		}
	}

	gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
	gEngfuncs.pTriAPI->CullFace(TRI_FRONT);
}

void JhlBlastRing_Reset()
{
	g_Rings.clear();
	g_LastTime = 0;
}
