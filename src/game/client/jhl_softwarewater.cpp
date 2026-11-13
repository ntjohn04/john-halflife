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
#include <chrono>
#include <cmath>
#include <numbers>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include "hud.h"
#include "com_model.h"
#include "pm_defs.h"
#include "pmtrace.h"
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
// full bright), so a pitch-black corner's water stood out of the dark. Here it takes the light of what's around it, as
// seen from just above it (LightAbove): the walls level with it (LightRays traces out to LightReach) and the floor
// under it, each where its trace hits it as the map's lightmaps have it there (the map's own light, its first style),
// the nearer counting more (LightFalloff); with none hit, the engine's LightAtPoint (the lightmap below, averaged
// LightSpread around, leaving out any in a wall, further out if all are: LightRings). As bright as ever from LitLight
// up, darkening smoothly below that to DarkestWater; under the open sky (straight up, SkyReach), always as bright. On a grid (LightGrid units, the faces cut up for it; EngineGrid over the engine's waved water), by where a
// point is alone, so faces in the same place (a water entity's brushes' tops and bottoms) are lit alike, not fighting.
// Taken once a map, no more than LightTime a frame (a face not yet taken drawn as bright as ever), again where
// a water entity has moved (as slowly, its old light kept meanwhile); lights switched later aren't followed, nor the
// flashlight and flashes.
constexpr float LitLight = 96;
constexpr float DarkestWater = 0.2f;
constexpr float SkyReach = 8192;
constexpr float LightAbove = 12;
constexpr int LightRays = 8;
constexpr float LightReach = 192;
constexpr float LightDepth = 512; // The floor's furthest under it.
constexpr float LightFalloff = 48;
constexpr float LightSpread = 24;
constexpr float LightGrid = 32;
// Where it's drawn over the engine's water as the engine waves it, on the engine's own cut instead (hw.dll's
// GL_SubdivideSurface, the same rule at 64 units): its pieces vertex for vertex, so its waves exactly.
constexpr float EngineGrid = 64;
constexpr float LightRings[] = {48, 80};
constexpr std::chrono::microseconds LightTime{1500}; // Its share of a frame: hitch-free with the engine's slowest.
constexpr float SurfaceCell = 128; // The world's lit faces kept by cells this size (IndexSurfaces).
std::chrono::steady_clock::duration g_LightSpent{}; // Taking light this frame so far.

JhlGL& gl = g_JhlGL;
cvar_t* g_SoftwareWater = nullptr;
cvar_t* g_WaterSides = nullptr;
cvar_t* g_WaterLight = nullptr;

// jhl_visuals_wateropacity (default 0.96): below 1, solid water entities (func_water...) drawn here
// see-through at that opacity, in the see-through pass; water already see-through as it was. Not the world's water:
// the map's visibility treats it as a wall, so what's under it isn't drawn from above (c2a5's dam: holes). Neither
// 1998 renderer showed through water.
cvar_t* g_WaterOpacity = nullptr;
bool g_SeeThrough = false; // This frame's water drawn see-through.

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
	float Waves = 0;  // Its waves' height (curstate.scale: the map's WaveHeight / 8), as OpenGL draws them (WaveHeight).
};

// A water face cut up on the light's grid, and the light under each of its points.
struct LitFace
{
	std::vector<Vector> Points;		  // Every piece's, one after another.
	std::vector<int> Counts;		  // Each piece's number of points.
	std::vector<float> Light;		  // Each point's brightness, 0 to 1 (Brightness).
	std::size_t Next = 0;			  // The next point to take (a face taken over several frames).
	bool HasTwin = false;			  // Its model has the same surface facing the other way (a water brush's top, seen from under it).
	Vector SampledOrigin;
	Vector SampledAngles;
	float SampledTime = -1;
};

std::string g_MapName;
bool g_MapReady = false;
std::vector<LitFace> g_LitFaces;	 // By face in the map; only water faces filled.
std::vector<LitFace> g_EngineFaces; // The same on the engine's cut (EngineGrid), lit likewise.

// A world face with a lightmap of the map's own light (its first style 0): where its samples start, in 16 texels, and
// how many across and down.
struct LitSurface
{
	int Face = 0;
	int MinS = 0;
	int MinT = 0;
	int Width = 0;
	int Height = 0;
};

std::vector<LitSurface> g_Surfaces;
std::unordered_map<std::int64_t, std::vector<int>> g_SurfaceCells; // By cell (SurfaceCell), into g_Surfaces.
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
void CutOnGrid(const std::vector<Vector>& polygon, std::vector<std::vector<Vector>>& pieces, float grid, int depth = 0)
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
		const float cut = grid * std::floor((mins[axis] + maxs[axis]) * 0.5f / grid + 0.5f);

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
			CutOnGrid(front, pieces, grid, depth + 1);
			CutOnGrid(back, pieces, grid, depth + 1);
			return;
		}
	}

	pieces.push_back(polygon);
}

void LightFace(const JhlBspFace& face, LitFace& lit, float grid)
{
	std::vector<std::vector<Vector>> pieces;
	CutOnGrid(face.Points, pieces, grid);

	for (const auto& piece : pieces)
	{
		lit.Counts.push_back(static_cast<int>(piece.size()));

		for (const auto& point : piece)
		{
			lit.Points.push_back(point);
		}
	}

	lit.Light.assign(lit.Points.size(), 1.0f);
}

void IndexSurfaces(const JhlBsp& bsp);

// The map's water: its textures (read from the engine's) and every model's water faces.
void LoadMap(const char* mapName)
{
	FreeTextures();
	g_Models.clear();
	g_LitFaces.clear();
	g_EngineFaces.clear();
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
	g_EngineFaces.resize(bsp->Faces.size());
	IndexSurfaces(*bsp);

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
			LightFace(face, g_LitFaces[index], LightGrid);
			LightFace(face, g_EngineFaces[index], EngineGrid);
		}

		model.AllWater = model.AllWater && model.HasWater;

		// Each surface's twin facing the other way, if it has one: of the two only the one facing the view is drawn (both
		// drawn, flat they were the same; waved, cut into triangles differently, they fought).
		for (const auto& faces : model.FacesByTexture)
		{
			for (const int a : faces)
			{
				for (const int b : faces)
				{
					const auto& one = bsp->Faces[a];
					const auto& other = bsp->Faces[b];

					if (a != b && DotProduct(one.Normal, other.Normal) < -0.999f && std::abs(one.Dist + other.Dist) < 0.1f)
					{
						g_LitFaces[a].HasTwin = true;
						break;
					}
				}
			}
		}
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

// The light at a point in the world (LightUnder), all in a wall: further out around it, the nearest ring with any;
// none: as bright as ever.
float LightAt(const Vector& point)
{
	float light = LightUnder(point);

	for (const float radius : LightRings)
	{
		if (light >= 0)
		{
			break;
		}

		float total = 0;
		int count = 0;

		for (int i = 0; i < 8; ++i)
		{
			const float angle = i * static_cast<float>(std::numbers::pi / 4);
			const Vector at = point + Vector(std::cos(angle) * radius, std::sin(angle) * radius, 0);

			if (gEngfuncs.PM_PointContents(const_cast<float*>(&at.x), nullptr) == CONTENTS_SOLID)
			{
				continue;
			}

			Vector tap;
			gEngfuncs.pTriAPI->LightAtPoint(const_cast<float*>(&at.x), tap);
			total += std::max({tap.x, tap.y, tap.z});
			++count;
		}

		light = count > 0 ? total / count : -1;
	}

	return light < 0 ? LitLight : light;
}

// The water's brightness for the light under it (jhl_visuals_waterlight): 1 from LitLight up, eased down below it.
float Brightness(float light)
{
	const float x = std::clamp(light / LitLight, 0.0f, 1.0f);
	return DarkestWater + (1 - DarkestWater) * x * x * (3 - 2 * x);
}

std::int64_t CellKey(int x, int y, int z)
{
	return ((static_cast<std::int64_t>(x) & 0x1FFFFF) << 42) | ((static_cast<std::int64_t>(y) & 0x1FFFFF) << 21) | (static_cast<std::int64_t>(z) & 0x1FFFFF);
}

int CellOf(float value)
{
	return static_cast<int>(std::floor(value / SurfaceCell));
}

// The world's faces with the map's own light, by the cells they're in.
void IndexSurfaces(const JhlBsp& bsp)
{
	g_Surfaces.clear();
	g_SurfaceCells.clear();

	if (bsp.Models.empty())
	{
		return;
	}

	for (int i = 0; i < bsp.Models[0].NumFaces; ++i)
	{
		const int index = bsp.Models[0].FirstFace + i;
		const auto& face = bsp.Faces[index];

		if (face.Points.empty() || face.LightOffset < 0 || face.Styles[0] != 0 || face.Texinfo < 0)
		{
			continue;
		}

		const auto& texinfo = bsp.Texinfo[face.Texinfo];
		float mins[2]{1e9f, 1e9f};
		float maxs[2]{-1e9f, -1e9f};
		Vector low = face.Points[0];
		Vector high = face.Points[0];

		for (const auto& point : face.Points)
		{
			for (int axis = 0; axis < 2; ++axis)
			{
				const float value = DotProduct(point, Vector(texinfo.Vecs[axis][0], texinfo.Vecs[axis][1], texinfo.Vecs[axis][2])) + texinfo.Vecs[axis][3];
				mins[axis] = std::min(mins[axis], value);
				maxs[axis] = std::max(maxs[axis], value);
			}

			for (int axis = 0; axis < 3; ++axis)
			{
				low[axis] = std::min(low[axis], point[axis]);
				high[axis] = std::max(high[axis], point[axis]);
			}
		}

		LitSurface surface;
		surface.Face = index;
		surface.MinS = static_cast<int>(std::floor(mins[0] / 16));
		surface.MinT = static_cast<int>(std::floor(mins[1] / 16));
		surface.Width = static_cast<int>(std::ceil(maxs[0] / 16)) - surface.MinS + 1;
		surface.Height = static_cast<int>(std::ceil(maxs[1] / 16)) - surface.MinT + 1;

		if (surface.Width <= 0 || surface.Height <= 0 ||
			static_cast<std::size_t>(face.LightOffset) + static_cast<std::size_t>(surface.Width) * surface.Height * 3 > bsp.Lighting.size())
		{
			continue;
		}

		const int number = static_cast<int>(g_Surfaces.size());
		g_Surfaces.push_back(surface);

		// In every cell its bounds touch, a unit around.
		for (int x = CellOf(low.x - 1); x <= CellOf(high.x + 1); ++x)
		{
			for (int y = CellOf(low.y - 1); y <= CellOf(high.y + 1); ++y)
			{
				for (int z = CellOf(low.z - 1); z <= CellOf(high.z + 1); ++z)
				{
					g_SurfaceCells[CellKey(x, y, z)].push_back(number);
				}
			}
		}
	}
}

// The map's own light (the brightest of its colours) of the world's face at a point a trace hit, facing back along it
// (normal); -1 if none there has it.
float SurfaceLight(const JhlBsp& bsp, const Vector& hit, const Vector& normal)
{
	const auto found = g_SurfaceCells.find(CellKey(CellOf(hit.x), CellOf(hit.y), CellOf(hit.z)));

	if (found == g_SurfaceCells.end())
	{
		return -1;
	}

	for (const int number : found->second)
	{
		const auto& surface = g_Surfaces[number];
		const auto& face = bsp.Faces[surface.Face];

		if (DotProduct(face.Normal, normal) < 0.99f || std::abs(DotProduct(face.Normal, hit) - face.Dist) > 1)
		{
			continue;
		}

		// Within its polygon (a unit's leeway), whichever way it winds.
		bool front = true;
		bool back = true;

		for (std::size_t i = 0; i < face.Points.size(); ++i)
		{
			const Vector edge = face.Points[(i + 1) % face.Points.size()] - face.Points[i];
			const float length = edge.Length();
			const float side = length > 0 ? DotProduct(CrossProduct(edge, hit - face.Points[i]), face.Normal) / length : 0;
			front = front && side >= -1;
			back = back && side <= 1;
		}

		if (!front && !back)
		{
			continue;
		}

		// Its lightmap there, between its four nearest samples.
		const auto& texinfo = bsp.Texinfo[face.Texinfo];
		const float s = std::clamp((DotProduct(hit, Vector(texinfo.Vecs[0][0], texinfo.Vecs[0][1], texinfo.Vecs[0][2])) + texinfo.Vecs[0][3]) / 16 - surface.MinS,
			0.0f, surface.Width - 1.0f);
		const float t = std::clamp((DotProduct(hit, Vector(texinfo.Vecs[1][0], texinfo.Vecs[1][1], texinfo.Vecs[1][2])) + texinfo.Vecs[1][3]) / 16 - surface.MinT,
			0.0f, surface.Height - 1.0f);
		const int s0 = static_cast<int>(s);
		const int t0 = static_cast<int>(t);
		const int s1 = std::min(s0 + 1, surface.Width - 1);
		const int t1 = std::min(t0 + 1, surface.Height - 1);

		const auto sample = [&](int column, int row)
		{
			const unsigned char* rgb = &bsp.Lighting[face.LightOffset + (static_cast<std::size_t>(row) * surface.Width + column) * 3];
			return static_cast<float>(std::max({rgb[0], rgb[1], rgb[2]}));
		};

		const float fs = s - s0;
		const float ft = t - t0;
		return (sample(s0, t0) * (1 - fs) + sample(s1, t0) * fs) * (1 - ft) + (sample(s0, t1) * (1 - fs) + sample(s1, t1) * fs) * ft;
	}

	return -1;
}

// The light of what's around a point on the water as seen from just above it (see LitLight).
float SeenLight(const JhlBsp& bsp, const Vector& point, int* hits = nullptr)
{
	Vector start = point + Vector(0, 0, LightAbove);

	// In a wall (the water runs into it): from just beside it.
	for (int i = 0; gEngfuncs.PM_PointContents(start, nullptr) == CONTENTS_SOLID; ++i)
	{
		if (i == LightRays)
		{
			return LightAt(point);
		}

		const float angle = i * 2 * static_cast<float>(std::numbers::pi) / LightRays;
		start = point + Vector(std::cos(angle) * LightSpread, std::sin(angle) * LightSpread, LightAbove);
	}

	// Under the open sky (straight up or tilted 45 degrees four ways, the first thing there is the sky: a fence or a rail
	// overhead blocks one, a roof all): outdoors, as bright as ever, whatever's around.
	static constexpr float Tilt = 0.7071f;
	static const Vector SkyDirections[] = {{0, 0, 1}, {Tilt, 0, Tilt}, {-Tilt, 0, Tilt}, {0, Tilt, Tilt}, {0, -Tilt, Tilt}};

	for (const auto& direction : SkyDirections)
	{
		const Vector up = start + direction * SkyReach;
		const pmtrace_t trace = *gEngfuncs.PM_TraceLine(start, up, PM_TRACELINE_PHYSENTSONLY, 2, -1);

		if (trace.startsolid || trace.fraction >= 1)
		{
			continue;
		}

		// A point trace passes through the sky (not solid to it) to the void beyond: in the sky just short of where it
		// stopped, or just past it.
		Vector before = Vector(trace.endpos) - direction * 2;
		Vector beyond = Vector(trace.endpos) + direction * 2;

		if (gEngfuncs.PM_PointContents(before, nullptr) == CONTENTS_SKY || gEngfuncs.PM_PointContents(beyond, nullptr) == CONTENTS_SKY)
		{
			if (hits)
			{
				*hits = -1;
			}

			return LitLight;
		}
	}

	float total = 0;
	float weight = 0;

	for (int i = 0; i <= LightRays; ++i)
	{
		Vector end;

		if (i == LightRays)
		{
			end = point - Vector(0, 0, LightDepth);
		}
		else
		{
			const float angle = i * 2 * static_cast<float>(std::numbers::pi) / LightRays;
			end = start + Vector(std::cos(angle) * LightReach, std::sin(angle) * LightReach, 0);
		}

		const pmtrace_t trace = *gEngfuncs.PM_TraceLine(start, end, PM_TRACELINE_PHYSENTSONLY, 2, -1);

		if (trace.startsolid || trace.allsolid || trace.fraction >= 1)
		{
			continue;
		}

		const float light = SurfaceLight(bsp, trace.endpos, trace.plane.normal);

		if (light < 0)
		{
			continue;
		}

		const float distance = trace.fraction * (end - start).Length();
		const float share = 1 / (1 + distance / LightFalloff);
		total += light * share;
		weight += share;

		if (hits)
		{
			++*hits;
		}
	}

	return weight > 0 ? total / weight : LightAt(point);
}

// A face's light, taken the first time it's drawn (budget allowing) and again if its entity has moved (see LitLight).
void Relight(const JhlBsp& bsp, LitFace& lit, const Vector& origin, const Vector& angles)
{
	const bool taken = lit.SampledTime >= 0;
	const bool moved = taken && ((lit.SampledOrigin - origin).Length() > 0.5f || (lit.SampledAngles - angles).Length() > 0.5f);

	if ((taken && !moved) || g_LightSpent >= LightTime)
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

	const auto began = std::chrono::steady_clock::now();
	const auto spent = g_LightSpent;

	for (; lit.Next < lit.Points.size() && spent + (std::chrono::steady_clock::now() - began) < LightTime; ++lit.Next)
	{
		lit.Light[lit.Next] = Brightness(SeenLight(bsp, place(lit.Points[lit.Next])));
	}

	g_LightSpent = spent + (std::chrono::steady_clock::now() - began);

	if (lit.Next < lit.Points.size())
	{
		return;
	}

	lit.SampledTime = gEngfuncs.GetClientTime();
	lit.SampledOrigin = origin;
	lit.SampledAngles = angles;
}

// The water's waves where OpenGL drew them (hw.dll's EmitWaterPolys): a water entity's surface raised at each point by
// two waves from its turbulence table (8 sin(2 pi i / 256)), times its scale (the map's WaveHeight / 8), lowered seen
// from under it; in its model's coordinates at the client's time. The software renderer drew none: here both, its
// ripples on OpenGL's waves.
float WaveHeight(float x, float y, double time)
{
	static const auto turbulence = []
	{
		std::array<float, 256> table{};

		for (int i = 0; i < 256; ++i)
			table[i] = static_cast<float>(8 * std::sin(i * 2 * std::numbers::pi / 256));

		return table;
	}();

	const int along = static_cast<int>(static_cast<double>(x * 5.0f) + time * 171.0 - y) & 0xFF;
	const int across = static_cast<int>(time * 160.0 + x + y) & 0xFF;
	return static_cast<float>((turbulence[along] + 8.0) * 0.8 + (turbulence[across] + 8.0));
}

// Draws a model's water faces, placed as the engine places a brush model (R_RotateForEntity: moved to its origin,
// turned by yaw about z, pitch (negated) about y, roll about x). The world's: those that face the view. An entity's: as
// the engine's R_DrawBrushModel (hw.dll) has them, only its level ones (any, gl_watersides 1) higher than a unit above
// its bottom (the entity's origin and its model's lowest point; turned, its origin less its radius), from either side:
// its top, seen from above and from under it, not its sides and floor.
void DrawModel(const JhlBsp& bsp, int modelIndex, const Vector& origin, const Vector& angles, float bottom = 0, float waves = 0,
	const std::array<float, 4>& color = {1, 1, 1, 1}, bool overEngine = false)
{
	const double time = gEngfuncs.GetClientTime();
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

				if ((!level && (!g_WaterSides || g_WaterSides->value == 0)) || bottom + 1 >= height ||
					(g_LitFaces[index].HasTwin && DotProduct(face.Normal, view) - face.Dist <= 0))
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
			// Over the engine's waved water: on its cut. Else the finer one (the light's gradients smoother).
			auto& litFace = overEngine && waves != 0 ? g_EngineFaces[index] : g_LitFaces[index];

			if (lit)
			{
				Relight(bsp, litFace, origin, angles);
			}

			// Waves lowered seen from under the face (the view no higher than its first point, as the engine compares them).
			const float waveScale = waves != 0 && v_origin.z <= face.Points[0].z ? -waves : waves;
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

					if (waveScale != 0)
					{
						const Vector raised(point.x, point.y, static_cast<float>(WaveHeight(point.x, point.y, time) * waveScale + point.z));
						gl.Vertex3fv(raised);
					}
					else
					{
						gl.Vertex3fv(point);
					}
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
	g_WaterOpacity = gEngfuncs.pfnRegisterVariable("jhl_visuals_wateropacity", "0.96", FCVAR_ARCHIVE);

	// The light under the player's eyes and the water's brightness for it there, for tuning the curve (LitLight).
	gEngfuncs.pfnAddCommand("jhl_debug_waterlight", []
		{
			const JhlBsp* bsp = JhlBsp_Get(g_MapName.c_str());
			int hits = 0;
			const float light = bsp ? SeenLight(*bsp, v_origin - Vector(0, 0, LightAbove), &hits) : LightAt(v_origin);
			gEngfuncs.Con_Printf("jhl_debug_waterlight: light %.0f around the view (%d of %d surfaces seen; 0: the floor below instead, -1: open sky above), water %.2f (full from %.0f, %.2f at the darkest)\n",
				light, hits, LightRays + 1, Brightness(light), LitLight, DarkestWater);
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
	water.Waves = ent->curstate.scale;
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
	const bool ready = Ready();
	g_SeeThrough = ready && g_WaterOpacity->value < 1;

	if (!ready)
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
	g_LightSpent = {};

	WaterDrawState state;
	gl.Color4f(1, 1, 1, 1);
	gl.Disable(GL_BLEND);

	// Over the engine's: the world's water, and opaque entities it draws with water on them.
	gl.DepthMask(GL_FALSE);
	gl.Enable(GL_POLYGON_OFFSET_FILL);
	gl.PolygonOffset(OverdrawOffsetFactor, OverdrawOffsetUnits);
	// The world's waves: its scale (worldspawn's WaveHeight / 8, Blue Shift's maps have them), as the engine waves it.
	const cl_entity_t* world = gEngfuncs.GetEntityByIndex(0);
	DrawModel(*bsp, 0, g_vecZero, g_vecZero, 0, world ? world->curstate.scale : 0, {1, 1, 1, 1}, true);

	for (const auto& water : g_Entities)
	{
		if (!water.Hidden && water.RenderMode == RenderNormal)
		{
			DrawModel(*bsp, water.Model, water.Origin, water.Angles, water.Bottom, water.Waves, {1, 1, 1, 1}, true);
		}
	}

	// In place of the engine's: opaque water entities.
	gl.Disable(GL_POLYGON_OFFSET_FILL);
	gl.DepthMask(GL_TRUE);

	for (const auto& water : g_Entities)
	{
		if (water.Hidden && water.RenderMode == RenderNormal && !g_SeeThrough)
		{
			DrawModel(*bsp, water.Model, water.Origin, water.Angles, water.Bottom, water.Waves);
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

	if (seeThrough.empty() && !g_SeeThrough)
	{
		return;
	}

	std::sort(seeThrough.begin(), seeThrough.end(), [](const WaterEntity* a, const WaterEntity* b)
		{ return a->Distance > b->Distance; });

	WaterDrawState state;
	gl.DepthMask(GL_FALSE);
	gl.Enable(GL_BLEND);

	// See-through (jhl_visuals_wateropacity): the solid water entities, at that opacity.
	if (g_SeeThrough)
	{
		const float opacity = std::clamp(g_WaterOpacity->value, 0.0f, 1.0f);
		gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		for (const auto& water : g_Entities)
		{
			if (water.Hidden && water.RenderMode == RenderNormal)
			{
				DrawModel(*bsp, water.Model, water.Origin, water.Angles, water.Bottom, water.Waves, {1, 1, 1, opacity});
			}
		}
	}

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

		DrawModel(*bsp, water->Model, water->Origin, water->Angles, water->Bottom, water->Waves, color);
	}
}
