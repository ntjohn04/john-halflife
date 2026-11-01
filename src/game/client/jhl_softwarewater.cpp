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
#include <map>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "hud.h"
#include "com_model.h"
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

JhlGL& gl = g_JhlGL;
cvar_t* g_SoftwareWater = nullptr;

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
};

std::string g_MapName;
bool g_MapReady = false;
std::vector<WaterTexture> g_Textures;
std::vector<WaterModel> g_Models;
std::vector<WaterEntity> g_Entities; // This frame's, until the see-through ones are drawn.

// What was made of the map's water and its entities, for jhl_softwarewater_info.
std::vector<std::string> g_TextureReport;
std::map<int, std::string> g_EntityReport; // By model.

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
int EngineTexture(const model_t* world, const JhlBsp& bsp, int miptex, std::string& why)
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

// The map's water: its textures (read from the engine's) and every model's water faces.
void LoadMap(const char* mapName)
{
	FreeTextures();
	g_Models.clear();
	g_TextureReport.clear();
	g_EntityReport.clear();
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

		if (LoadTexture(water, EngineTexture(worldEntity->model, *bsp, static_cast<int>(miptex), why), why))
		{
			g_TextureReport.push_back(water.Name + ": " + std::to_string(water.Width) + " x " + std::to_string(water.Height));
			textureByMiptex[miptex] = static_cast<int>(g_Textures.size());
			g_Textures.push_back(std::move(water));
		}
		else
		{
			g_TextureReport.push_back(water.Name + ": not used, " + why);
		}
	}

	g_Models.resize(bsp->Models.size());

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

// Draws a model's water faces that face the view, placed as the engine places a brush model (R_RotateForEntity: moved
// to its origin, turned by yaw about z, pitch (negated) about y, roll about x).
void DrawModel(const JhlBsp& bsp, int modelIndex, const Vector& origin, const Vector& angles)
{
	const auto& model = g_Models[modelIndex];
	const bool turned = angles != g_vecZero;

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

			if (DotProduct(face.Normal, view) - face.Dist <= 0)
			{
				continue;
			}

			const auto& texinfo = bsp.Texinfo[face.Texinfo];

			gl.Begin(GL_POLYGON);

			for (const auto& point : face.Points)
			{
				const float s = DotProduct(point, Vector(texinfo.Vecs[0][0], texinfo.Vecs[0][1], texinfo.Vecs[0][2])) + texinfo.Vecs[0][3];
				const float t2 = DotProduct(point, Vector(texinfo.Vecs[1][0], texinfo.Vecs[1][1], texinfo.Vecs[1][2])) + texinfo.Vecs[1][3];
				gl.TexCoord2f(s / TileSize, t2 / TileSize);
				gl.Vertex3fv(point);
			}

			gl.End();
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

void JhlSoftwareWater_Initialize()
{
	g_SoftwareWater = gEngfuncs.pfnRegisterVariable("jhl_visuals_softwarewater", "0", FCVAR_ARCHIVE);

	// TODO(johnhalflife): temporary, remove once software water is settled (with g_TextureReport, g_EntityReport and
	// the reasons recorded for them).
	gEngfuncs.pfnAddCommand("jhl_softwarewater_info", []
		{
			if (0 == g_SoftwareWater->value)
			{
				gEngfuncs.Con_Printf("jhl_visuals_softwarewater is off\n");
				return;
			}

			if (!JhlGL_Available())
			{
				gEngfuncs.Con_Printf("Software water needs the OpenGL renderer\n");
				return;
			}

			gEngfuncs.Con_Printf("Software water in %s: %s\n", g_MapName.c_str(), g_MapReady ? "ready" : "map not read");

			for (const auto& line : g_TextureReport)
			{
				gEngfuncs.Con_Printf("  texture %s\n", line.c_str());
			}

			for (std::size_t m = 0; m < g_Models.size(); ++m)
			{
				if (g_Models[m].HasWater)
				{
					const auto report = g_EntityReport.find(static_cast<int>(m));
					gEngfuncs.Con_Printf("  model *%d: %s%s\n", static_cast<int>(m), g_Models[m].AllWater ? "all water" : "partly water",
						m == 0 ? " (the world: drawn over the engine's)" : report != g_EntityReport.end() ? (", " + report->second).c_str() : ", not seen yet");
				}
			}
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

	if (model.AllWater && knownMode)
	{
		// All water: drawn here, not by the engine.
		water.Hidden = true;
		g_EntityReport[modelIndex] = "drawn here, rendermode " + std::to_string(renderMode);
	}
	else if (renderMode != RenderNormal)
	{
		// See-through and partly water: left to the engine.
		g_EntityReport[modelIndex] = "left to the engine: rendermode " + std::to_string(renderMode) + (model.AllWater ? "" : ", not all water");
		return false;
	}
	else
	{
		g_EntityReport[modelIndex] = "drawn over the engine's (opaque, not all water)";
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
			DrawModel(*bsp, water.Model, water.Origin, water.Angles);
		}
	}

	// In place of the engine's: opaque water entities.
	gl.Disable(GL_POLYGON_OFFSET_FILL);
	gl.DepthMask(GL_TRUE);

	for (const auto& water : g_Entities)
	{
		if (water.Hidden && water.RenderMode == RenderNormal)
		{
			DrawModel(*bsp, water.Model, water.Origin, water.Angles);
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
		if (water->RenderMode == RenderTransAdd)
		{
			gl.BlendFunc(GL_ONE, GL_ONE);
			gl.Color4f(water->Alpha, water->Alpha, water->Alpha, 1);
		}
		else
		{
			gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			gl.Color4f(1, 1, 1, water->Alpha);
		}

		DrawModel(*bsp, water->Model, water->Origin, water->Angles);
	}
}
