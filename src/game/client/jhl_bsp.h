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

struct JhlBspTexinfo
{
	float Vecs[2][4];
	int Miptex;
	int Flags;
};

struct JhlBspFace
{
	Vector Normal;
	float Dist = 0;
	int Texinfo = -1;
	std::vector<Vector> Points;
	unsigned char Styles[4]{255, 255, 255, 255};
	int LightOffset = -1;
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
	std::vector<unsigned char> Lighting;
	std::vector<JhlBspModel> Models;	   // 0 is the world, then *1 ...
	std::vector<std::unordered_map<std::string, std::string>> Entities;

	const std::string& TextureName(const JhlBspFace& face) const;
};

const JhlBsp* JhlBsp_Get(const char* mapName);

Vector JhlBsp_ParseVector(const std::string& text);
