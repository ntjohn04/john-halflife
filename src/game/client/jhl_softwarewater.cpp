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
#include <array>
#include <cmath>
#include <numbers>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "hud.h"
#include "com_model.h"
#include "triangleapi.h"
#include "view.h"
#include "jhl_bsp.h"
#include "jhl_gl.h"
#include "jhl_softwarewater.h"

namespace
{
// The ripple field: 128 x 128 heights (16 bit, wrapping as the software renderer's do).
constexpr int FieldSize = 128;
constexpr int FieldMask = FieldSize - 1;
constexpr int FieldCells = FieldSize * FieldSize;

// It steps once 0.05 s have passed since the last: every 4th frame at 72 fps (jhl_framerate.h), 18 times a second; a
// drop lands once more than 0.1 s have passed since the last, every other step.
constexpr float StepTime = 4.0f / 72.0f;
constexpr float DropTime = 0.1f;
constexpr int MostStepsAFrame = 4; // After a pause or hitch, catch up no further.

// Texels a unit, and the tile's size (s and t, in texels, over this).
constexpr float TileSize = FieldSize;

// Pulled toward the view a little where drawn over the engine's water (its polygons are cut differently, so the same
// plane isn't quite the same depth), as decals are.
constexpr float OverdrawOffsetFactor = -1;
constexpr float OverdrawOffsetUnits = -2;

// Render modes (const.h's kRender*) its entities are drawn in here.
constexpr int RenderNormal = 0;
constexpr int RenderTransTexture = 2;
constexpr int RenderTransAdd = 5;

// jhl_visuals_waterlight: the map has no light on water (its compiler gives water no lightmap; both renderers draw it
// full bright), so a pitch-black corner's water stood out of the dark. Here it's as bright as ever wherever the light
// under it (the engine's LightAtPoint: the lightmap below, the floor under the water, its styles as they are) is at
// least LitLight, darkening smoothly below that to DarkestWater with none. Sampled on a grid (LightGrid units, the
// faces cut up for it), each point the average of the light around it LightSpread apart (smoother), leaving out any in
// a wall (water brushes run into the walls; light taken in one is black), a point with all of them in one taking the
// light found stepping in toward its face's middle (LightStep, at most LightSteps). Taken once a map, no more than
// LightBudget points a frame (a face not yet taken drawn as bright as ever), again where a water entity has moved (as
// slowly, its old light kept meanwhile); lights
// switched later aren't followed. Light from the flashlight and flashes isn't taken: dark water stays dark under them.
constexpr float LitLight = 32;
constexpr float DarkestWater = 0.25f;
constexpr float LightSpread = 24;
constexpr float LightGrid = 32;
constexpr float LightInset = 4; // A face's edge sampled this far in (its corner touches walls).
constexpr float LightStep = 16;
constexpr int LightSteps = 8;
constexpr int LightBudget = 300;
int g_LightBudget = 0; // Points still to be taken this frame.

JhlGL& gl = g_JhlGL;
cvar_t* g_SoftwareWater = nullptr;
cvar_t* g_WaterSides = nullptr;
cvar_t* g_WaterLight = nullptr;

using Field = std::array<std::int16_t, FieldCells>;
Field g_Fields[2]{};
int g_Newest = 0; // The field last stepped to, the one drawn; the other is the one before it.
float g_LastStep = -1;
float g_LastDrop = 0;
bool g_Stepped = false;

// A water texture: the engine's, read back, and the tile drawn from it.
struct WaterTexture
{
	std::string Name;
	int Width = 0;
	int Height = 0;
	std::vector<std::uint32_t> Source; // RGBA, as the engine uploaded it.
	GLuint Tile = 0;
	bool Mipmapped = false;
	int Fog[4]{}; // Its fog under water: colour and density, from its palette (see EngineTexture).
	bool HasFog = false;
};

// A model's water faces, by texture.
struct WaterModel
{
	std::vector<std::vector<int>> FacesByTexture; // Index: g_Textures; values: face indices in the map.
	bool HasWater = false;
	bool AllWater = false; // Every face drawn is water: it can be drawn here instead of by the engine.
};

// An entity whose water is drawn here this frame.
struct WaterEntity
{
	int Model = 0;
	Vector Origin;
	Vector Angles;
	int RenderMode = RenderNormal;
	float Alpha = 1;
	bool Hidden = false; // The engine isn't drawing it.
	float Distance = 0;
	float Bottom = 0; // Its lowest point as the engine reckons it (see DrawModel).
};

// A water face cut up on the light's grid, and the light under each of its points.
struct LitFace
{
	std::vector<Vector> Points;		  // Every piece's, one after another.
	std::vector<Vector> SamplePoints; // Each point moved in from the face's edge (LightInset), where light is taken.
	std::vector<int> Counts;		  // Each piece's number of points.
	std::vector<float> Light;		  // Each point's brightness, 0 to 1 (Brightness).
	Vector Middle;					  // The face's.
	std::size_t Next = 0;			  // The next point to take (a face taken over several frames).
	Vector SampledOrigin;
	Vector SampledAngles;
	float SampledTime = -1;
};

std::string g_MapName;
bool g_MapReady = false;
std::vector<LitFace> g_LitFaces; // By face in the map; only water faces filled.
std::vector<WaterTexture> g_Textures;
std::vector<WaterModel> g_Models;
std::vector<WaterEntity> g_Entities; // This frame's, until the see-through ones are drawn.

// The fog of the water drawn last, and when (the engine takes its fog from the water it drew last).
int g_LastFog[4]{};
float g_LastFogTime = -1;

// The engine's rule for water (Mod_LoadTextures): names starting with '!', "laser" or "water". Sky and scrolling
// textures aren't.
bool IsWaterName(const std::string& name)
{
	if (name.empty() || 0 == std::strncmp(name.c_str(), "sky", 3) || 0 == std::strncmp(name.c_str(), "scroll", 6))
	{
		return false;
	}

	return name[0] == '!' || 0 == strnicmp(name.c_str(), "laser", 5) || 0 == strnicmp(name.c_str(), "water", 5);
}

bool IsPowerOfTwo(int value)
{
	return value > 0 && (value & (value - 1)) == 0;
}

void FreeTextures()
{
	for (auto& texture : g_Textures)
	{
		if (0 != texture.Tile)
		{
			gl.DeleteTextures(1, &texture.Tile);
		}
	}

	g_Textures.clear();
}

// The engine's GL texture for a map texture, from the world model's texture list: texture_t as the OpenGL renderer has
// it, its GL texture number after its name and size (R_RenderBrushPoly binds texture + 0x18). Only if the list is the
// map's, name for name.
int EngineTexture(const model_t* world, const JhlBsp& bsp, int miptex, std::string& why, WaterTexture& water)
{
	if (!world || world->numtextures != static_cast<int>(bsp.TextureNames.size()) || !world->textures ||
		miptex < 0 || miptex >= world->numtextures)
	{
		why = "the engine's texture list isn't the map's (" + std::to_string(world ? world->numtextures : -1) + " textures, the map " +
			  std::to_string(bsp.TextureNames.size()) + ")";
		return 0;
	}

	const texture_t* texture = world->textures[miptex];

	if (!texture || 0 != strnicmp(texture->name, bsp.TextureNames[miptex].c_str(), sizeof(texture->name)))
	{
		why = std::string("the engine has another texture there (") + (texture ? texture->name : "none") + ")";
		return 0;
	}

	int number = 0;
	std::memcpy(&number, reinterpret_cast<const std::byte*>(texture) + 0x18, sizeof(number));

	// Its fog under water, as both renderers take it when they draw it (EmitWaterPolys, and sw.dll's turbulence):
	// the palette (texture + 0x44) entry 3's colour, and entry 4's red for its density.
	const std::uint8_t* palette = nullptr;
	std::memcpy(&palette, reinterpret_cast<const std::byte*>(texture) + 0x44, sizeof(palette));

	if (palette)
	{
		water.Fog[0] = palette[9];
		water.Fog[1] = palette[10];
		water.Fog[2] = palette[11];
		water.Fog[3] = palette[12];
		water.HasFog = true;
	}

	return number;
}

// Reads back a texture the engine uploaded, and makes its tile with the same filtering.
bool LoadTexture(WaterTexture& water, int engineTexture, std::string& why)
{
	if (!why.empty())
	{
		return false;
	}

	if (engineTexture <= 0 || GL_TRUE != gl.IsTexture(engineTexture))
	{
		why = "no GL texture " + std::to_string(engineTexture);
		return false;
	}

	GLint previous = 0;
	gl.GetIntegerv(GL_TEXTURE_BINDING_2D, &previous);
	gl.BindTexture(GL_TEXTURE_2D, engineTexture);

	GLint width = 0, height = 0, minFilter = GL_LINEAR, magFilter = GL_LINEAR;
	gl.GetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
	gl.GetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
	gl.GetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &minFilter);
	gl.GetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &magFilter);

	bool ok = IsPowerOfTwo(width) && IsPowerOfTwo(height) && width <= 1024 && height <= 1024;

	if (!ok)
	{
		why = "its size isn't a power of two (" + std::to_string(width) + " x " + std::to_string(height) + ")";
	}

	if (ok)
	{
		water.Width = width;
		water.Height = height;
		water.Source.resize(static_cast<std::size_t>(width) * height);
		gl.GetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, water.Source.data());

		water.Mipmapped = minFilter != GL_NEAREST && minFilter != GL_LINEAR;
		gl.GenTextures(1, &water.Tile);
		gl.BindTexture(GL_TEXTURE_2D, water.Tile);
		gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, FieldSize, FieldSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
		gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);
		gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	}

	gl.BindTexture(GL_TEXTURE_2D, previous);
	return ok;
}

// A convex polygon cut on the light's grid (as Quake's GL_SubdivideSurface cuts water: across its middle, at a grid
// line, while it's longer than a cell along an axis; no sliver under 8 units).
void CutOnGrid(const std::vector<Vector>& polygon, std::vector<std::vector<Vector>>& pieces, int depth = 0)
{
	Vector mins = polygon[0];
	Vector maxs = polygon[0];

	for (const auto& point : polygon)
	{
		for (int axis = 0; axis < 3; ++axis)
		{
			mins[axis] = std::min(mins[axis], point[axis]);
			maxs[axis] = std::max(maxs[axis], point[axis]);
		}
	}

	for (int axis = 0; axis < 3 && depth < 64; ++axis)
	{
		const float cut = LightGrid * std::floor((mins[axis] + maxs[axis]) * 0.5f / LightGrid + 0.5f);

		if (maxs[axis] - cut < 8 || cut - mins[axis] < 8)
		{
			continue;
		}

		std::vector<Vector> front;
		std::vector<Vector> back;

		for (std::size_t i = 0; i < polygon.size(); ++i)
		{
			const Vector& a = polygon[i];
			const Vector& b = polygon[(i + 1) % polygon.size()];
			const float da = a[axis] - cut;
			const float db = b[axis] - cut;

			if (da >= 0)
				front.push_back(a);
			if (da <= 0)
				back.push_back(a);

			if ((da > 0 && db < 0) || (da < 0 && db > 0))
			{
				const Vector crossing = a + (b - a) * (da / (da - db));
				front.push_back(crossing);
				back.push_back(crossing);
			}
		}

		if (front.size() >= 3 && back.size() >= 3)
		{
			CutOnGrid(front, pieces, depth + 1);
			CutOnGrid(back, pieces, depth + 1);
			return;
		}
	}

	pieces.push_back(polygon);
}

void LightFace(const JhlBspFace& face, LitFace& lit)
{
	std::vector<std::vector<Vector>> pieces;
	CutOnGrid(face.Points, pieces);

	Vector middle = g_vecZero;

	for (const auto& point : face.Points)
		middle = middle + point;

	middle = middle * (1.0f / face.Points.size());
	lit.Middle = middle;

	for (const auto& piece : pieces)
	{
		lit.Counts.push_back(static_cast<int>(piece.size()));

		for (const auto& point : piece)
		{
			lit.Points.push_back(point);

			const Vector in = middle - point;
			const float length = in.Length();
			lit.SamplePoints.push_back(length > LightInset ? point + in * (LightInset / length) : middle);
		}
	}

	lit.Light.assign(lit.Points.size(), 1.0f);
}

// The map's water: its textures (read from the engine's) and every model's water faces.
void LoadMap(const char* mapName)
{
	FreeTextures();
	g_Models.clear();
	g_LitFaces.clear();
	g_MapReady = false;

	const JhlBsp* bsp = JhlBsp_Get(mapName);
	const cl_entity_t* worldEntity = gEngfuncs.GetEntityByIndex(0);

	if (!bsp || !worldEntity || !worldEntity->model)
	{
		return;
	}

	std::vector<int> textureByMiptex(bsp->TextureNames.size(), -1);

	for (std::size_t miptex = 0; miptex < bsp->TextureNames.size(); ++miptex)
	{
		if (!IsWaterName(bsp->TextureNames[miptex]))
		{
			continue;
		}

		WaterTexture water;
		water.Name = bsp->TextureNames[miptex];
		std::string why;

		if (LoadTexture(water, EngineTexture(worldEntity->model, *bsp, static_cast<int>(miptex), why, water), why))
		{
			textureByMiptex[miptex] = static_cast<int>(g_Textures.size());
			g_Textures.push_back(std::move(water));
		}
		else
		{
			gEngfuncs.Con_DPrintf("johnhalflife: water texture %s left to the engine: %s\n", water.Name.c_str(), why.c_str());
		}
	}

	g_Models.resize(bsp->Models.size());
	g_LitFaces.resize(bsp->Faces.size());

	for (std::size_t m = 0; m < bsp->Models.size(); ++m)
	{
		auto& model = g_Models[m];
		model.FacesByTexture.resize(g_Textures.size());
		model.AllWater = true;

		for (int i = 0; i < bsp->Models[m].NumFaces; ++i)
		{
			const int index = bsp->Models[m].FirstFace + i;
			const auto& face = bsp->Faces[index];
			const int miptex = face.Texinfo >= 0 ? bsp->Texinfo[face.Texinfo].Miptex : -1;
			const int texture = miptex >= 0 && static_cast<std::size_t>(miptex) < textureByMiptex.size() ? textureByMiptex[miptex] : -1;

			if (face.Points.empty())
			{
				continue;
			}

			if (texture < 0)
			{
				model.AllWater = false;
				continue;
			}

			model.FacesByTexture[texture].push_back(index);
			model.HasWater = true;
			LightFace(face, g_LitFaces[index]);
		}

		model.AllWater = model.AllWater && model.HasWater;
	}

	g_MapReady = true;
	g_Stepped = true; // Tiles drawn before they're first used.
}

bool Ready()
{
	if (0 == g_SoftwareWater->value || !JhlGL_Available())
	{
		return false;
	}

	const char* mapName = gEngfuncs.pfnGetLevelName();

	if (!mapName || '\0' == *mapName)
	{
		return false;
	}

	if (g_MapName != mapName)
	{
		g_MapName = mapName;
		LoadMap(mapName);
	}

	return g_MapReady && !g_Textures.empty();
}

// One step of the ripples (sw.dll's, as it is): a drop now and then on the newer field, then the older one becomes the
// next.
void Step(float time)
{
	auto& from = g_Fields[g_Newest];
	auto& to = g_Fields[1 - g_Newest];

	if (time - g_LastDrop > DropTime)
	{
		g_LastDrop = time;

		const int size = gEngfuncs.pfnRandomLong(0, 0x3ff);
		const int row = gEngfuncs.pfnRandomLong(0, 0x7fff) & FieldMask;
		const int column = gEngfuncs.pfnRandomLong(0, 0x7fff) & FieldMask;
		const auto add = [&](int r, int c, int amount)
		{
			auto& cell = from[(r & FieldMask) * FieldSize + (c & FieldMask)];
			cell = static_cast<std::int16_t>(cell + amount);
		};

		add(row, column, size);
		add(row, column + 1, size >> 2);
		add(row, column - 1, size >> 2);
		add(row + 1, column, size >> 2);
		add(row - 1, column, size >> 2);
	}

	// Neighbours along the field as a row of cells, wrapping (so a row's ends touch the next and last rows' ends).
	for (int i = 0; i < FieldCells; ++i)
	{
		const int sum = from[(i - FieldSize) & (FieldCells - 1)] + from[(i - 1) & (FieldCells - 1)] +
						from[(i + 1) & (FieldCells - 1)] + from[(i + FieldSize) & (FieldCells - 1)];
		const int value = (sum >> 1) - to[i];
		to[i] = static_cast<std::int16_t>(value - (value >> 6));
	}

	g_Newest = 1 - g_Newest;
	g_Stepped = true;
}

void Advance()
{
	const float time = gEngfuncs.GetClientTime();

	// A new map or time going back: start again.
	if (g_LastStep < 0 || time < g_LastStep)
	{
		g_LastStep = time;
		g_LastDrop = time - DropTime;
		return;
	}

	for (int steps = 0; time - g_LastStep >= StepTime && steps < MostStepsAFrame; ++steps)
	{
		g_LastStep += StepTime;
		Step(g_LastStep);
	}

	if (time - g_LastStep >= StepTime)
	{
		g_LastStep = time;
	}
}

// Each tile drawn again from its texture, displaced by the newest field (sw.dll's: right by height / 16, up by it).
void UpdateTiles()
{
	if (!g_Stepped)
	{
		return;
	}

	g_Stepped = false;

	const auto& field = g_Fields[g_Newest];
	std::vector<std::uint32_t> tile(FieldCells);

	GLint previous = 0;
	gl.GetIntegerv(GL_TEXTURE_BINDING_2D, &previous);

	for (auto& water : g_Textures)
	{
		const int widthBits = water.Width - 1;
		const int heightBits = water.Height - 1;
		int shift = 0;

		while ((1 << shift) < water.Width)
		{
			++shift;
		}

		for (int i = 0; i < FieldCells; ++i)
		{
			const int offset = field[i] >> 4;
			const int x = (offset + i) & widthBits;
			const int y = ((i >> 7) - offset) & heightBits;
			tile[i] = water.Source[(y << shift) + x];
		}

		gl.BindTexture(GL_TEXTURE_2D, water.Tile);
		gl.TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, FieldSize, FieldSize, GL_RGBA, GL_UNSIGNED_BYTE, tile.data());

		if (water.Mipmapped)
		{
			gl.GenerateMipmap(GL_TEXTURE_2D);
		}
	}

	gl.BindTexture(GL_TEXTURE_2D, previous);
}

// A point turned about an axis (0 x, 1 y, 2 z) by degrees, as glRotatef turns it.
Vector Turn(const Vector& point, int axis, float degrees)
{
	const float radians = degrees * static_cast<float>(std::numbers::pi / 180);
	const float c = std::cos(radians);
	const float s = std::sin(radians);
	const int a = (axis + 1) % 3;
	const int b = (axis + 2) % 3;
	Vector turned = point;
	turned[a] = point[a] * c - point[b] * s;
	turned[b] = point[a] * s + point[b] * c;
	return turned;
}

// Draws a model's water faces, placed as the engine places a brush model (R_RotateForEntity: moved to its origin,
// turned by yaw about z, pitch (negated) about y, roll about x). The world's: those that face the view. An entity's: as
// the engine's R_DrawBrushModel (hw.dll) has them, only its level ones (any, gl_watersides 1) higher than a unit above
// its bottom (the entity's origin and its model's lowest point; turned, its origin less its radius), from either side:
// its top, seen from above and from under it, not its sides and floor.
// The light under a point (the brightest of its colours), averaged with the light around it (LightSpread), any in a
// wall left out; -1 if all are.
float LightUnder(const Vector& point)
{
	static constexpr float Taps[][2] = {{0, 0}, {LightSpread, 0}, {-LightSpread, 0}, {0, LightSpread}, {0, -LightSpread}};
	float total = 0;
	int count = 0;

	for (const auto& tap : Taps)
	{
		Vector at = point + Vector(tap[0], tap[1], 0);

		if (gEngfuncs.PM_PointContents(at, nullptr) == CONTENTS_SOLID)
		{
			continue;
		}

		Vector light;
		gEngfuncs.pTriAPI->LightAtPoint(at, light);
		total += std::max({light.x, light.y, light.z});
		++count;
	}

	return count > 0 ? total / count : -1;
}

// The water's brightness for the light under it (jhl_visuals_waterlight): 1 from LitLight up, eased down below it.
float Brightness(float light)
{
	const float x = std::clamp(light / LitLight, 0.0f, 1.0f);
	return DarkestWater + (1 - DarkestWater) * x * x * (3 - 2 * x);
}

// A face's light, taken the first time it's drawn (budget allowing) and again if its entity has moved (see LitLight).
void Relight(LitFace& lit, const Vector& origin, const Vector& angles)
{
	const bool taken = lit.SampledTime >= 0;
	const bool moved = taken && ((lit.SampledOrigin - origin).Length() > 0.5f || (lit.SampledAngles - angles).Length() > 0.5f);

	if ((taken && !moved) || g_LightBudget <= 0)
	{
		return;
	}

	// Moved: taken again within the budget as any face is, its old light kept meanwhile (a little behind while it moves).
	if (moved)
	{
		lit.SampledTime = -1;
		lit.Next = 0;
	}

	// Placed as the engine places its model (DrawModel), a unit above the water.
	const auto place = [&](Vector point)
	{
		if (angles != g_vecZero)
		{
			point = Turn(Turn(Turn(point, 0, angles[2]), 1, -angles[0]), 2, angles[1]);
		}

		return point + origin + Vector(0, 0, 1);
	};

	for (; lit.Next < lit.SamplePoints.size() && g_LightBudget > 0; ++lit.Next, --g_LightBudget)
	{
		const std::size_t i = lit.Next;
		float light = LightUnder(place(lit.SamplePoints[i]));

		// All in a wall: stepping in toward the face's middle (none found: as bright as ever).
		const Vector in = lit.Middle - lit.SamplePoints[i];
		const float length = in.Length();

		for (int step = 1; light < 0 && step <= LightSteps && step * LightStep < length; ++step)
		{
			light = LightUnder(place(lit.SamplePoints[i] + in * (step * LightStep / length)));
		}

		lit.Light[i] = Brightness(light < 0 ? LitLight : light);
	}

	if (lit.Next < lit.SamplePoints.size())
	{
		return;
	}

	lit.SampledTime = gEngfuncs.GetClientTime();
	lit.SampledOrigin = origin;
	lit.SampledAngles = angles;
}

void DrawModel(const JhlBsp& bsp, int modelIndex, const Vector& origin, const Vector& angles, float bottom = 0, const std::array<float, 4>& color = {1, 1, 1, 1})
{
	const auto& model = g_Models[modelIndex];
	const bool turned = angles != g_vecZero;
	const bool lit = g_WaterLight && g_WaterLight->value != 0;

	// The view in the model's own coordinates, to tell which faces face it.
	Vector view = v_origin - origin;

	if (turned)
	{
		view = Turn(Turn(Turn(view, 2, -angles[1]), 1, angles[0]), 0, -angles[2]);
	}

	gl.PushMatrix();
	gl.Translatef(origin.x, origin.y, origin.z);

	if (turned)
	{
		gl.Rotatef(angles[1], 0, 0, 1);
		gl.Rotatef(-angles[0], 0, 1, 0);
		gl.Rotatef(angles[2], 1, 0, 0);
	}

	for (std::size_t t = 0; t < model.FacesByTexture.size(); ++t)
	{
		if (model.FacesByTexture[t].empty())
		{
			continue;
		}

		gl.BindTexture(GL_TEXTURE_2D, g_Textures[t].Tile);

		for (const int index : model.FacesByTexture[t])
		{
			const auto& face = bsp.Faces[index];

			if (modelIndex == 0)
			{
				if (DotProduct(face.Normal, view) - face.Dist <= 0)
				{
					continue;
				}
			}
			else
			{
				// The plane as the map keeps it: a level one points up.
				const bool level = face.Normal.x == 0 && face.Normal.y == 0;
				const float height = face.Normal.z < 0 ? -face.Dist : face.Dist;

				if ((!level && (!g_WaterSides || g_WaterSides->value == 0)) || bottom + 1 >= height)
				{
					continue;
				}
			}

			if (g_Textures[t].HasFog)
			{
				std::copy(std::begin(g_Textures[t].Fog), std::end(g_Textures[t].Fog), g_LastFog);
				g_LastFogTime = gEngfuncs.GetClientTime();
			}

			const auto& texinfo = bsp.Texinfo[face.Texinfo];
			auto& litFace = g_LitFaces[index];

			if (lit)
			{
				Relight(litFace, origin, angles);
			}

			std::size_t next = 0;

			for (const int count : litFace.Counts)
			{
				gl.Begin(GL_POLYGON);

				for (int i = 0; i < count; ++i, ++next)
				{
					const Vector& point = litFace.Points[next];
					const float light = lit ? litFace.Light[next] : 1.0f;
					const float s = DotProduct(point, Vector(texinfo.Vecs[0][0], texinfo.Vecs[0][1], texinfo.Vecs[0][2])) + texinfo.Vecs[0][3];
					const float t2 = DotProduct(point, Vector(texinfo.Vecs[1][0], texinfo.Vecs[1][1], texinfo.Vecs[1][2])) + texinfo.Vecs[1][3];
					gl.Color4f(color[0] * light, color[1] * light, color[2] * light, color[3]);
					gl.TexCoord2f(s / TileSize, t2 / TileSize);
					gl.Vertex3fv(point);
				}

				gl.End();
			}
		}
	}

	gl.PopMatrix();
}

// What all the drawing here needs, kept apart from the engine's state.
struct WaterDrawState
{
	GLint Program = 0;
	GLint Texture = 0;
	GLint ActiveTexture = 0;

	WaterDrawState()
	{
		gl.GetIntegerv(GL_CURRENT_PROGRAM, &Program);
		gl.GetIntegerv(GL_ACTIVE_TEXTURE, &ActiveTexture);
		gl.PushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_POLYGON_BIT | GL_TEXTURE_BIT | GL_CURRENT_BIT | GL_TRANSFORM_BIT);
		gl.MatrixMode(GL_MODELVIEW);

		gl.UseProgram(0);

		// Only the first texture unit (the engine's lightmaps use the second).
		gl.ActiveTexture(GL_TEXTURE1);
		gl.Disable(GL_TEXTURE_2D);
		gl.ActiveTexture(GL_TEXTURE0);
		gl.GetIntegerv(GL_TEXTURE_BINDING_2D, &Texture);
		gl.Enable(GL_TEXTURE_2D);
		gl.TexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

		gl.Disable(GL_CULL_FACE); // Faces facing away are left out above.

		// Fogged under water as the engine fogs the world (it turns its fog off around the client's drawing).
		if (const int contents = gEngfuncs.PM_PointContents(v_origin, nullptr); contents <= CONTENTS_WATER && contents != CONTENTS_TRANSLUCENT)
		{
			gl.Enable(GL_FOG);
		}
		gl.Disable(GL_ALPHA_TEST);
		gl.Enable(GL_DEPTH_TEST);
		gl.DepthFunc(GL_LEQUAL);
	}

	~WaterDrawState()
	{
		gl.BindTexture(GL_TEXTURE_2D, Texture);
		gl.PopAttrib();
		gl.ActiveTexture(ActiveTexture);
		gl.UseProgram(Program);
	}
};
}

bool JhlSoftwareWater_LastFog(int (&fog)[4])
{
	// Drawn this frame or the last few.
	const float time = gEngfuncs.GetClientTime();

	if (0 == g_SoftwareWater->value || g_LastFogTime < 0 || time < g_LastFogTime || time - g_LastFogTime > 0.5f)
	{
		return false;
	}

	std::copy(std::begin(g_LastFog), std::end(g_LastFog), fog);
	return true;
}

void JhlSoftwareWater_Initialize()
{
	g_SoftwareWater = gEngfuncs.pfnRegisterVariable("jhl_visuals_softwarewater", "1", FCVAR_ARCHIVE);
	g_WaterSides = gEngfuncs.pfnGetCvarPointer("gl_watersides");
	g_WaterLight = gEngfuncs.pfnRegisterVariable("jhl_visuals_waterlight", "1", FCVAR_ARCHIVE);

	// The light under the player's eyes and the water's brightness for it there, for tuning the curve (LitLight).
	gEngfuncs.pfnAddCommand("jhl_debug_waterlight", []
		{
			const float light = LightUnder(v_origin);
			gEngfuncs.Con_Printf("jhl_debug_waterlight: light %.0f under the view (averaged around it; -1: all in walls), water %.2f (full from %.0f, %.2f at the darkest)\n",
				light, Brightness(light < 0 ? LitLight : light), LitLight, DarkestWater);
		});
}

bool JhlSoftwareWater_Add(cl_entity_t* ent)
{
	if (!ent->model || ent->model->type != mod_brush || ent->model->name[0] != '*' || !Ready())
	{
		return false;
	}

	const int modelIndex = std::atoi(ent->model->name + 1);

	if (modelIndex <= 0 || static_cast<std::size_t>(modelIndex) >= g_Models.size() || !g_Models[modelIndex].HasWater)
	{
		return false;
	}


	const auto& model = g_Models[modelIndex];
	const int renderMode = ent->curstate.rendermode;
	const bool knownMode = renderMode == RenderNormal || renderMode == RenderTransTexture || renderMode == RenderTransAdd;

	WaterEntity water;
	water.Model = modelIndex;
	water.Origin = ent->origin;
	water.Angles = ent->angles;
	water.RenderMode = renderMode;
	water.Alpha = renderMode == RenderNormal ? 1.0f : std::clamp(ent->curstate.renderamt / 255.0f, 0.0f, 1.0f);
	water.Distance = (ent->origin + (ent->model->mins + ent->model->maxs) * 0.5f - v_origin).Length();
	water.Bottom = ent->angles != g_vecZero ? ent->origin.z - ent->model->radius : ent->origin.z + ent->model->mins.z;

	if (model.AllWater && knownMode)
	{
		// All water: drawn here, not by the engine.
		water.Hidden = true;
	}
	else if (renderMode != RenderNormal)
	{
		// See-through and partly water: left to the engine.
		return false;
	}

	g_Entities.push_back(water);
	return water.Hidden;
}

void JhlSoftwareWater_DrawOpaque()
{
	if (!Ready())
	{
		return;
	}

	const JhlBsp* bsp = JhlBsp_Get(g_MapName.c_str());

	if (!bsp)
	{
		return;
	}

	Advance();
	UpdateTiles();
	g_LightBudget = LightBudget;

	WaterDrawState state;
	gl.Color4f(1, 1, 1, 1);
	gl.Disable(GL_BLEND);

	// Over the engine's: the world's water, and opaque entities it draws with water on them.
	gl.DepthMask(GL_FALSE);
	gl.Enable(GL_POLYGON_OFFSET_FILL);
	gl.PolygonOffset(OverdrawOffsetFactor, OverdrawOffsetUnits);
	DrawModel(*bsp, 0, g_vecZero, g_vecZero);

	for (const auto& water : g_Entities)
	{
		if (!water.Hidden && water.RenderMode == RenderNormal)
		{
			DrawModel(*bsp, water.Model, water.Origin, water.Angles, water.Bottom);
		}
	}

	// In place of the engine's: opaque water entities.
	gl.Disable(GL_POLYGON_OFFSET_FILL);
	gl.DepthMask(GL_TRUE);

	for (const auto& water : g_Entities)
	{
		if (water.Hidden && water.RenderMode == RenderNormal)
		{
			DrawModel(*bsp, water.Model, water.Origin, water.Angles, water.Bottom);
		}
	}
}

void JhlSoftwareWater_DrawTransparent()
{
	// Whatever happens, this frame's entities are done with.
	struct ClearEntities
	{
		~ClearEntities() { g_Entities.clear(); }
	} clearEntities;

	if (!Ready())
	{
		return;
	}

	const JhlBsp* bsp = JhlBsp_Get(g_MapName.c_str());

	if (!bsp)
	{
		return;
	}

	// See-through water entities, far to near, as the engine draws them.
	std::vector<const WaterEntity*> seeThrough;

	for (const auto& water : g_Entities)
	{
		if (water.Hidden && water.RenderMode != RenderNormal)
		{
			seeThrough.push_back(&water);
		}
	}

	if (seeThrough.empty())
	{
		return;
	}

	std::sort(seeThrough.begin(), seeThrough.end(), [](const WaterEntity* a, const WaterEntity* b)
		{ return a->Distance > b->Distance; });

	WaterDrawState state;
	gl.DepthMask(GL_FALSE);
	gl.Enable(GL_BLEND);

	for (const auto* water : seeThrough)
	{
		std::array<float, 4> color{1, 1, 1, water->Alpha};

		if (water->RenderMode == RenderTransAdd)
		{
			gl.BlendFunc(GL_ONE, GL_ONE);
			color = {water->Alpha, water->Alpha, water->Alpha, 1};
		}
		else
		{
			gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}

		DrawModel(*bsp, water->Model, water->Origin, water->Angles, water->Bottom, color);
	}
}
