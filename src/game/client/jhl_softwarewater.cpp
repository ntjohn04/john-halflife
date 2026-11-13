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
#include "entity_types.h"
#include "event_api.h"
#include "pm_defs.h"
#include "pmtrace.h"
#include "triangleapi.h"
#include "view.h"
#include "jhl_bsp.h"
#include "jhl_gl.h"
#include "jhl_softwarewater.h"

namespace
{
constexpr int FieldSize = 128;
constexpr int FieldMask = FieldSize - 1;
constexpr int FieldCells = FieldSize * FieldSize;

constexpr float StepTime = 4.0f / 72.0f;
constexpr float DropTime = 0.1f;
constexpr int MostStepsAFrame = 4;

constexpr float TileSize = FieldSize;

constexpr float OverdrawOffsetFactor = -1;
constexpr float OverdrawOffsetUnits = -2;

constexpr int RenderNormal = 0;
constexpr int RenderTransTexture = 2;
constexpr int RenderTransAdd = 5;

constexpr float LitLight = 96;
constexpr float DarkestWater = 0.2f;
constexpr float SkyReach = 8192;
constexpr float LightAbove = 12;
constexpr int LightRays = 16;
constexpr float LightReach = 192;
constexpr int FloorRays = 8;
constexpr float LightDepth = 512; // The floor's furthest under it.
constexpr float LightFalloff = 48;
constexpr float LightSpread = 24;
constexpr float LightGrid = 32;
constexpr float EngineGrid = 64;
constexpr float LightRings[] = {48, 80};
constexpr std::chrono::microseconds LightTime{1500};
constexpr float SurfaceCell = 128;
std::chrono::steady_clock::duration g_LightSpent{};

JhlGL& gl = g_JhlGL;
cvar_t* g_SoftwareWater = nullptr;
cvar_t* g_WaterSides = nullptr;
cvar_t* g_WaterLight = nullptr;

cvar_t* g_WaterOpacity = nullptr;
bool g_SeeThrough = false;

using Field = std::array<std::int16_t, FieldCells>;
Field g_Fields[2]{};
int g_Newest = 0;
float g_LastStep = -1;
float g_LastDrop = 0;
bool g_Stepped = false;

struct WaterTexture
{
	std::string Name;
	int Width = 0;
	int Height = 0;
	std::vector<std::uint32_t> Source;
	GLuint Tile = 0;
	bool Mipmapped = false;
	int Fog[4]{};
	bool HasFog = false;
	float Darkest = DarkestWater;
	float Full = LitLight;
						   // full bright but where it ends in a dark corner.
	bool Smoothed = false;
						   // point's own unevenness as patches).
};

// A model's water faces, by texture.
struct WaterModel
{
	std::vector<std::vector<int>> FacesByTexture;
	bool HasWater = false;
	bool AllWater = false;
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
	float Bottom = 0;
	float Waves = 0;
};

struct LitFace
{
	std::vector<Vector> Points;
	std::vector<int> Counts;		  // Each piece's number of points.
	std::vector<float> Light;
	std::size_t Next = 0;
	bool HasTwin = false;
	Vector SampledOrigin;
	Vector SampledAngles;
	float SampledTime = -1;
	bool Indexed = false;
	std::vector<float> Smooth;
	int SmoothVersion = -1;
	float SmoothTime = -1;
	float Blend = 0;
	bool Stale = false;
	bool Drawn = false;
	Vector LastOrigin;
	Vector LastAngles;
	float FadeTime = 0;
};

std::string g_MapName;
bool g_MapReady = false;
struct LightCell
{
	float Sum = 0;
	int Count = 0;
};

constexpr float SmoothReach = 128;

constexpr float MovingWater = 0.85f;
constexpr float MovingFade = 0.75f;
constexpr float SmoothEvery = 0.25f;
std::unordered_map<std::int64_t, LightCell> g_LightCells;
int g_LightCellsVersion = 0;

std::int64_t LightCellKey(int model, int x, int y, int z)
{
	return (static_cast<std::int64_t>(model & 0x3FF) << 54) | ((static_cast<std::int64_t>(x) & 0x3FFFF) << 36) |
		   ((static_cast<std::int64_t>(y) & 0x3FFFF) << 18) | (static_cast<std::int64_t>(z) & 0x3FFFF);
}

int LightCellOf(float value)
{
	return static_cast<int>(std::floor(value / LightGrid));
}

float SmoothedLight(int model, const Vector& point, float own)
{
	const int cx = LightCellOf(point.x);
	const int cy = LightCellOf(point.y);
	const int cz = LightCellOf(point.z);
	const int reach = static_cast<int>(SmoothReach / LightGrid);
	float total = 0;
	float weight = 0;

	for (int x = cx - reach; x <= cx + reach; ++x)
	{
		for (int y = cy - reach; y <= cy + reach; ++y)
		{
			const auto found = g_LightCells.find(LightCellKey(model, x, y, cz));

			if (found == g_LightCells.end() || found->second.Count == 0)
			{
				continue;
			}

			const float dx = (x + 0.5f) * LightGrid - point.x;
			const float dy = (y + 0.5f) * LightGrid - point.y;
			const float distance = std::sqrt(dx * dx + dy * dy);

			if (distance > SmoothReach + LightGrid)
			{
				continue;
			}

			const float share = 1 / (1 + distance / LightGrid);
			total += found->second.Sum / found->second.Count * share;
			weight += share;
		}
	}

	return weight > 0 ? total / weight : own;
}

std::vector<LitFace> g_LitFaces;
std::vector<LitFace> g_EngineFaces;

struct LitSurface
{
	int Face = 0;
	int MinS = 0;
	int MinT = 0;
	int Width = 0;
	int Height = 0;
};

std::vector<LitSurface> g_Surfaces;
std::unordered_map<std::int64_t, std::vector<int>> g_SurfaceCells;
std::vector<WaterTexture> g_Textures;
std::vector<WaterModel> g_Models;
std::vector<WaterEntity> g_Entities;
std::vector<WaterEntity> g_LastEntities;
int g_EntitiesAdded = 0;

int g_LastFog[4]{};
float g_LastFogTime = -1;

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

	lit.Light.assign(lit.Points.size(), LitLight);
}

void IndexSurfaces(const JhlBsp& bsp);

void LoadMap(const char* mapName)
{
	FreeTextures();
	g_Models.clear();
	g_LitFaces.clear();
	g_LightCells.clear();
	++g_LightCellsVersion;
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
		if (0 == strnicmp(water.Name.c_str(), "!fluid", 6) || 0 == strnicmp(water.Name.c_str(), "!toxicgrn", 9))
			water.Darkest = 1;
		else if (0 == stricmp(water.Name.c_str(), "!gwater3"))
			water.Darkest = 0.8f;
		else if (0 == stricmp(water.Name.c_str(), "!bwater2") || 0 == stricmp(water.Name.c_str(), "!gwater2"))
			water.Darkest = 0.6f;
		else if (0 == stricmp(water.Name.c_str(), "!coolant5"))
		{
			water.Full = 48;
			water.Darkest = 0.2f;
			water.Smoothed = true;
		}
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
	g_Stepped = true;
}

bool Ready()
{
	if (0 == g_SoftwareWater->value || !JhlGL_Available())
	{
		return false;
	}

	// The world being drawn, not the level name, which can run ahead of it in a level change.
	const cl_entity_t* world = gEngfuncs.GetEntityByIndex(0);
	const char* mapName = world && world->model ? world->model->name : nullptr;

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

float Brightness(float light, float full = LitLight, float darkest = DarkestWater)
{
	const float x = std::clamp(light / full, 0.0f, 1.0f);
	return darkest + (1 - darkest) * x * x * (3 - 2 * x);
}

std::int64_t CellKey(int x, int y, int z)
{
	return ((static_cast<std::int64_t>(x) & 0x1FFFFF) << 42) | ((static_cast<std::int64_t>(y) & 0x1FFFFF) << 21) | (static_cast<std::int64_t>(z) & 0x1FFFFF);
}

int CellOf(float value)
{
	return static_cast<int>(std::floor(value / SurfaceCell));
}

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

pmtrace_t WorldTrace(const Vector& start, const Vector& end)
{
	pmtrace_t trace;
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(start, end, PM_WORLD_ONLY, -1, &trace);
	return trace;
}

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

	static constexpr float Tilt = 0.7071f;
	static const Vector SkyDirections[] = {{0, 0, 1}, {Tilt, 0, Tilt}, {-Tilt, 0, Tilt}, {0, Tilt, Tilt}, {0, -Tilt, Tilt}};

	for (const auto& direction : SkyDirections)
	{
		const Vector up = start + direction * SkyReach;
		const pmtrace_t trace = WorldTrace(start, up);

		if (trace.startsolid || trace.fraction >= 1)
		{
			continue;
		}

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

	for (int i = 0; i < LightRays + FloorRays; ++i)
	{
		Vector end;

		if (i >= LightRays)
		{
			const float angle = (i - LightRays + 0.5f) * 2 * static_cast<float>(std::numbers::pi) / FloorRays;
			end = point + Vector(std::cos(angle) * LightDepth, std::sin(angle) * LightDepth, -LightDepth);
		}
		else
		{
			const float angle = i * 2 * static_cast<float>(std::numbers::pi) / LightRays;
			end = start + Vector(std::cos(angle) * LightReach, std::sin(angle) * LightReach, 0);
		}

		const pmtrace_t trace = WorldTrace(start, end);

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

void Relight(const JhlBsp& bsp, LitFace& lit, const Vector& origin, const Vector& angles, int model)
{
	const float now = gEngfuncs.GetClientTime();
	const float step = std::clamp(now - lit.FadeTime, 0.0f, 0.1f) / MovingFade;
	lit.FadeTime = now;

	const bool taken = lit.SampledTime >= 0;
	const bool moving = lit.Drawn && ((lit.LastOrigin - origin).Length() > 0.01f || (lit.LastAngles - angles).Length() > 0.01f);
	lit.LastOrigin = origin;
	lit.LastAngles = angles;
	lit.Drawn = true;

	if (taken && ((lit.SampledOrigin - origin).Length() > 0.5f || (lit.SampledAngles - angles).Length() > 0.5f))
	{
		lit.Stale = true;
	}

	lit.Blend = lit.Stale ? std::min(lit.Blend + step, 1.0f) : std::max(lit.Blend - step, 0.0f);

	if (moving)
	{
		if (lit.Stale)
		{
			lit.Next = 0;
		}

		return;
	}

	if ((taken && !lit.Stale) || g_LightSpent >= LightTime)
	{
		return;
	}

	if (taken)
	{
		lit.SampledTime = -1;
		lit.Next = 0;
	}

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
		lit.Light[lit.Next] = SeenLight(bsp, place(lit.Points[lit.Next]));
	}

	g_LightSpent = spent + (std::chrono::steady_clock::now() - began);

	if (lit.Next < lit.Points.size())
	{
		return;
	}

	if (!lit.Indexed)
	{
		for (std::size_t i = 0; i < lit.Points.size(); ++i)
		{
			auto& cell = g_LightCells[LightCellKey(model, LightCellOf(lit.Points[i].x), LightCellOf(lit.Points[i].y), LightCellOf(lit.Points[i].z))];
			cell.Sum += lit.Light[i];
			++cell.Count;
		}

		lit.Indexed = true;
		++g_LightCellsVersion;
	}

	lit.SampledTime = gEngfuncs.GetClientTime();
	lit.SampledOrigin = origin;
	lit.SampledAngles = angles;
	lit.Stale = false;
}

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

void DrawModel(const JhlBsp& bsp, int modelIndex, const Vector& origin, const Vector& angles, float bottom = 0, float waves = 0,
	const std::array<float, 4>& color = {1, 1, 1, 1}, bool overEngine = false)
{
	const double time = gEngfuncs.GetClientTime();
	const auto& model = g_Models[modelIndex];
	const bool turned = angles != g_vecZero;
	const bool lit = g_WaterLight && g_WaterLight->value != 0;

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
			auto& litFace = overEngine && waves != 0 ? g_EngineFaces[index] : g_LitFaces[index];

			const float darkest = g_Textures[t].Darkest;
			const float full = g_Textures[t].Full;
			const bool faceLit = lit && darkest < 1;

			if (faceLit)
			{
				Relight(bsp, litFace, origin, angles, modelIndex);

				const float now = gEngfuncs.GetClientTime();

				if (g_Textures[t].Smoothed && litFace.Indexed && litFace.SmoothVersion != g_LightCellsVersion &&
					(litFace.SmoothVersion < 0 || now < litFace.SmoothTime || now - litFace.SmoothTime >= SmoothEvery))
				{
					litFace.SmoothTime = now;
					litFace.Smooth.resize(litFace.Points.size());

					for (std::size_t i = 0; i < litFace.Points.size(); ++i)
						litFace.Smooth[i] = SmoothedLight(modelIndex, litFace.Points[i], litFace.Light[i]);

					litFace.SmoothVersion = g_LightCellsVersion;
				}
			}

			const bool smooth = faceLit && g_Textures[t].Smoothed && litFace.SmoothVersion >= 0;

			const float waveScale = waves != 0 && v_origin.z <= face.Points[0].z ? -waves : waves;
			std::size_t next = 0;

			for (const int count : litFace.Counts)
			{
				gl.Begin(GL_POLYGON);

				for (int i = 0; i < count; ++i, ++next)
				{
					const Vector& point = litFace.Points[next];
					float light = faceLit ? Brightness(smooth ? litFace.Smooth[next] : litFace.Light[next], full, darkest) : 1.0f;

					if (faceLit && litFace.Blend > 0)
					{
						light += (std::max(MovingWater, darkest) - light) * litFace.Blend;
					}

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

		gl.ActiveTexture(GL_TEXTURE1);
		gl.Disable(GL_TEXTURE_2D);
		gl.ActiveTexture(GL_TEXTURE0);
		gl.GetIntegerv(GL_TEXTURE_BINDING_2D, &Texture);
		gl.Enable(GL_TEXTURE_2D);
		gl.TexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

		gl.Disable(GL_CULL_FACE);

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
	g_WaterOpacity = gEngfuncs.pfnRegisterVariable("jhl_visuals_wateropacity", "0.85", FCVAR_ARCHIVE);

	gEngfuncs.pfnAddCommand("jhl_debug_waterlight", []
		{
			const JhlBsp* bsp = JhlBsp_Get(g_MapName.c_str());
			int hits = 0;
			const float light = bsp ? SeenLight(*bsp, v_origin - Vector(0, 0, LightAbove), &hits) : LightAt(v_origin);
			gEngfuncs.Con_Printf("jhl_debug_waterlight: light %.0f around the view (%d of %d surfaces seen; 0: the floor below instead, -1: open sky above), water %.2f (full from %.0f, %.2f at the darkest)\n",
				light, hits, LightRays + FloorRays, Brightness(light), LitLight, DarkestWater);
		});
}

bool JhlSoftwareWater_Add(int type, cl_entity_t* ent)
{
	++g_EntitiesAdded;

	if (type != ET_NORMAL || !ent->model || ent->model->type != mod_brush || ent->model->name[0] != '*' || !Ready())
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
	// A redrawn frame (the loading screen) passes no entities: draw the last frame's.
	if (0 == g_EntitiesAdded)
	{
		g_Entities = g_LastEntities;
	}

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

	gl.DepthMask(GL_FALSE);
	gl.Enable(GL_POLYGON_OFFSET_FILL);
	gl.PolygonOffset(OverdrawOffsetFactor, OverdrawOffsetUnits);
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
		~ClearEntities()
		{
			g_LastEntities = std::move(g_Entities);
			g_Entities.clear();
			g_EntitiesAdded = 0;
		}
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
