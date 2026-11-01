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

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "hud.h"
#include "utils/filesystem_utils.h"
#include "jhl_gl.h"
#include "jhl_skyboxes.h"

namespace
{
// The sky's faces, as the engine loads them (R_LoadSkys).
constexpr const char* Suffixes[6] = {"rt", "bk", "lf", "ft", "up", "dn"};

// GL texture names looked through for the sky's (the engine numbers its own from 1 up).
constexpr GLuint MostTextureNames = 32768;

// A texture is the image if its picture follows the image's this closely (correlation of their brightness).
constexpr double MatchCorrelation = 0.95;

JhlGL& gl = g_JhlGL;
cvar_t* g_SkyFiltering = nullptr;
cvar_t* g_TextureMode = nullptr;

std::string g_SkyName; // The sky the textures were looked for (with the map, so a new map looks again).
std::array<GLuint, 6> g_SkyTextures{};
GLint g_AppliedFilter = 0; // What they were given (0: nothing, as the engine left them).

struct Image
{
	int Width = 0;
	int Height = 0;
	std::vector<float> Brightness; // Row by row, top first.
};

// A TGA's brightness (uncompressed or run-length, 24 or 32 bit), as the engine reads the sky.
bool LoadTga(const std::string& path, Image& image)
{
	const auto data = FileSystem_LoadFileIntoBuffer(path.c_str(), FileContentFormat::Binary);

	if (data.size() < 18)
	{
		return false;
	}

	const auto byte = [&](std::size_t i)
	{
		return static_cast<int>(static_cast<std::uint8_t>(data[i]));
	};

	const int idLength = byte(0);
	const int type = byte(2);
	const int width = byte(12) | (byte(13) << 8);
	const int height = byte(14) | (byte(15) << 8);
	const int bits = byte(16);
	const bool topFirst = (byte(17) & 0x20) != 0;
	const int bytes = bits / 8;

	if ((type != 2 && type != 10) || (bytes != 3 && bytes != 4) || width <= 0 || height <= 0 || byte(1) != 0)
	{
		return false;
	}

	std::vector<float> pixels(static_cast<std::size_t>(width) * height);
	std::size_t at = 18 + idLength;
	std::size_t count = 0;

	const auto readPixel = [&](float& out)
	{
		if (at + bytes > data.size())
		{
			return false;
		}

		// BGR(A).
		out = 0.114f * byte(at) + 0.587f * byte(at + 1) + 0.299f * byte(at + 2);
		at += bytes;
		return true;
	};

	while (count < pixels.size())
	{
		if (type == 2)
		{
			if (!readPixel(pixels[count++]))
			{
				return false;
			}

			continue;
		}

		if (at >= data.size())
		{
			return false;
		}

		const int packet = byte(at++);
		const int run = (packet & 0x7F) + 1;

		if (packet & 0x80)
		{
			float value;

			if (!readPixel(value))
			{
				return false;
			}

			for (int i = 0; i < run && count < pixels.size(); ++i)
			{
				pixels[count++] = value;
			}
		}
		else
		{
			for (int i = 0; i < run && count < pixels.size(); ++i)
			{
				if (!readPixel(pixels[count++]))
				{
					return false;
				}
			}
		}
	}

	image.Width = width;
	image.Height = height;
	image.Brightness.resize(pixels.size());

	for (int y = 0; y < height; ++y)
	{
		const int row = topFirst ? y : height - 1 - y;
		std::memcpy(&image.Brightness[static_cast<std::size_t>(y) * width], &pixels[static_cast<std::size_t>(row) * width], sizeof(float) * width);
	}

	return true;
}

// How closely two pictures of a size follow each other (every 4th pixel each way); upside down too.
double Correlation(const Image& image, const std::vector<std::uint8_t>& rgba, bool flipped)
{
	double sa = 0, sb = 0, saa = 0, sbb = 0, sab = 0;
	int n = 0;

	for (int y = 0; y < image.Height; y += 4)
	{
		const int textureRow = flipped ? image.Height - 1 - y : y;

		for (int x = 0; x < image.Width; x += 4)
		{
			const double a = image.Brightness[static_cast<std::size_t>(y) * image.Width + x];
			const std::size_t i = (static_cast<std::size_t>(textureRow) * image.Width + x) * 4;
			const double b = 0.299 * rgba[i] + 0.587 * rgba[i + 1] + 0.114 * rgba[i + 2];
			sa += a;
			sb += b;
			saa += a * a;
			sbb += b * b;
			sab += a * b;
			++n;
		}
	}

	const double va = saa - sa * sa / n;
	const double vb = sbb - sb * sb / n;
	return va > 0 && vb > 0 ? (sab - sa * sb / n) / std::sqrt(va * vb) : 0;
}

// The engine's GL textures for the sky's six images; 0 for one not found.
void FindSkyTextures(const std::string& sky)
{
	g_SkyTextures = {};
	g_AppliedFilter = 0;

	std::array<Image, 6> images;
	bool any = false;

	for (int face = 0; face < 6; ++face)
	{
		any = LoadTga("gfx/env/" + sky + Suffixes[face] + ".tga", images[face]) || any;
	}

	if (!any)
	{
		gEngfuncs.Con_DPrintf("johnhalflife: sky %s not found for its filtering\n", sky.c_str());
		return;
	}

	GLint previous = 0;
	gl.GetIntegerv(GL_TEXTURE_BINDING_2D, &previous);

	std::vector<std::uint8_t> rgba;
	std::array<double, 6> best{};

	for (GLuint name = 1; name < MostTextureNames; ++name)
	{
		if (GL_TRUE != gl.IsTexture(name))
		{
			continue;
		}

		gl.BindTexture(GL_TEXTURE_2D, name);

		GLint width = 0, height = 0;
		gl.GetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
		gl.GetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);

		bool read = false;

		for (int face = 0; face < 6; ++face)
		{
			const auto& image = images[face];

			if (image.Width != width || image.Height != height)
			{
				continue;
			}

			if (!read)
			{
				rgba.resize(static_cast<std::size_t>(width) * height * 4);
				gl.GetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
				read = true;
			}

			const double match = std::max(Correlation(image, rgba, false), Correlation(image, rgba, true));

			if (match >= MatchCorrelation && match > best[face])
			{
				best[face] = match;
				g_SkyTextures[face] = name;
			}
		}
	}

	gl.BindTexture(GL_TEXTURE_2D, previous);

	int found = 0;

	for (const auto texture : g_SkyTextures)
	{
		found += 0 != texture ? 1 : 0;
	}

	gEngfuncs.Con_DPrintf("johnhalflife: sky %s: %d of its 6 textures found for its filtering\n", sky.c_str(), found);
}

// The filter the world's textures have, without mipmaps (the sky has none): nearest when they're nearest.
GLint WorldFilter()
{
	return 0 == strnicmp(g_TextureMode->string, "GL_NEAREST", 10) ? GL_NEAREST : GL_LINEAR;
}

void ApplyFilter(GLint filter)
{
	GLint previous = 0;
	gl.GetIntegerv(GL_TEXTURE_BINDING_2D, &previous);

	for (const auto texture : g_SkyTextures)
	{
		if (0 != texture && GL_TRUE == gl.IsTexture(texture))
		{
			gl.BindTexture(GL_TEXTURE_2D, texture);
			gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
			gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
		}
	}

	gl.BindTexture(GL_TEXTURE_2D, previous);
}
}

void JhlSkyboxes_Initialize()
{
	g_SkyFiltering = gEngfuncs.pfnRegisterVariable("jhl_visuals_skyfiltering", "1", FCVAR_ARCHIVE);
}

void JhlSkyboxes_Update()
{
	if (!g_TextureMode)
	{
		g_TextureMode = gEngfuncs.pfnGetCvarPointer("gl_texturemode");
	}

	if (!g_TextureMode || !JhlGL_Available())
	{
		return;
	}

	// Once, for developer 1: whether the view is anti-aliased (the engine multisamples by default; -nomsaa turns it off).
	static bool reportedSamples = false;

	if (!reportedSamples)
	{
		GLint samples = 0;
		gl.GetIntegerv(GL_SAMPLES, &samples);
		gEngfuncs.Con_DPrintf("johnhalflife: the view is drawn with %d samples a pixel (%s)\n", std::max(samples, 1),
			samples > 1 ? "anti-aliased; -nomsaa turns it off" : "not anti-aliased");
		reportedSamples = true;
	}

	const char* map = gEngfuncs.pfnGetLevelName();
	const char* skyName = gEngfuncs.pfnGetCvarString("sv_skyname");

	if (!map || '\0' == *map || !skyName || '\0' == *skyName)
	{
		return;
	}

	// Off: as the engine left them (smooth).
	if (0 == g_SkyFiltering->value)
	{
		if (0 != g_AppliedFilter)
		{
			ApplyFilter(GL_LINEAR);
			g_AppliedFilter = 0;
		}

		return;
	}

	if (const std::string key = std::string(map) + "|" + skyName; key != g_SkyName)
	{
		g_SkyName = key;
		FindSkyTextures(skyName);
	}

	if (const GLint filter = WorldFilter(); filter != g_AppliedFilter)
	{
		ApplyFilter(filter);
		g_AppliedFilter = filter;
	}
}
