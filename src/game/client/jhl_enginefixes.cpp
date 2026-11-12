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

#include <cstdint>
#include <cstring>
#include <iterator>

#include "hud.h"
#include "jhl_enginefixes.h"

#ifdef _WIN32
#include "PlatformHeaders.h"
#endif

namespace
{
#ifdef _WIN32
// GL_BuildLightmaps' test of each model in the map's list (hw.dll, 25th anniversary build):
//   cmp byte ptr [eax], '*'		; a brush entity's submodel: built with the world
//   je skip
//   cmp dword ptr [eax+0B0h], 0	; numsurfaces
//   je skip
// made
//   cmp dword ptr [eax+44h], 0		; type: mod_brush
//   jne skip
// -1 a byte that may be anything (the jump's distance).
constexpr int LightmapTest[] = {0x80, 0x38, 0x2A, 0x0F, 0x84, -1, -1, -1, -1, 0x83, 0xB8, 0xB0, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x84};
constexpr std::size_t LightmapOffsetByte = 11; // 0xB0 -> 0x44.
constexpr std::size_t LightmapJumpByte = 17;   // je (0x84) -> jne (0x85).

// The places in the module's code that match, up to MaxFound (none if more).
constexpr std::size_t MaxFound = 2;

std::size_t FindAll(HMODULE module, const int* pattern, std::size_t length, std::uint8_t* (&found)[MaxFound])
{
	const auto base = reinterpret_cast<std::uint8_t*>(module);
	const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
	const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
	const IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt);
	std::size_t count = 0;

	for (int s = 0; s < nt->FileHeader.NumberOfSections; ++s, ++section)
	{
		if ((section->Characteristics & IMAGE_SCN_MEM_EXECUTE) == 0 || section->Misc.VirtualSize < length)
			continue;

		std::uint8_t* const start = base + section->VirtualAddress;
		const std::size_t size = section->Misc.VirtualSize;

		for (std::size_t i = 0; i + length <= size; ++i)
		{
			std::size_t j = 0;

			while (j < length && (pattern[j] < 0 || start[i + j] == pattern[j]))
				++j;

			if (j == length)
			{
				if (count == MaxFound)
					return 0;

				found[count++] = start + i;
			}
		}
	}

	return count;
}

void PatchLightmapTest()
{
	const HMODULE engine = GetModuleHandleA("hw.dll");

	if (!engine)
		return; // The software renderer (sw.dll) has no lightmap blocks.

	// GL_BuildLightmaps, and a second loop over the map's models' surfaces alike (0x1024B7F0 in this build).
	std::uint8_t* found[MaxFound];
	const std::size_t count = FindAll(engine, LightmapTest, std::size(LightmapTest), found);

	if (count == 0)
	{
		gEngfuncs.Con_DPrintf("jhl_enginefixes: GL_BuildLightmaps not as expected, left alone\n");
		return;
	}

	for (std::size_t i = 0; i < count; ++i)
	{
		std::uint8_t* const code = found[i];
		DWORD old;

		if (!VirtualProtect(code, std::size(LightmapTest), PAGE_EXECUTE_READWRITE, &old))
			continue;

		code[LightmapOffsetByte] = 0x44;
		code[LightmapJumpByte] = 0x85;

		VirtualProtect(code, std::size(LightmapTest), old, &old);
		FlushInstructionCache(GetCurrentProcess(), code, std::size(LightmapTest));
	}

	gEngfuncs.Con_DPrintf("jhl_enginefixes: GL_BuildLightmaps takes brush models only (%d places)\n", static_cast<int>(count));
}
#endif
}

void JhlEngineFixes_Initialize()
{
#ifdef _WIN32
	PatchLightmapTest();
#endif
}
