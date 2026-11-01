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
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include "hud.h"
#include "com_model.h"
#include "view.h"
#include "jhl_bsp.h"
#include "jhl_flushfaces.h"

namespace
{
// How far out: the engine's view has its near plane at 4 units, so in a 24 bit depth buffer two surfaces d units away
// can be told apart from about d² / (4 * 2^24) units apart; coplanar faces made of different triangles differ by a few
// of those steps besides. 16 steps out: 0.06 units at 500, 0.24 at 1000, 1 at 2000; at least a tenth of a unit (so
// the software renderer, which sorts by the BSP rather than a depth buffer, puts it in front of the wall), at most 2.
constexpr float OffsetPerDistanceSquared = 16.0f / (4.0f * 16777216.0f);
constexpr float MinOffset = 0.1f;
constexpr float MaxOffset = 2;

// Faces count as coplanar within this (units, and of their normals), and as covering a world face from this much area
// of it (square units).
constexpr float PlaneEpsilon = 0.02f;
constexpr float NormalEpsilon = 0.0005f;
constexpr float MinOverlap = 1;

// The entity is where the map placed it within this (its origin is sent to 1/8 unit).
constexpr float RestEpsilon = 0.2f;

// A face, in the world's coordinates.
struct Face
{
	Vector Normal;
	float Dist = 0;
	std::vector<Vector> Points;
	Vector Mins, Maxs;
};

// A plane an entity is flush with the world on, where the map placed it.
struct FlushPlane
{
	Vector Normal;
	float Dist = 0;
	Vector Mins, Maxs; // Of its faces there.
};

struct FlushModel
{
	Vector Origin; // Where the map placed it.
	std::vector<FlushPlane> Planes;
};

struct Moved
{
	cl_entity_t* Entity;
	Vector Offset;
};

cvar_t* g_FlushFaces = nullptr;
std::string g_MapName;
std::vector<FlushModel> g_Models; // By submodel (*1 ...).
std::vector<Moved> g_Moved;
float g_MovedTime = -1;

// The two axes a face is seen along best, to compare faces on a plane in 2D.
void PlaneAxes(const Vector& normal, int& u, int& v)
{
	const float x = std::abs(normal.x);
	const float y = std::abs(normal.y);
	const float z = std::abs(normal.z);

	if (z >= x && z >= y)
	{
		u = 0;
		v = 1;
	}
	else if (y >= x)
	{
		u = 0;
		v = 2;
	}
	else
	{
		u = 1;
		v = 2;
	}
}

struct Point2D
{
	float X, Y;
};

float SignedArea(const std::vector<Point2D>& polygon)
{
	float area = 0;

	for (std::size_t i = 0; i < polygon.size(); ++i)
	{
		const auto& a = polygon[i];
		const auto& b = polygon[(i + 1) % polygon.size()];
		area += a.X * b.Y - b.X * a.Y;
	}

	return area / 2;
}

std::vector<Point2D> Project(const Face& face, int u, int v)
{
	std::vector<Point2D> polygon;
	polygon.reserve(face.Points.size());

	for (const auto& point : face.Points)
	{
		polygon.push_back({point[u], point[v]});
	}

	if (SignedArea(polygon) < 0)
	{
		std::reverse(polygon.begin(), polygon.end());
	}

	return polygon;
}

// The area two convex polygons (counter-clockwise) share (Sutherland-Hodgman).
float OverlapArea(std::vector<Point2D> subject, const std::vector<Point2D>& clipper)
{
	std::vector<Point2D> input;

	for (std::size_t i = 0; i < clipper.size() && !subject.empty(); ++i)
	{
		const auto a = clipper[i];
		const auto b = clipper[(i + 1) % clipper.size()];
		const auto side = [&](const Point2D& p)
		{
			return (b.X - a.X) * (p.Y - a.Y) - (b.Y - a.Y) * (p.X - a.X);
		};

		input.swap(subject);
		subject.clear();

		for (std::size_t j = 0; j < input.size(); ++j)
		{
			const auto p = input[j];
			const auto q = input[(j + 1) % input.size()];
			const float sp = side(p);
			const float sq = side(q);

			if (sp >= 0)
			{
				subject.push_back(p);
			}

			if ((sp >= 0) != (sq >= 0))
			{
				const float t = sp / (sp - sq);
				subject.push_back({p.X + t * (q.X - p.X), p.Y + t * (q.Y - p.Y)});
			}
		}
	}

	return subject.size() >= 3 ? std::abs(SignedArea(subject)) : 0;
}

// Finds the brush entities flush with the world in the map being played.
void LoadMap(const char* mapName)
{
	g_Models.clear();

	const JhlBsp* bsp = JhlBsp_Get(mapName);

	if (!bsp || bsp->Models.empty())
	{
		return;
	}

	// A face of a model placed at origin, in the world's coordinates.
	const auto makeFace = [&](const JhlBspFace& source, const Vector& origin, Face& face)
	{
		if (source.Points.empty())
		{
			return false;
		}

		face.Normal = source.Normal;
		face.Dist = source.Dist + DotProduct(face.Normal, origin);
		face.Points.clear();
		face.Mins = Vector(99999, 99999, 99999);
		face.Maxs = Vector(-99999, -99999, -99999);

		for (const auto& sourcePoint : source.Points)
		{
			const Vector point = sourcePoint + origin;
			face.Points.push_back(point);

			for (int k = 0; k < 3; ++k)
			{
				face.Mins[k] = std::min(face.Mins[k], point[k]);
				face.Maxs[k] = std::max(face.Maxs[k], point[k]);
			}
		}

		return true;
	};

	// The world's faces, by distance from the world's origin (to the unit) for finding those on a plane. Sky faces
	// aren't drawn as the world is.
	std::vector<Face> world;
	std::unordered_map<int, std::vector<std::size_t>> worldByDist;

	for (int i = 0; i < bsp->Models[0].NumFaces; ++i)
	{
		const auto& source = bsp->Faces[bsp->Models[0].FirstFace + i];
		Face face;

		if (0 == strnicmp(bsp->TextureName(source).c_str(), "sky", 4) || !makeFace(source, g_vecZero, face))
		{
			continue;
		}

		worldByDist[static_cast<int>(std::floor(face.Dist))].push_back(world.size());
		world.push_back(std::move(face));
	}

	g_Models.resize(bsp->Models.size());

	for (const auto& keys : bsp->Entities)
	{
		const auto model = keys.find("model");

		if (model == keys.end() || model->second.size() < 2 || model->second[0] != '*')
		{
			continue;
		}

		const int submodel = std::atoi(model->second.c_str() + 1);

		if (submodel <= 0 || static_cast<std::size_t>(submodel) >= bsp->Models.size())
		{
			continue;
		}

		// Placed turned: left alone (rare, and where it's drawn turned isn't worth working out here).
		if (const auto angles = keys.find("angles"); angles != keys.end() && JhlBsp_ParseVector(angles->second) != g_vecZero)
		{
			continue;
		}

		auto& flush = g_Models[submodel];

		if (const auto origin = keys.find("origin"); origin != keys.end())
		{
			flush.Origin = JhlBsp_ParseVector(origin->second);
		}

		Face face;

		for (int i = 0; i < bsp->Models[submodel].NumFaces; ++i)
		{
			if (!makeFace(bsp->Faces[bsp->Models[submodel].FirstFace + i], flush.Origin, face))
			{
				continue;
			}

			int u, v;
			PlaneAxes(face.Normal, u, v);
			const auto polygon = Project(face, u, v);
			bool covers = false;

			for (int bucket = -1; bucket <= 1 && !covers; ++bucket)
			{
				const auto found = worldByDist.find(static_cast<int>(std::floor(face.Dist)) + bucket);

				if (found == worldByDist.end())
				{
					continue;
				}

				for (const auto w : found->second)
				{
					const auto& other = world[w];

					if (std::abs(other.Dist - face.Dist) > PlaneEpsilon || DotProduct(other.Normal, face.Normal) < 1 - NormalEpsilon)
					{
						continue;
					}

					if (OverlapArea(polygon, Project(other, u, v)) >= MinOverlap)
					{
						covers = true;
						break;
					}
				}
			}

			if (!covers)
			{
				continue;
			}

			const auto plane = std::find_if(flush.Planes.begin(), flush.Planes.end(), [&](const FlushPlane& candidate)
				{ return std::abs(candidate.Dist - face.Dist) <= PlaneEpsilon && DotProduct(candidate.Normal, face.Normal) >= 1 - NormalEpsilon; });

			if (plane == flush.Planes.end())
			{
				flush.Planes.push_back({face.Normal, face.Dist, face.Mins, face.Maxs});
			}
			else
			{
				for (int k = 0; k < 3; ++k)
				{
					plane->Mins[k] = std::min(plane->Mins[k], face.Mins[k]);
					plane->Maxs[k] = std::max(plane->Maxs[k], face.Maxs[k]);
				}
			}
		}
	}
}

// The farthest point of a box from a point.
float FarthestDistance(const Vector& from, const Vector& mins, const Vector& maxs)
{
	Vector farthest;

	for (int k = 0; k < 3; ++k)
	{
		farthest[k] = std::max(std::abs(from[k] - mins[k]), std::abs(from[k] - maxs[k]));
	}

	return farthest.Length();
}
}

void JhlFlushFaces_Initialize()
{
	g_FlushFaces = gEngfuncs.pfnRegisterVariable("jhl_visuals_flushfaces", "1", FCVAR_ARCHIVE);
}

void JhlFlushFaces_Add(cl_entity_t* ent)
{
	if (0 == g_FlushFaces->value || !ent->model || ent->model->type != mod_brush || ent->model->name[0] != '*')
	{
		return;
	}

	// The engine has put every entity back where it is since the last frame drawn.
	if (const float time = gEngfuncs.GetClientTime(); time != g_MovedTime)
	{
		g_Moved.clear();
		g_MovedTime = time;
	}

	if (const char* mapName = gEngfuncs.pfnGetLevelName(); mapName && g_MapName != mapName)
	{
		g_MapName = mapName;
		LoadMap(mapName);

		const auto flushCount = std::count_if(g_Models.begin(), g_Models.end(), [](const FlushModel& model)
			{ return !model.Planes.empty(); });
		gEngfuncs.Con_DPrintf("johnhalflife: %d brush entities flush with walls in %s\n", static_cast<int>(flushCount), mapName);
	}

	const int submodel = std::atoi(ent->model->name + 1);

	if (submodel <= 0 || static_cast<std::size_t>(submodel) >= g_Models.size())
	{
		return;
	}

	const auto& flush = g_Models[submodel];

	// Only where the map placed it (a door closed, not a train on its way).
	if (flush.Planes.empty() || (ent->curstate.origin - flush.Origin).Length() > RestEpsilon || ent->curstate.angles != g_vecZero)
	{
		return;
	}

	// Out of each wall it's flush with that faces the view (last frame's: near enough).
	Vector direction;
	float distance = 0;

	for (const auto& plane : flush.Planes)
	{
		if (DotProduct(plane.Normal, v_origin) - plane.Dist > 0)
		{
			direction = direction + plane.Normal;
			distance = std::max(distance, FarthestDistance(v_origin, plane.Mins, plane.Maxs));
		}
	}

	if (direction.Length() < 0.01f)
	{
		return;
	}

	const float amount = std::clamp(OffsetPerDistanceSquared * distance * distance, MinOffset, MaxOffset);
	const Vector offset = direction.Normalize() * amount;

	ent->origin = ent->origin + offset;

	// Added again before the view was drawn (time stands still while paused): the engine has put it back since.
	if (auto moved = std::find_if(g_Moved.begin(), g_Moved.end(), [&](const Moved& candidate)
			{ return candidate.Entity == ent; });
		moved != g_Moved.end())
	{
		moved->Offset = offset;
	}
	else
	{
		g_Moved.push_back({ent, offset});
	}
}

void JhlFlushFaces_Restore()
{
	if (gEngfuncs.GetClientTime() == g_MovedTime)
	{
		for (const auto& moved : g_Moved)
		{
			moved.Entity->origin = moved.Entity->origin - moved.Offset;
		}
	}

	g_Moved.clear();
}
