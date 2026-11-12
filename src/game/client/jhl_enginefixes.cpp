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
#include <initializer_list>
#include <iterator>

#include "hud.h"
#include "com_model.h"
#include "jhl_enginefixes.h"

#ifdef _WIN32
#include "PlatformHeaders.h"
#define JHL_CDECL __cdecl
#else
#include <cstdio>
#include <elf.h>
#include <string>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>
#define JHL_CDECL __attribute__((cdecl))
#endif

namespace
{
// The places in a stretch of code that match a pattern (-1 a byte that may be anything), up to MaxFound.
constexpr std::size_t MaxFound = 2;
constexpr std::size_t TooMany = MaxFound + 1;

std::size_t FindIn(std::uint8_t* start, std::size_t size, const int* pattern, std::size_t length,
	std::uint8_t* (&found)[MaxFound], std::size_t count = 0)
{
	for (std::size_t i = 0; i + length <= size; ++i)
	{
		std::size_t j = 0;

		while (j < length && (pattern[j] < 0 || start[i + j] == pattern[j]))
			++j;

		if (j == length)
		{
			if (count == MaxFound)
				return TooMany;

			found[count++] = start + i;
		}
	}

	return count;
}

// Engine code rewritten in place.
bool WriteCode(std::uint8_t* at, const void* bytes, std::size_t size)
{
#ifdef _WIN32
	DWORD old;

	if (!VirtualProtect(at, size, PAGE_EXECUTE_READWRITE, &old))
		return false;

	std::memcpy(at, bytes, size);
	VirtualProtect(at, size, old, &old);
	FlushInstructionCache(GetCurrentProcess(), at, size);
#else
	const auto page = static_cast<std::uintptr_t>(sysconf(_SC_PAGESIZE));
	const auto first = reinterpret_cast<std::uintptr_t>(at) & ~(page - 1);
	const auto last = (reinterpret_cast<std::uintptr_t>(at) + size + page - 1) & ~(page - 1);

	if (mprotect(reinterpret_cast<void*>(first), last - first, PROT_READ | PROT_WRITE | PROT_EXEC) != 0)
		return false;

	std::memcpy(at, bytes, size);
	mprotect(reinterpret_cast<void*>(first), last - first, PROT_READ | PROT_EXEC);
#endif
	return true;
}

// A call instruction (E8 and a distance from the next instruction): where it goes, and made to go elsewhere.
std::uint8_t* CallTarget(std::uint8_t* call)
{
	std::int32_t distance;
	std::memcpy(&distance, call + 1, sizeof(distance));
	return call + 5 + distance;
}

bool Retarget(std::uint8_t* call, void* target)
{
	const auto distance = static_cast<std::int32_t>(
		reinterpret_cast<std::intptr_t>(target) - reinterpret_cast<std::intptr_t>(call + 5));
	return WriteCode(call + 1, &distance, sizeof(distance));
}

// The lightmap builder's test of each model in the map's list: a model with a surface count made a model of brush
// type (the surface count's offset made the type's, and its jump the other way round).
void PatchLightmapTests(std::uint8_t* const* sites, std::size_t count, std::size_t length, std::size_t offsetByte,
	std::size_t jumpByte)
{
	for (std::size_t i = 0; i < count; ++i)
	{
		std::uint8_t bytes[32];
		std::memcpy(bytes, sites[i], length);
		bytes[offsetByte] = 0x44;
		bytes[jumpByte] ^= 0x01; // je <-> jne
		WriteCode(sites[i], bytes, length);
	}

	gEngfuncs.Con_DPrintf("jhl_enginefixes: GL_BuildLightmaps takes brush models only (%d places)\n", static_cast<int>(count));
}

// A sprite's drawing starts with the second texture unit off: R_DrawSpriteModel sets its texture mode (glTexEnv) and
// only then turns it off, so after a brush entity drawn in rendermode 1 (Color), which leaves it on and selected, the
// mode was set on the second unit and the sprite drawn with the first's last one (GL_REPLACE): white and opaque.
using GetSpriteFrameFn = void*(JHL_CDECL*)(void* sprite, int frame);
using DisableMultitextureFn = void(JHL_CDECL*)();

GetSpriteFrameFn g_GetSpriteFrame = nullptr;
DisableMultitextureFn g_DisableMultitexture = nullptr;

void* JHL_CDECL SpriteFrameFirst(void* sprite, int frame)
{
	g_DisableMultitexture();
	return g_GetSpriteFrame(sprite, frame);
}

// call: R_DrawSpriteModel's first call, to R_GetSpriteFrame (sprite, frame).
void PatchSpriteMultitexture(std::uint8_t* call, void* disableMultitexture)
{
	g_GetSpriteFrame = reinterpret_cast<GetSpriteFrameFn>(CallTarget(call));
	g_DisableMultitexture = reinterpret_cast<DisableMultitextureFn>(disableMultitexture);

	if (Retarget(call, reinterpret_cast<void*>(&SpriteFrameFirst)))
		gEngfuncs.Con_DPrintf("jhl_enginefixes: R_DrawSpriteModel starts with the second texture unit off\n");
}

// Once the engine's table of known models (1024 places) is full (a long session: about 170 maps in one go), Mod_FindName
// gives a new name the place an earlier map's model had and copies in only the name: the old model's data stays, and
// with it its claim on a block of the model cache.

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

using StrncpyFn = char*(JHL_CDECL*)(char* dest, const char* src, int count);
using CacheFreeFn = void(JHL_CDECL*)(cache_user_t* user);

StrncpyFn g_Strncpy = nullptr;
CacheFreeFn g_CacheFree = nullptr;

// A name given a place in the table starts a clean model there. The old model's cache block is freed if it is still
// the old model's; left claimed, the engine later freed the new model's block in its place, and a map's loading could
// go round forever moving the old one (the hang of a long map sweep); its brush data misled the lightmaps as well.
char* JHL_CDECL NameNewModel(char* place, const char* name, int count)
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

// call: Mod_FindName's call to Q_strncpy (place, name, 63).
void PatchModelNames(std::uint8_t* call, void* cacheFree)
{
	g_Strncpy = reinterpret_cast<StrncpyFn>(CallTarget(call));
	g_CacheFree = reinterpret_cast<CacheFreeFn>(cacheFree);

	if (Retarget(call, reinterpret_cast<void*>(&NameNewModel)))
		gEngfuncs.Con_DPrintf("jhl_enginefixes: Mod_FindName starts a reused place in the table of models clean\n");
}

#ifdef _WIN32
// hw.dll and sw.dll (25th anniversary build) have no symbols: the places are found by their code.

std::size_t FindAll(HMODULE module, const int* pattern, std::size_t length, std::uint8_t* (&found)[MaxFound])
{
	const auto base = reinterpret_cast<std::uint8_t*>(module);
	const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
	const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
	const IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt);
	std::size_t count = 0;

	for (int s = 0; s < nt->FileHeader.NumberOfSections && count != TooMany; ++s, ++section)
	{
		if ((section->Characteristics & IMAGE_SCN_MEM_EXECUTE) == 0 || section->Misc.VirtualSize < length)
			continue;

		count = FindIn(base + section->VirtualAddress, section->Misc.VirtualSize, pattern, length, found, count);
	}

	return count;
}

// GL_BuildLightmaps' test of each model in the map's list:
//   cmp byte ptr [eax], '*'		; a brush entity's submodel: built with the world
//   je skip
//   cmp dword ptr [eax+0B0h], 0	; numsurfaces
//   je skip
// made
//   cmp dword ptr [eax+44h], 0		; type: mod_brush
//   jne skip
// Also in a second loop alike (R_ReUploadLightmaps).
constexpr int LightmapTest[] = {0x80, 0x38, 0x2A, 0x0F, 0x84, -1, -1, -1, -1, 0x83, 0xB8, 0xB0, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x84};
constexpr std::size_t LightmapOffsetByte = 11;
constexpr std::size_t LightmapJumpByte = 17;

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

// Mod_FindName giving a name its place, cdecl:
//   mov [ecx*4+...], eax
//   push 3Fh
//   push [ebp+0Ch]			; the name
//   push esi				; the place
//   call Q_strncpy			; made to call NameNewModel
//   add esp, 0Ch
//   mov byte ptr [esi+3Fh], 0
//   cmp dword ptr [esi+40h], 3
constexpr int NameCopyCall[] = {0x89, 0x04, 0x8D, -1, -1, -1, -1, 0x6A, 0x3F, 0xFF, 0x75, 0x0C, 0x56, 0xE8, -1, -1, -1, -1,
	0x83, 0xC4, 0x0C, 0xC6, 0x46, 0x3F, 0x00, 0x83, 0x7E, 0x40, 0x03};
constexpr std::size_t NameCopyCallByte = 13; // The call's E8.

// Cache_Free (cache_user_t*): its start, up to the unlinking of the block.
constexpr int CacheFree[] = {0x55, 0x8B, 0xEC, 0x56, 0x57, 0x8B, 0x7D, 0x08, 0x8B, 0x37, 0x85, 0xF6, 0x75, 0x0F, 0x68, -1, -1,
	-1, -1, 0xE8, -1, -1, -1, -1, 0x8B, 0x37, 0x83, 0xC4, 0x04, 0x8B, 0x4E, 0xF0, 0x8B, 0x46, 0xF4, 0x89, 0x41, 0x4C};

void PatchEngine()
{
	const HMODULE gl = GetModuleHandleA("hw.dll");

	if (gl)
	{
		std::uint8_t* found[MaxFound];
		const std::size_t count = FindAll(gl, LightmapTest, std::size(LightmapTest), found);

		if (count >= 1 && count <= MaxFound)
			PatchLightmapTests(found, count, std::size(LightmapTest), LightmapOffsetByte, LightmapJumpByte);
		else
			gEngfuncs.Con_DPrintf("jhl_enginefixes: GL_BuildLightmaps not as expected, left alone\n");

		std::uint8_t* calls[MaxFound];
		std::uint8_t* disables[MaxFound];

		if (FindAll(gl, SpriteFrameCall, std::size(SpriteFrameCall), calls) == 1 &&
			FindAll(gl, DisableMultitexture, std::size(DisableMultitexture), disables) == 1)
			PatchSpriteMultitexture(calls[0] + SpriteFrameCallByte, disables[0]);
		else
			gEngfuncs.Con_DPrintf("jhl_enginefixes: R_DrawSpriteModel not as expected, left alone\n");
	}

	// The software renderer (sw.dll) has no lightmap blocks or texture units, but its Mod_FindName is alike.
	const HMODULE engine = gl ? gl : GetModuleHandleA("sw.dll");

	if (!engine)
		return;

	std::uint8_t* calls[MaxFound];
	std::uint8_t* frees[MaxFound];

	if (FindAll(engine, NameCopyCall, std::size(NameCopyCall), calls) == 1 &&
		FindAll(engine, CacheFree, std::size(CacheFree), frees) == 1)
		PatchModelNames(calls[0] + NameCopyCallByte, frees[0]);
	else
		gEngfuncs.Con_DPrintf("jhl_enginefixes: Mod_FindName not as expected, left alone\n");
}
#else
// hw.so and sw.so keep their symbols (not exported): the functions are found by name in the file's symbol table, and
// each place in them by its code, and a call by where it goes.

struct Module
{
	std::uint8_t* base = nullptr;
	std::string path;
};

// The loaded library whose file is named fileName ("/hw.so"): where it starts in memory, and its file.
Module FindModule(const char* fileName)
{
	Module module;
	FILE* maps = std::fopen("/proc/self/maps", "r");

	if (!maps)
		return module;

	const std::size_t nameLength = std::strlen(fileName);
	char line[1024];

	while (std::fgets(line, sizeof(line), maps))
	{
		unsigned long start, end, offset;
		char perms[8];
		char path[768] = "";

		if (std::sscanf(line, "%lx-%lx %7s %lx %*s %*s %767[^\n]", &start, &end, perms, &offset, path) < 5)
			continue;

		const std::size_t length = std::strlen(path);

		if (offset == 0 && length >= nameLength && std::strcmp(path + length - nameLength, fileName) == 0)
		{
			module.base = reinterpret_cast<std::uint8_t*>(start);
			module.path = path;
			break;
		}
	}

	std::fclose(maps);
	return module;
}

struct Symbol
{
	std::uint8_t* address = nullptr;
	std::size_t size = 0;
};

// The functions named, from the library file's symbol table (those not found left empty).
void ReadSymbols(const Module& module, const char* const* names, Symbol* symbols, std::size_t count)
{
	FILE* file = std::fopen(module.path.c_str(), "rb");

	if (!file)
		return;

	const auto read = [file](long offset, void* to, std::size_t size)
	{
		return std::fseek(file, offset, SEEK_SET) == 0 && std::fread(to, 1, size, file) == size;
	};

	Elf32_Ehdr header;

	if (read(0, &header, sizeof(header)) && std::memcmp(header.e_ident, ELFMAG, SELFMAG) == 0 &&
		header.e_ident[EI_CLASS] == ELFCLASS32 && header.e_shentsize == sizeof(Elf32_Shdr) && header.e_shnum > 0)
	{
		std::vector<Elf32_Shdr> sections(header.e_shnum);

		if (read(header.e_shoff, sections.data(), sections.size() * sizeof(Elf32_Shdr)))
		{
			for (const Elf32_Shdr& section : sections)
			{
				if (section.sh_type != SHT_SYMTAB || section.sh_link >= sections.size() || section.sh_entsize != sizeof(Elf32_Sym))
					continue;

				const Elf32_Shdr& stringSection = sections[section.sh_link];
				std::vector<Elf32_Sym> table(section.sh_size / sizeof(Elf32_Sym));
				std::vector<char> strings(stringSection.sh_size + 1, '\0');

				if (!read(section.sh_offset, table.data(), table.size() * sizeof(Elf32_Sym)) ||
					!read(stringSection.sh_offset, strings.data(), stringSection.sh_size))
					continue;

				for (const Elf32_Sym& symbol : table)
				{
					if (ELF32_ST_TYPE(symbol.st_info) != STT_FUNC || symbol.st_value == 0 || symbol.st_name >= stringSection.sh_size)
						continue;

					for (std::size_t i = 0; i < count; ++i)
					{
						if (!symbols[i].address && std::strcmp(strings.data() + symbol.st_name, names[i]) == 0)
						{
							symbols[i].address = module.base + symbol.st_value;
							symbols[i].size = symbol.st_size;
						}
					}
				}
			}
		}
	}

	std::fclose(file);
}

// The one call in a function to target (none if not exactly one).
std::uint8_t* FindCall(const Symbol& function, const Symbol& target)
{
	if (!function.address || !target.address || function.size < 5)
		return nullptr;

	std::uint8_t* call = nullptr;

	for (std::size_t i = 0; i + 5 <= function.size; ++i)
	{
		std::uint8_t* const at = function.address + i;

		if (*at == 0xE8 && CallTarget(at) == target.address)
		{
			if (call)
				return nullptr;

			call = at;
		}
	}

	return call;
}

// GL_BuildLightmaps' (and R_ReUploadLightmaps') test of each model in the map's list, GCC's way:
//   cmp byte ptr [eax], '*'		; a brush entity's submodel: built with the world
//   je skip
//   mov edi, [eax+0B0h]			; numsurfaces
//   test edi, edi
//   je skip
// made
//   mov edi, [eax+44h]				; type: mod_brush
//   test edi, edi
//   jne skip
constexpr int LightmapTest[] = {0x80, 0x38, 0x2A, 0x74, -1, 0x8B, -1, 0xB0, 0x00, 0x00, 0x00, 0x85, -1, 0x74};
constexpr std::size_t LightmapOffsetByte = 7;
constexpr std::size_t LightmapJumpByte = 13;

enum
{
	ModFindName,
	QStrncpy,
	CacheFreeFunction,
	DrawSpriteModel,
	GetSpriteFrame,
	DisableMultitextureFunction,
	BuildLightmaps,
	ReUploadLightmaps,
	SymbolCount
};

constexpr const char* SymbolNames[SymbolCount] = {"Mod_FindName", "Q_strncpy", "Cache_Free", "R_DrawSpriteModel",
	"R_GetSpriteFrame", "GL_DisableMultitexture", "GL_BuildLightmaps", "R_ReUploadLightmaps"};

void PatchEngine()
{
	Module engine = FindModule("/hw.so");
	const bool gl = engine.base != nullptr;

	if (!gl)
		engine = FindModule("/sw.so");

	if (!engine.base)
		return;

	Symbol symbols[SymbolCount];
	ReadSymbols(engine, SymbolNames, symbols, SymbolCount);

	if (gl)
	{
		std::uint8_t* sites[2];
		std::size_t count = 0;

		for (const int function : {BuildLightmaps, ReUploadLightmaps})
		{
			std::uint8_t* found[MaxFound];

			if (symbols[function].address &&
				FindIn(symbols[function].address, symbols[function].size, LightmapTest, std::size(LightmapTest), found) == 1)
				sites[count++] = found[0];
		}

		if (count == 2)
			PatchLightmapTests(sites, count, std::size(LightmapTest), LightmapOffsetByte, LightmapJumpByte);
		else
			gEngfuncs.Con_DPrintf("jhl_enginefixes: GL_BuildLightmaps not as expected, left alone\n");

		std::uint8_t* const call = FindCall(symbols[DrawSpriteModel], symbols[GetSpriteFrame]);

		if (call && symbols[DisableMultitextureFunction].address)
			PatchSpriteMultitexture(call, symbols[DisableMultitextureFunction].address);
		else
			gEngfuncs.Con_DPrintf("jhl_enginefixes: R_DrawSpriteModel not as expected, left alone\n");
	}

	std::uint8_t* const call = FindCall(symbols[ModFindName], symbols[QStrncpy]);

	if (call && symbols[CacheFreeFunction].address)
		PatchModelNames(call, symbols[CacheFreeFunction].address);
	else
		gEngfuncs.Con_DPrintf("jhl_enginefixes: Mod_FindName not as expected, left alone\n");
}
#endif
}

void JhlEngineFixes_Initialize()
{
	PatchEngine();
}
