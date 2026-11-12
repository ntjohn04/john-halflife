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

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>

#include "hud.h"
#include "com_model.h"
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

// R_DrawSpriteModel's first call, to R_GetSpriteFrame (sprite, frame), cdecl:
//   mov eax, [esi+0B94h]		; the entity's model
//   mov ebx, [eax+184h]		; its sprite
//   cvttss2si eax, [esi+2E0h]	; its frame
//   push eax
//   push ebx
//   call R_GetSpriteFrame		; made to call SpriteFrameFirst
//   mov edi, eax
//   add esp, 8
//   test edi, edi
constexpr int SpriteFrameCall[] = {0x8B, 0x86, 0x94, 0x0B, 0x00, 0x00, 0x8B, 0x98, 0x84, 0x01, 0x00, 0x00, 0xF3, 0x0F, 0x2C,
	0x86, 0xE0, 0x02, 0x00, 0x00, 0x50, 0x53, 0xE8, -1, -1, -1, -1, 0x8B, 0xF8, 0x83, 0xC4, 0x08, 0x85, 0xFF};
constexpr std::size_t SpriteFrameCallByte = 22; // The call's E8.

// GL_DisableMultitexture: the second texture unit off and the first selected, if it was on.
//   cmp dword ptr [mtexenabled], 0
//   je done
//   push GL_TEXTURE_2D
//   call [qglDisable]
//   push [TEXTURE0_SGIS]
//   call GL_SelectTexture
//   add esp, 4
//   mov dword ptr [mtexenabled], 0
// done:
//   ret
constexpr int DisableMultitexture[] = {0x83, 0x3D, -1, -1, -1, -1, 0x00, 0x74, 0x23, 0x68, 0xE1, 0x0D, 0x00, 0x00, 0xFF, 0x15,
	-1, -1, -1, -1, 0xFF, 0x35, -1, -1, -1, -1, 0xE8, -1, -1, -1, -1, 0x83, 0xC4, 0x04, 0xC7, 0x05, -1, -1, -1, -1, 0x00, 0x00,
	0x00, 0x00, 0xC3};

using GetSpriteFrameFn = void*(__cdecl*)(void* sprite, int frame);
using DisableMultitextureFn = void(__cdecl*)();

GetSpriteFrameFn g_GetSpriteFrame = nullptr;
DisableMultitextureFn g_DisableMultitexture = nullptr;

// A sprite's drawing starts with the second texture unit off: R_DrawSpriteModel sets its texture mode (glTexEnv) and
// only then turns it off, so after a brush entity drawn in rendermode 1 (Color), which leaves it on and selected, the
// mode was set on the second unit and the sprite drawn with the first's last one (GL_REPLACE): white and opaque.
void* __cdecl SpriteFrameFirst(void* sprite, int frame)
{
	g_DisableMultitexture();
	return g_GetSpriteFrame(sprite, frame);
}

void PatchSpriteMultitexture()
{
	const HMODULE engine = GetModuleHandleA("hw.dll");

	if (!engine)
		return;

	std::uint8_t* calls[MaxFound];
	std::uint8_t* disables[MaxFound];

	if (FindAll(engine, SpriteFrameCall, std::size(SpriteFrameCall), calls) != 1 ||
		FindAll(engine, DisableMultitexture, std::size(DisableMultitexture), disables) != 1)
	{
		gEngfuncs.Con_DPrintf("jhl_enginefixes: R_DrawSpriteModel not as expected, left alone\n");
		return;
	}

	std::uint8_t* const call = calls[0] + SpriteFrameCallByte;
	std::int32_t target;
	std::memcpy(&target, call + 1, sizeof(target));

	g_GetSpriteFrame = reinterpret_cast<GetSpriteFrameFn>(call + 5 + target);
	g_DisableMultitexture = reinterpret_cast<DisableMultitextureFn>(disables[0]);

	DWORD old;

	if (!VirtualProtect(call, 5, PAGE_EXECUTE_READWRITE, &old))
		return;

	const std::int32_t first = static_cast<std::int32_t>(
		reinterpret_cast<std::intptr_t>(&SpriteFrameFirst) - reinterpret_cast<std::intptr_t>(call + 5));
	std::memcpy(call + 1, &first, sizeof(first));

	VirtualProtect(call, 5, old, &old);
	FlushInstructionCache(GetCurrentProcess(), call, 5);
	gEngfuncs.Con_DPrintf("jhl_enginefixes: R_DrawSpriteModel starts with the second texture unit off\n");
}

// Mod_FindName giving a model name its place in the engine's table of known models (1024 places), cdecl:
//   mov [ecx*4+...], eax
//   push 3Fh
//   push [ebp+0Ch]			; the name
//   push esi				; the place
//   call Q_strncpy			; made to call NameNewModel
//   add esp, 0Ch
//   mov byte ptr [esi+3Fh], 0
//   cmp dword ptr [esi+40h], 3
// Once the table is full (a long session: about 170 maps in one go) the place is one an earlier map's model had, and
// the engine copies only the name: the old model's data stays, and with it its claim on a block of the model cache.
constexpr int NameCopyCall[] = {0x89, 0x04, 0x8D, -1, -1, -1, -1, 0x6A, 0x3F, 0xFF, 0x75, 0x0C, 0x56, 0xE8, -1, -1, -1, -1,
	0x83, 0xC4, 0x0C, 0xC6, 0x46, 0x3F, 0x00, 0x83, 0x7E, 0x40, 0x03};
constexpr std::size_t NameCopyCallByte = 13; // The call's E8.

// Cache_Free (cache_user_t*): its start, up to the unlinking of the block.
constexpr int CacheFree[] = {0x55, 0x8B, 0xEC, 0x56, 0x57, 0x8B, 0x7D, 0x08, 0x8B, 0x37, 0x85, 0xF6, 0x75, 0x0F, 0x68, -1, -1,
	-1, -1, 0xE8, -1, -1, -1, -1, 0x8B, 0x37, 0x83, 0xC4, 0x04, 0x8B, 0x4E, 0xF0, 0x8B, 0x46, 0xF4, 0x89, 0x41, 0x4C};

// A block of the model cache (cache_system_t) as the engine lays it out; the model's data follows it.
struct CacheBlock
{
	int size;
	cache_user_t* user;
	char name[64];
	CacheBlock* prev;
	CacheBlock* next;
	CacheBlock* lruPrev;
	CacheBlock* lruNext;
};

static_assert(sizeof(CacheBlock) == 0x58);
static_assert(sizeof(model_t) == 0x188 && offsetof(model_t, cache) == 0x184);

using StrncpyFn = char*(__cdecl*)(char* dest, const char* src, int count);
using CacheFreeFn = void(__cdecl*)(cache_user_t* user);

StrncpyFn g_Strncpy = nullptr;
CacheFreeFn g_CacheFree = nullptr;

// A name given a place in the table starts a clean model there. The old model's cache block is freed if it is still
// the old model's; left claimed, the engine later freed the new model's block in its place, and a map's loading could
// go round forever moving the old one (the hang of a long map sweep); its brush data misled the lightmaps as well.
char* __cdecl NameNewModel(char* place, const char* name, int count)
{
	auto model = reinterpret_cast<model_t*>(place);

	if (model->cache.data)
	{
		const auto block = static_cast<CacheBlock*>(model->cache.data) - 1;

		if (block->user == &model->cache && block->lruPrev && block->lruNext)
			g_CacheFree(&model->cache);
	}

	if (model->name[0] != '\0')
		gEngfuncs.Con_DPrintf("jhl_enginefixes: the place of %s in the table of models now %s\n", model->name, name);

	std::memset(model, 0, sizeof(*model));
	return g_Strncpy(place, name, count);
}

void PatchModelNames()
{
	// Mod_FindName is alike in both renderers.
	HMODULE engine = GetModuleHandleA("hw.dll");

	if (!engine)
		engine = GetModuleHandleA("sw.dll");

	if (!engine)
		return;

	std::uint8_t* calls[MaxFound];
	std::uint8_t* frees[MaxFound];

	if (FindAll(engine, NameCopyCall, std::size(NameCopyCall), calls) != 1 ||
		FindAll(engine, CacheFree, std::size(CacheFree), frees) != 1)
	{
		gEngfuncs.Con_DPrintf("jhl_enginefixes: Mod_FindName not as expected, left alone\n");
		return;
	}

	std::uint8_t* const call = calls[0] + NameCopyCallByte;
	std::int32_t target;
	std::memcpy(&target, call + 1, sizeof(target));

	g_Strncpy = reinterpret_cast<StrncpyFn>(call + 5 + target);
	g_CacheFree = reinterpret_cast<CacheFreeFn>(frees[0]);

	DWORD old;

	if (!VirtualProtect(call, 5, PAGE_EXECUTE_READWRITE, &old))
		return;

	const std::int32_t name = static_cast<std::int32_t>(
		reinterpret_cast<std::intptr_t>(&NameNewModel) - reinterpret_cast<std::intptr_t>(call + 5));
	std::memcpy(call + 1, &name, sizeof(name));

	VirtualProtect(call, 5, old, &old);
	FlushInstructionCache(GetCurrentProcess(), call, 5);
	gEngfuncs.Con_DPrintf("jhl_enginefixes: Mod_FindName starts a reused place in the table of models clean\n");
}
#endif
}

void JhlEngineFixes_Initialize()
{
#ifdef _WIN32
	PatchLightmapTest();
	PatchSpriteMultitexture();
	PatchModelNames();
#endif
}
