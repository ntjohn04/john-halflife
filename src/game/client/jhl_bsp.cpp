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
#include <cstdlib>
#include <cstring>
#include <memory>

#include "hud.h"
#include "com_model.h"
#include "utils/filesystem_utils.h"
#include "jhl_bsp.h"

namespace
{
// BSP version 30, as the engine reads it (Blue Shift's maps swap the first two lumps).
constexpr int BspVersion = 30;

enum BspLump
{
	LumpEntities = 0,
	LumpPlanes = 1,
	LumpTextures = 2,
	LumpVertexes = 3,
	LumpTexinfo = 6,
	LumpFaces = 7,
	LumpEdges = 12,
	LumpSurfedges = 13,
	LumpModels = 14,
	LumpCount = 15
};

struct BspPlane
{
	float Normal[3];
	float Dist;
	int Type;
};

struct BspFace
{
	std::uint16_t Plane;
	std::uint16_t Side;
	int FirstEdge;
	std::uint16_t NumEdges;
	std::uint16_t Texinfo;
	std::uint8_t Styles[4];
	int LightOffset;
};

struct BspEdge
{
	std::uint16_t V[2];
};

const std::string NoName;
std::unique_ptr<JhlBsp> g_Bsp;
std::string g_FailedMap;

template <typename T>
bool ReadLump(const std::vector<std::byte>& data, const int (&lumps)[LumpCount][2], int lump, std::vector<T>& out)
{
	const int offset = lumps[lump][0];
	const int length = lumps[lump][1];

	if (offset < 0 || length < 0 || static_cast<std::size_t>(offset) + length > data.size() || length % sizeof(T) != 0)
	{
		return false;
	}

	out.resize(length / sizeof(T));
	std::memcpy(out.data(), data.data() + offset, length);
	return true;
}

// The keys of each entity in the entity lump.
std::vector<std::unordered_map<std::string, std::string>> ParseEntities(const char* text, std::size_t length)
{
	std::vector<std::unordered_map<std::string, std::string>> entities;
	std::string key;
	bool haveKey = false;

	for (std::size_t i = 0; i < length && text[i] != '\0'; ++i)
	{
		if (text[i] == '{')
		{
			entities.emplace_back();
			haveKey = false;
		}
		else if (text[i] == '"' && !entities.empty())
		{
			const std::size_t start = ++i;

			while (i < length && text[i] != '"' && text[i] != '\0')
			{
				++i;
			}

			std::string token(text + start, i - start);

			if (haveKey)
			{
				entities.back()[key] = std::move(token);
			}
			else
			{
				key = std::move(token);
			}

			haveKey = !haveKey;
		}
	}

	return entities;
}

std::unique_ptr<JhlBsp> Load(const char* mapName)
{
	const auto data = FileSystem_LoadFileIntoBuffer(mapName, FileContentFormat::Binary);

	if (data.size() < sizeof(int) * (1 + LumpCount * 2))
	{
		return nullptr;
	}

	int version;
	int lumps[LumpCount][2];
	std::memcpy(&version, data.data(), sizeof(version));
	std::memcpy(lumps, data.data() + sizeof(version), sizeof(lumps));

	if (version != BspVersion)
	{
		return nullptr;
	}

	const auto lumpStartsEntities = [&](int lump)
	{
		const int offset = lumps[lump][0];
		return offset >= 0 && lumps[lump][1] > 0 && static_cast<std::size_t>(offset) < data.size() &&
			   static_cast<char>(data[offset]) == '{';
	};

	if (!lumpStartsEntities(LumpEntities) && lumpStartsEntities(LumpPlanes))
	{
		std::swap(lumps[LumpEntities], lumps[LumpPlanes]);
	}

	auto bsp = std::make_unique<JhlBsp>();
	bsp->MapName = mapName;

	std::vector<BspPlane> planes;
	std::vector<Vector> vertexes;
	std::vector<BspFace> faces;
	std::vector<BspEdge> edges;
	std::vector<int> surfedges;
	std::vector<dmodel_t> models;

	if (!ReadLump(data, lumps, LumpPlanes, planes) || !ReadLump(data, lumps, LumpVertexes, vertexes) ||
		!ReadLump(data, lumps, LumpTexinfo, bsp->Texinfo) || !ReadLump(data, lumps, LumpFaces, faces) ||
		!ReadLump(data, lumps, LumpEdges, edges) || !ReadLump(data, lumps, LumpSurfedges, surfedges) ||
		!ReadLump(data, lumps, LumpModels, models) || !lumpStartsEntities(LumpEntities))
	{
		return nullptr;
	}

	// Texture names, from the texture lump's headers.
	const int texturesOffset = lumps[LumpTextures][0];
	int textureCount = 0;

	if (texturesOffset >= 0 && static_cast<std::size_t>(texturesOffset) + sizeof(int) <= data.size())
	{
		std::memcpy(&textureCount, data.data() + texturesOffset, sizeof(textureCount));
	}

	if (textureCount < 0 || static_cast<std::size_t>(texturesOffset) + sizeof(int) * (1 + static_cast<std::size_t>(textureCount)) > data.size())
	{
		textureCount = 0;
	}

	bsp->TextureNames.resize(textureCount);

	for (int i = 0; i < textureCount; ++i)
	{
		int offset;
		std::memcpy(&offset, data.data() + texturesOffset + sizeof(int) * (1 + i), sizeof(offset));

		if (offset >= 0 && static_cast<std::size_t>(texturesOffset) + offset + 16 <= data.size())
		{
			char name[17]{};
			std::memcpy(name, data.data() + texturesOffset + offset, 16);
			bsp->TextureNames[i] = name;
		}
	}

	// Faces, as polygons.
	bsp->Faces.resize(faces.size());

	for (std::size_t f = 0; f < faces.size(); ++f)
	{
		const auto& source = faces[f];
		auto& face = bsp->Faces[f];

		if (source.Plane >= planes.size() || source.NumEdges < 3 || source.FirstEdge < 0 ||
			static_cast<std::size_t>(source.FirstEdge) + source.NumEdges > surfedges.size())
		{
			continue;
		}

		const auto& plane = planes[source.Plane];
		const float sign = 0 != source.Side ? -1.0f : 1.0f;
		face.Normal = Vector(plane.Normal[0], plane.Normal[1], plane.Normal[2]) * sign;
		face.Dist = plane.Dist * sign;
		face.Texinfo = source.Texinfo < bsp->Texinfo.size() ? source.Texinfo : -1;
		face.Points.reserve(source.NumEdges);

		for (int i = 0; i < source.NumEdges; ++i)
		{
			const int surfedge = surfedges[source.FirstEdge + i];
			const auto edge = static_cast<std::size_t>(std::abs(surfedge));
			const auto vertex = edge < edges.size() ? edges[edge].V[surfedge >= 0 ? 0 : 1] : vertexes.size();

			if (vertex >= vertexes.size())
			{
				face.Points.clear();
				break;
			}

			face.Points.push_back(vertexes[vertex]);
		}
	}

	bsp->Models.reserve(models.size());

	for (const auto& model : models)
	{
		const bool valid = model.firstface >= 0 && model.numfaces >= 0 &&
						   static_cast<std::size_t>(model.firstface) + model.numfaces <= faces.size();
		bsp->Models.push_back({valid ? model.firstface : 0, valid ? model.numfaces : 0});
	}

	const auto& entitiesLump = lumps[LumpEntities];
	bsp->Entities = ParseEntities(reinterpret_cast<const char*>(data.data()) + entitiesLump[0], entitiesLump[1]);
	return bsp;
}
}

const std::string& JhlBsp::TextureName(const JhlBspFace& face) const
{
	if (face.Texinfo < 0)
	{
		return NoName;
	}

	const int miptex = Texinfo[face.Texinfo].Miptex;
	return miptex >= 0 && static_cast<std::size_t>(miptex) < TextureNames.size() ? TextureNames[miptex] : NoName;
}

const JhlBsp* JhlBsp_Get(const char* mapName)
{
	if (!mapName || '\0' == *mapName)
	{
		return nullptr;
	}

	if (g_Bsp && g_Bsp->MapName == mapName)
	{
		return g_Bsp.get();
	}

	// Don't try again every frame.
	if (g_FailedMap == mapName)
	{
		return nullptr;
	}

	g_Bsp = Load(mapName);

	if (!g_Bsp)
	{
		g_FailedMap = mapName;
	}

	return g_Bsp.get();
}

Vector JhlBsp_ParseVector(const std::string& text)
{
	Vector result;
	std::sscanf(text.c_str(), "%f %f %f", &result.x, &result.y, &result.z);
	return result;
}
