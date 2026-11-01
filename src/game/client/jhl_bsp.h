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

#pragma once

#include <string>
#include <unordered_map>
#include <vector>

// johnhalflife: the map being played, read from its BSP file (version 30, Blue Shift's too) for what the engine doesn't
// hand the client: its faces as polygons, their texture mapping and texture names, its models and entity keys.

struct JhlBspTexinfo
{
	float Vecs[2][4]; // s and t: dot(point, Vecs[i]) + Vecs[i][3], in texels.
	int Miptex;
	int Flags;
};

struct JhlBspFace
{
	Vector Normal; // Facing out of the face's front, in its model's coordinates.
	float Dist = 0;
	int Texinfo = -1;
	std::vector<Vector> Points; // Its polygon, in its model's coordinates; empty if the face was unreadable.
};

struct JhlBspModel
{
	int FirstFace = 0;
	int NumFaces = 0;
};

struct JhlBsp
{
	std::string MapName;
	std::vector<JhlBspFace> Faces;
	std::vector<JhlBspTexinfo> Texinfo;
	std::vector<std::string> TextureNames; // By miptex.
	std::vector<JhlBspModel> Models;	   // 0 is the world, then *1 ...
	std::vector<std::unordered_map<std::string, std::string>> Entities;

	/**
	 *	@brief The name of a face's texture ("" if unknown).
	 */
	const std::string& TextureName(const JhlBspFace& face) const;
};

/**
 *	@brief The map (as gEngfuncs.pfnGetLevelName names it), read once and kept until another is asked for; null if it
 *	can't be read.
 */
const JhlBsp* JhlBsp_Get(const char* mapName);

/**
 *	@brief Three numbers from an entity key ("origin", "angles").
 */
Vector JhlBsp_ParseVector(const std::string& text);
