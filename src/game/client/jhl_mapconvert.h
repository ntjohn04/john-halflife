#pragma once

#include <string>
#include <string_view>

enum class JhlMapGame
{
	HalfLife,
	OpposingForce,
	BlueShift
};

bool JhlMapConvert(const std::string& bsp, JhlMapGame game, std::string_view mapName, std::string& converted, std::string& error);

bool JhlMapConvert_EntityText(const std::string& bsp, std::string& text);
