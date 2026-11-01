/***
 *
 *	johnhalflife: Valve's maps converted for the Unified SDK's game code, as its ContentInstaller does
 *	(docs/johnhalflife/release.md#first-start). Plain C++, no engine: a command line test builds it too.
 *
 ****/

#pragma once

#include <string>
#include <string_view>

enum class JhlMapGame
{
	HalfLife,
	OpposingForce,
	BlueShift
};

/**
 *	@brief Converts a map: its entities upgraded, Blue Shift's lump order made Half-Life's, the rest as it was.
 *	@param bsp The map file's contents.
 *	@param mapName The map's name without folder or extension (e.g. c2a5).
 *	@return false, with a reason in error, if it isn't a map this can convert.
 */
bool JhlMapConvert(const std::string& bsp, JhlMapGame game, std::string_view mapName, std::string& converted, std::string& error);

/**
 *	@brief A map file's entity text, in Half-Life's lump order or Blue Shift's.
 */
bool JhlMapConvert_EntityText(const std::string& bsp, std::string& text);
