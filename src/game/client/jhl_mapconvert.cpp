/***
 *
 *	johnhalflife: Valve's maps converted for the Unified SDK's game code (docs/release.md#first-start).
 *
 *	The Unified SDK's ContentInstaller rewrites each map's entities with its map upgrades
 *	(HalfLife.UnifiedSdk-CSharp, MapUpgrader.Upgrades, version 1.0.0): old class names and keyvalues into the ones the
 *	game code reads, and fixes to particular maps. This is that, in the same order with the same results: an entity's
 *	keyvalues kept in order, one changed in place, a new one added at the end, numbers written as .NET writes them.
 *	Checked against the ContentInstaller's own output, map by map (scripts/mapconvert_check.cpp): 215 of the 220 maps
 *	the same, byte for byte. The others keep what the ContentInstaller's BSP reader lost: bytes outside ASCII in the
 *	wad lists of c1a3d, op4_kbase and ba_tram2 (it wrote '?'), the target of c1a4i's door testroom1 (the vanilla map has
 *	it, after the class name); and op4_disposal's shotgun is 1.4142133 units up, not 1.4142134 (its cosine was one bit
 *	off).
 *
 *	Only the entities change; Blue Shift's maps, which list their first two lumps the other way round, get Half-Life's
 *	order. Everything else in the file is Valve's, as it was.
 *
 ****/

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "jhl_mapconvert.h"

namespace
{
// Numbers as .NET reads and writes them (invariant culture).

bool IsDotNetSpace(char c)
{
	return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}

std::string_view Trim(std::string_view text)
{
	while (!text.empty() && IsDotNetSpace(text.front()))
		text.remove_prefix(1);
	while (!text.empty() && IsDotNetSpace(text.back()))
		text.remove_suffix(1);
	return text;
}

// int.TryParse: spaces around, a sign, digits.
bool TryParseInt(std::string_view text, int& value)
{
	text = Trim(text);

	if (!text.empty() && text.front() == '+')
		text.remove_prefix(1);

	if (text.empty())
		return false;

	const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
	return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

template <typename T>
bool TryParseReal(std::string_view text, T& value)
{
	text = Trim(text);

	if (!text.empty() && text.front() == '+')
		text.remove_prefix(1);

	if (text.empty())
		return false;

	const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
	return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

// float.ToString() and double.ToString(): the shortest text that reads back the same, plain from 1e-5 up to 1e15,
// otherwise as 1.5E+15.
template <typename T>
std::string FormatReal(T value)
{
	if (value == 0)
		return std::signbit(value) ? "-0" : "0";

	char buffer[64];
	const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::scientific);
	std::string scientific(buffer, result.ptr);

	const bool negative = scientific.front() == '-';
	if (negative)
		scientific.erase(0, 1);

	const std::size_t e = scientific.find('e');
	const int exponent = std::stoi(scientific.substr(e + 1));
	std::string digits = scientific.substr(0, e);
	digits.erase(std::remove(digits.begin(), digits.end(), '.'), digits.end());

	std::string text;

	if (exponent >= -5 && exponent < 15)
	{
		if (exponent < 0)
		{
			text = "0." + std::string(-exponent - 1, '0') + digits;
		}
		else if (static_cast<std::size_t>(exponent) + 1 >= digits.size())
		{
			text = digits + std::string(exponent + 1 - digits.size(), '0');
		}
		else
		{
			text = digits.substr(0, exponent + 1) + "." + digits.substr(exponent + 1);
		}
	}
	else
	{
		text = digits.substr(0, 1);
		if (digits.size() > 1)
			text += "." + digits.substr(1);

		char exponentText[16];
		std::snprintf(exponentText, sizeof(exponentText), "E%c%02d", exponent < 0 ? '-' : '+', std::abs(exponent));
		text += exponentText;
	}

	return negative ? "-" + text : text;
}

struct Vector3
{
	float X = 0, Y = 0, Z = 0;

	Vector3 operator+(const Vector3& other) const { return {X + other.X, Y + other.Y, Z + other.Z}; }
	Vector3 operator*(float scale) const { return {X * scale, Y * scale, Z * scale}; }
};

// Components split on runs of spaces, as Regex.Split(value, @"\s+"): space at either end makes an empty one.
Vector3 ParseVector3(std::string_view text)
{
	std::vector<std::string_view> parts;
	std::size_t start = 0;

	for (std::size_t i = 0; i <= text.size(); ++i)
	{
		if (i == text.size() || IsDotNetSpace(text[i]))
		{
			parts.push_back(text.substr(start, i - start));

			while (i < text.size() && IsDotNetSpace(text[i]))
				++i;

			start = i;
		}
	}

	float values[3]{};

	for (std::size_t i = 0; i < parts.size() && i < 3; ++i)
	{
		if (!TryParseReal(parts[i], values[i]))
			values[i] = 0;
	}

	return {values[0], values[1], values[2]};
}

std::string FormatVector3(const Vector3& v)
{
	return FormatReal(v.X) + " " + FormatReal(v.Y) + " " + FormatReal(v.Z);
}

// An entity: its keyvalues in order, the class name among them.
class Entity
{
public:
	std::vector<std::pair<std::string, std::string>> KeyValues;

	const std::string* Find(std::string_view key) const
	{
		for (const auto& kv : KeyValues)
		{
			if (kv.first == key)
				return &kv.second;
		}

		return nullptr;
	}

	bool Has(std::string_view key) const { return Find(key) != nullptr; }

	std::string GetString(std::string_view key, std::string_view defaultValue = {}) const
	{
		const auto value = Find(key);
		return value ? *value : std::string(defaultValue);
	}

	int GetInteger(std::string_view key, int defaultValue = 0) const
	{
		int value;
		const auto text = Find(key);
		return text && TryParseInt(*text, value) ? value : defaultValue;
	}

	double GetDouble(std::string_view key, double defaultValue = 0) const
	{
		double value;
		const auto text = Find(key);
		return text && TryParseReal(*text, value) ? value : defaultValue;
	}

	Vector3 GetVector3(std::string_view key) const
	{
		const auto text = Find(key);
		return text ? ParseVector3(*text) : Vector3{};
	}

	// Changes the first with this key, or adds it at the end.
	void Set(std::string_view key, std::string value)
	{
		for (auto& kv : KeyValues)
		{
			if (kv.first == key)
			{
				kv.second = std::move(value);
				return;
			}
		}

		KeyValues.emplace_back(std::string(key), std::move(value));
	}

	void SetInteger(std::string_view key, int value) { Set(key, std::to_string(value)); }
	void SetDouble(std::string_view key, double value) { Set(key, FormatReal(value)); }
	void SetVector3(std::string_view key, const Vector3& value) { Set(key, FormatVector3(value)); }

	// Removes every one with this key.
	void Remove(std::string_view key)
	{
		std::erase_if(KeyValues, [&](const auto& kv)
			{ return kv.first == key; });
	}

	std::string ClassName() const { return GetString("classname"); }
	void SetClassName(std::string name) { Set("classname", std::move(name)); }
	std::string TargetName() const { return GetString("targetname"); }
	std::string Target() const { return GetString("target"); }
	std::string Model() const { return GetString("model"); }
	int SpawnFlags() const { return GetInteger("spawnflags"); }
	void SetSpawnFlags(int flags) { SetInteger("spawnflags", flags); }
};

class EntityList
{
public:
	std::vector<std::unique_ptr<Entity>> Entities;

	Entity& Worldspawn() { return *Entities.front(); }

	// A snapshot, safe to change the list while going through it.
	std::vector<Entity*> All() const
	{
		std::vector<Entity*> all;
		for (const auto& entity : Entities)
			all.push_back(entity.get());
		return all;
	}

	std::vector<Entity*> OfClass(std::string_view className) const
	{
		std::vector<Entity*> found;
		for (const auto& entity : Entities)
		{
			if (entity->ClassName() == className)
				found.push_back(entity.get());
		}
		return found;
	}

	template <typename Predicate>
	Entity* First(Predicate predicate) const
	{
		for (const auto& entity : Entities)
		{
			if (predicate(*entity))
				return entity.get();
		}
		return nullptr;
	}

	// The first with this targetname (worldspawn, with none, matches an empty one).
	Entity* Find(std::string_view targetName) const
	{
		return First([&](const Entity& e)
			{ return e.TargetName() == targetName; });
	}

	Entity& Create(std::string className)
	{
		auto& entity = *Entities.emplace_back(std::make_unique<Entity>());
		entity.KeyValues.emplace_back("classname", std::move(className));
		return entity;
	}

	void Remove(const Entity* entity)
	{
		std::erase_if(Entities, [&](const auto& e)
			{ return e.get() == entity; });
	}

	std::string UniqueTargetName(const std::string& baseName) const
	{
		for (int number = 0;; ++number)
		{
			const std::string name = baseName + std::to_string(number);

			if (!Find(name))
				return name;
		}
	}
};

struct Context
{
	EntityList& Entities;
	JhlMapGame Game;
	std::string_view MapName;

	const char* ModDirectory() const
	{
		switch (Game)
		{
		case JhlMapGame::OpposingForce:
			return "gearbox";
		case JhlMapGame::BlueShift:
			return "bshift";
		default:
			return "valve";
		}
	}
};

using Remap = std::vector<std::pair<std::string, std::string>>;

const std::string* Lookup(const Remap& remap, std::string_view key)
{
	for (const auto& [from, to] : remap)
	{
		if (from == key)
			return &to;
	}
	return nullptr;
}

// ScriptedSequenceUtilities.RenameAnimations: scripts whose target is this NPC class or uses this model.
void RenameAnimations(Context& context, const char* npcName, std::string_view modelName, const Remap& remap)
{
	for (Entity* script : context.Entities.OfClass("scripted_sequence"))
	{
		if (!script->Has("m_iszEntity"))
			continue;

		const Entity* target = context.Entities.Find(script->GetString("m_iszEntity"));

		if (!target)
			continue;

		if ((!npcName || target->ClassName() != npcName) && target->Model() != modelName)
			continue;

		for (const char* key : {"m_iszIdle", "m_iszPlay"})
		{
			if (const auto animation = script->Find(key))
			{
				if (const auto replacement = Lookup(remap, *animation))
					script->Set(key, *replacement);
			}
		}
	}
}

const Vector3 HullMin{-16, -16, -36};
const Vector3 HullMax{16, 16, 36};

// MathF.Sin and MathF.Cos as the ContentInstaller (64-bit) got them: correctly rounded, which a 32-bit sinf isn't always.
float Sin(float angle)
{
	return static_cast<float>(std::sin(static_cast<double>(angle)));
}

float Cos(float angle)
{
	return static_cast<float>(std::cos(static_cast<double>(angle)));
}

// MathUtilities.AngleVectors, up only, in single precision as MathF.
Vector3 AngleUp(const Vector3& angles)
{
	constexpr float ToRadians = 3.14159265f * 2 / 360;

	float angle = angles.Y * ToRadians;
	const float sy = Sin(angle);
	const float cy = Cos(angle);
	angle = angles.X * ToRadians;
	const float sp = Sin(angle);
	const float cp = Cos(angle);
	angle = angles.Z * ToRadians;
	const float sr = Sin(angle);
	const float cr = Cos(angle);

	return {(cr * sp * cy) + (-sr * -sy), (cr * sp * sy) + (-sr * cy), cr * cp};
}

// ---- Applied to all maps ----

void ConvertAngleToAngles(Context& context)
{
	for (Entity* entity : context.Entities.All())
	{
		if (!entity->Has("angle"))
			continue;

		const auto angle = static_cast<float>(entity->GetDouble("angle"));

		if (angle >= 0)
		{
			auto angles = entity->GetVector3("angles");
			angles.Y = angle;
			entity->SetVector3("angles", angles);
		}
		else
		{
			Vector3 angles;
			angles.X = std::floor(angle) == -1 ? -90.f : 90.f;
			entity->SetVector3("angles", angles);
		}

		entity->Remove("angle");
	}
}

const Remap ClassNameRemap{
	{"weapon_glock", "weapon_9mmhandgun"},
	{"ammo_glockclip", "ammo_9mmclip"},
	{"weapon_mp5", "weapon_9mmar"},
	{"ammo_mp5clip", "ammo_9mmar"},
	{"ammo_mp5grenades", "ammo_argrenades"},
	{"weapon_python", "weapon_357"},
	{"weapon_shockroach", "weapon_shockrifle"},
	{"weapon_9mmAR", "weapon_9mmar"},
	{"ammo_9mmAR", "ammo_9mmar"},
	{"ammo_ARgrenades", "ammo_argrenades"},
	{"monster_ShockTrooper_dead", "monster_shocktrooper_dead"},
};

void RenameEntityClassNames(Context& context)
{
	for (Entity* entity : context.Entities.All())
	{
		if (const auto className = Lookup(ClassNameRemap, entity->ClassName()))
			entity->SetClassName(*className);

		if (entity->ClassName() == "game_player_equip")
		{
			const auto keyValues = entity->KeyValues;

			for (const auto& [key, value] : keyValues)
			{
				if (key == "classname")
					continue;

				if (const auto className = Lookup(ClassNameRemap, key))
				{
					entity->Remove(key);
					entity->Set(*className, value);
				}
			}
		}
	}
}

void ConvertWorldItemsToItem(Context& context)
{
	for (Entity* entity : context.Entities.OfClass("world_items"))
	{
		const char* className = nullptr;

		switch (entity->GetInteger("type"))
		{
		case 42: className = "item_antidote"; break;
		case 43: className = "item_security"; break;
		case 44: className = "item_battery"; break;
		case 45: className = "item_suit"; break;
		}

		if (className)
		{
			entity->SetClassName(className);
			entity->Remove("type");
		}
		else
		{
			context.Entities.Remove(entity);
		}
	}
}

void AdjustShotgunAngles(Context& context)
{
	if (context.Game != JhlMapGame::OpposingForce && context.Game != JhlMapGame::BlueShift)
		return;

	auto updateAngles = [](Entity& entity, const Vector3& adjust)
	{
		entity.SetVector3("angles", entity.GetVector3("angles") + adjust);
	};

	auto updateOrigin = [](Entity& entity, Vector3 origin)
	{
		origin = origin + AngleUp(entity.GetVector3("angles")) * -2;
		entity.SetVector3("origin", origin);
	};

	for (Entity* entity : context.Entities.OfClass("weapon_shotgun"))
	{
		updateAngles(*entity, {0, 180, 90});
		updateOrigin(*entity, entity->GetVector3("origin"));
	}

	if (context.MapName == "ba_security2")
	{
		for (Entity* script : context.Entities.OfClass("scripted_sequence"))
		{
			if (!script->Has("m_iszEntity"))
				continue;

			Entity* shotgun = context.Entities.Find(script->GetString("m_iszEntity"));

			if (!shotgun || shotgun->ClassName() != "monster_generic" || shotgun->Model() != "models/w_shotgun.mdl")
				continue;

			shotgun->SetClassName("item_generic");
			shotgun->SetSpawnFlags(0);
			updateAngles(*shotgun, {270, 180, 0});
			updateOrigin(*shotgun, script->GetVector3("origin"));
			context.Entities.Remove(script);
		}
	}
}

void ConvertOtisModel(Context& context)
{
	for (Entity* otis : context.Entities.OfClass("monster_otis"))
	{
		const int head = otis->GetInteger("head");
		otis->Remove("head");
		otis->Remove("body");
		otis->Remove("sleeves");
		otis->Remove("item");
		otis->SetInteger("skin", head);
		otis->SetInteger("sleeves", head == 1 ? 1 : 0);
	}

	for (Entity* otis : context.Entities.OfClass("monster_otis_dead"))
	{
		if (!otis->Has("body"))
			continue;

		const int body = otis->GetInteger("body");
		otis->SetInteger("bodystate", body == 1 ? 2 : body == 2 ? 0 : 1);

		if (body == 2)
			otis->SetInteger("item", 2);

		otis->Remove("body");
	}

	if (context.Game == JhlMapGame::BlueShift)
	{
		for (Entity* entity : context.Entities.All())
		{
			if (entity->Model() != "models/otis.mdl")
				continue;

			entity->SetInteger("body", entity->GetInteger("body") == 2 ? 13 : 1);
			entity->SetInteger("skin", 1);
		}

		for (Entity* entity : context.Entities.All())
		{
			if (entity->Model() != "models/intro_otis.mdl")
				continue;

			entity->Set("model", "models/otis.mdl");
			entity->SetInteger("body", 9);
			entity->SetInteger("skin", 1);
		}
	}
}

void RenameMessages(Context& context)
{
	static const std::regex Patterns[] = {
		std::regex("^CR[0-9]+$"), std::regex("^END[0-9]+$"), std::regex("^GAMEOVER$"), std::regex("^TRAITOR$"),
		std::regex("^LOSER$"), std::regex("^GAMETITLE$"), std::regex("^T0A0TITLE$"), std::regex("^HZBUTTON1$"),
		std::regex("^HZBARNEY$")};

	std::string prefix = context.ModDirectory();
	std::transform(prefix.begin(), prefix.end(), prefix.begin(), [](unsigned char c)
		{ return static_cast<char>(std::toupper(c)); });
	prefix += '_';

	auto check = [&](Entity& entity, std::string_view key)
	{
		const auto message = entity.Find(key);

		if (message && std::any_of(std::begin(Patterns), std::end(Patterns), [&](const std::regex& p)
						   { return std::regex_match(*message, p); }))
		{
			entity.Set(key, prefix + *message);
		}
	};

	for (Entity* entity : context.Entities.All())
	{
		const auto className = entity->ClassName();

		if (className == "env_message" || className == "player_loadsaved")
			check(*entity, "message");
	}

	check(context.Entities.Worldspawn(), "chaptertitle");
}

// GameMedia.TrackToMusicMap: CD tracks to the soundtrack's files.
const char* const TrackMusic[] = {"", "", "Half-Life01", "Prospero01", "Half-Life12", "Half-Life07", "Half-Life10",
	"Suspense01", "Suspense03", "Half-Life09", "Half-Life02", "Half-Life13", "Half-Life04", "Half-Life15", "Half-Life14",
	"Half-Life16", "Suspense02", "Half-Life03", "Half-Life08", "Prospero02", "Half-Life05", "Prospero04", "Half-Life11",
	"Half-Life06", "Prospero03", "Half-Life17", "Prospero05", "Suspense05", "Suspense07"};

// The tracks Opposing Force's and Blue Shift's soundtracks have of Half-Life's names (theirs, renamed).
const char* const ExpansionMusic[] = {"Half-Life01", "Half-Life02", "Half-Life03", "Half-Life04", "Half-Life05",
	"Half-Life07", "Half-Life08", "Half-Life09", "Half-Life10", "Half-Life12", "Half-Life13", "Half-Life14",
	"Half-Life15", "Half-Life16", "Prospero01", "Prospero02", "Suspense01", "Suspense02", "Suspense03"};

std::string MusicFileName(int track, JhlMapGame game)
{
	if (track < 0 || track >= static_cast<int>(std::size(TrackMusic)))
		return {};

	std::string name = TrackMusic[track];

	if (game != JhlMapGame::HalfLife &&
		std::any_of(std::begin(ExpansionMusic), std::end(ExpansionMusic), [&](const char* n)
			{ return name == n; }))
	{
		const std::string prefix = game == JhlMapGame::OpposingForce ? "Opposing-Force" : "Blue-Shift";

		if (name.starts_with("Half-Life"))
			name = prefix + name.substr(std::strlen("Half-Life"));
		else
			name = prefix + name;
	}

	return "media/" + name + ".mp3";
}

void ReworkMusicPlayback(Context& context)
{
	enum
	{
		Play = 0,
		Stop = 3
	};

	enum
	{
		AllPlayers = 0,
		Radius = 2
	};

	auto update = [&](Entity& trigger, Entity& music, int targetSelector)
	{
		const int track = trigger.GetInteger("health", 0);
		trigger.Remove("health");

		if (track == -1)
		{
			music.SetInteger("command", Stop);
			trigger.Remove("filename");
		}
		else
		{
			music.SetInteger("command", Play);
			music.Set("filename", MusicFileName(track, context.Game));
		}

		music.SetInteger("target_selector", targetSelector);
		music.SetInteger("remove_on_fire", 1);
	};

	for (Entity* target : context.Entities.OfClass("target_cdaudio"))
	{
		target->SetClassName("ambient_music");
		update(*target, *target, Radius);
	}

	for (Entity* trigger : context.Entities.OfClass("trigger_cdaudio"))
	{
		Entity* music;

		if (!Trim(trigger->TargetName()).empty())
		{
			trigger->SetClassName("ambient_music");
			music = trigger;
			trigger->Remove("model");
		}
		else
		{
			trigger->SetClassName("trigger_once");
			music = &context.Entities.Create("ambient_music");
			const auto targetName = context.Entities.UniqueTargetName("music");
			music->Set("targetname", targetName);
			trigger->Set("target", targetName);
		}

		update(*trigger, *music, AllPlayers);
	}

	auto& worldspawn = context.Entities.Worldspawn();
	const int track = worldspawn.GetInteger("sounds", 0);
	worldspawn.Remove("sounds");

	if (track != 0 && track != 1)
	{
		auto& music = context.Entities.Create("ambient_music");

		if (track == -1)
		{
			music.SetInteger("command", Stop);
		}
		else
		{
			music.SetInteger("command", Play);
			music.Set("filename", MusicFileName(track, context.Game));
		}

		music.SetInteger("target_selector", AllPlayers);
		music.SetInteger("remove_on_fire", 1);
		music.Set("targetname", "game_playeractivate");
	}
}

void ConvertWorldspawnGameTitleValue(Context& context)
{
	auto& worldspawn = context.Entities.Worldspawn();

	if (!worldspawn.Has("gametitle"))
		return;

	if (worldspawn.GetInteger("gametitle") != 0)
	{
		const char* title = context.MapName == "of0a0" ? "gearbox" : context.MapName == "ba_tram1" ? "bshift" : "valve";
		worldspawn.Set("gametitle", title);
	}
	else
	{
		worldspawn.Remove("gametitle");
	}
}

struct SoundKey
{
	const char* Key;
	const char* Default;
	std::vector<const char*> Names;
	bool RotatingMessage = false;
};

void ConvertSoundIndicesToNames(Context& context)
{
	static const std::vector<const char*> DoorMove{"common/null.wav", "doors/doormove1.wav", "doors/doormove2.wav",
		"doors/doormove3.wav", "doors/doormove4.wav", "doors/doormove5.wav", "doors/doormove6.wav", "doors/doormove7.wav",
		"doors/doormove8.wav", "doors/doormove9.wav", "doors/doormove10.wav"};
	static const std::vector<const char*> DoorStop{"common/null.wav", "doors/doorstop1.wav", "doors/doorstop2.wav",
		"doors/doorstop3.wav", "doors/doorstop4.wav", "doors/doorstop5.wav", "doors/doorstop6.wav", "doors/doorstop7.wav",
		"doors/doorstop8.wav"};
	static const std::vector<const char*> Button{"common/null.wav", "buttons/button1.wav", "buttons/button2.wav",
		"buttons/button3.wav", "buttons/button4.wav", "buttons/button5.wav", "buttons/button6.wav", "buttons/button7.wav",
		"buttons/button8.wav", "buttons/button9.wav", "buttons/button10.wav", "buttons/button11.wav",
		"buttons/latchlocked1.wav", "buttons/latchunlocked1.wav", "buttons/lightswitch2.wav", "buttons/button9.wav",
		"buttons/button9.wav", "buttons/button9.wav", "buttons/button9.wav", "buttons/button9.wav", "buttons/button9.wav",
		"buttons/lever1.wav", "buttons/lever2.wav", "buttons/lever3.wav", "buttons/lever4.wav", "buttons/lever5.wav"};
	static const std::vector<const char*> Locked{"", "NA", "ND", "NF", "NFIRE", "NCHEM", "NRAD", "NCON", "NH", "NG"};
	static const std::vector<const char*> Unlocked{"", "EA", "ED", "EF", "EFIRE", "ECHEM", "ERAD", "ECON", "EH"};
	static const std::vector<const char*> MomentaryDoorMove{"common/null.wav", "doors/doormove1.wav",
		"doors/doormove2.wav", "doors/doormove3.wav", "doors/doormove4.wav", "doors/doormove5.wav", "doors/doormove6.wav",
		"doors/doormove7.wav", "doors/doormove8.wav"};
	static const std::vector<const char*> RotatingMove{"common/null.wav", "fans/fan1.wav", "fans/fan2.wav",
		"fans/fan3.wav", "fans/fan4.wav", "fans/fan5.wav"};
	static const std::vector<const char*> PlatMove{"common/null.wav", "plats/bigmove1.wav", "plats/bigmove2.wav",
		"plats/elevmove1.wav", "plats/elevmove2.wav", "plats/elevmove3.wav", "plats/freightmove1.wav",
		"plats/freightmove2.wav", "plats/heavymove1.wav", "plats/rackmove1.wav", "plats/railmove1.wav",
		"plats/squeekmove1.wav", "plats/talkmove1.wav", "plats/talkmove2.wav"};
	static const std::vector<const char*> PlatStop{"common/null.wav", "plats/bigstop1.wav", "plats/bigstop2.wav",
		"plats/freightstop1.wav", "plats/heavystop2.wav", "plats/rackstop1.wav", "plats/railstop1.wav",
		"plats/squeekstop1.wav", "plats/talkstop1.wav"};
	static const std::vector<const char*> TrackTrainMove{"", "plats/ttrain1.wav", "plats/ttrain2.wav",
		"plats/ttrain3.wav", "plats/ttrain4.wav", "plats/ttrain6.wav", "plats/ttrain7.wav"};

	static const std::vector<SoundKey> Door{{"movesnd", "common/null.wav", DoorMove},
		{"stopsnd", "common/null.wav", DoorStop}, {"locked_sound", "", Button}, {"unlocked_sound", "", Button},
		{"locked_sentence", "", Locked}, {"unlocked_sentence", "", Unlocked}};
	static const std::vector<SoundKey> ButtonKeys{{"sounds", "", Button}, {"locked_sound", "", Button},
		{"unlocked_sound", "", Button}, {"locked_sentence", "", Locked}, {"unlocked_sentence", "", Unlocked}};
	static const std::vector<SoundKey> Plat{{"movesnd", "", PlatMove}, {"stopsnd", "", PlatStop}};

	static const std::unordered_map<std::string, std::vector<SoundKey>> Classes{
		{"func_door", Door},
		{"func_water", Door},
		{"func_door_rotating", Door},
		{"momentary_door", {{"movesnd", "common/null.wav", MomentaryDoorMove}}},
		{"func_rotating", {{"sounds", "common/null.wav", RotatingMove, true}}},
		{"func_button", ButtonKeys},
		{"func_rot_button", ButtonKeys},
		{"momentary_rot_button", {{"sounds", "buttons/button9.wav", Button}}},
		{"func_train", Plat},
		{"func_plat", Plat},
		{"func_platrot", Plat},
		{"func_trackchange", Plat},
		{"func_trackautochange", Plat},
		{"env_spritetrain", Plat},
		{"func_tracktrain", {{"sounds", "", TrackTrainMove}}},
	};

	for (Entity* entity : context.Entities.All())
	{
		const auto keys = Classes.find(entity->ClassName());

		if (keys == Classes.end())
			continue;

		for (const auto& key : keys->second)
		{
			std::optional<std::string> name;

			// func_rotating: the game read a sound file name from "message" first.
			if (key.RotatingMessage)
			{
				const auto message = entity->Find("message");
				std::string value = message ? *message : std::string{};
				const bool had = message != nullptr;
				entity->Remove("message");

				if (had && !value.empty())
					name = std::move(value);
			}

			if (!name)
			{
				name = key.Default;

				if (const auto value = entity->Find(key.Key))
				{
					int index = 0;
					if (!TryParseInt(*value, index))
						index = 0;

					if (index >= 0 && index < static_cast<int>(key.Names.size()))
						name = key.Names[index];
				}
			}

			entity->Remove(key.Key);

			if (!name->empty())
				entity->Set(key.Key, *name);
		}
	}
}

void SetCustomHullForGenericMonsters(Context& context)
{
	for (Entity* entity : context.Entities.OfClass("monster_generic"))
	{
		const auto model = entity->Model();

		if (model == "models/player.mdl" || model == "models/holo.mdl")
		{
			entity->SetVector3("custom_hull_min", HullMin);
			entity->SetVector3("custom_hull_max", HullMax);
		}
	}
}

void FixRenderColorFormat(Context& context)
{
	static const std::regex Invalid("^([0-9]+), ([0-9]+), ([0-9]+)$");

	for (Entity* entity : context.Entities.All())
	{
		const auto value = entity->Find("rendercolor");
		std::smatch match;

		if (value && std::regex_match(*value, match, Invalid))
			entity->Set("rendercolor", match[1].str() + " " + match[2].str() + " " + match[3].str());
	}
}

void RemoveDMDelayFromChargers(Context& context)
{
	for (Entity* entity : context.Entities.All())
	{
		const auto className = entity->ClassName();

		if (className == "func_healthcharger" || className == "func_recharge")
			entity->Remove("dmdelay");
	}
}

void ConvertBreakableItem(Context& context)
{
	static const char* const Items[] = {"item_battery", "item_healthkit", "weapon_9mmhandgun", "ammo_9mmclip",
		"weapon_9mmar", "ammo_9mmar", "ammo_argrenades", "weapon_shotgun", "ammo_buckshot", "weapon_crossbow",
		"ammo_crossbow", "weapon_357", "ammo_357", "weapon_rpg", "ammo_rpgclip", "ammo_gaussclip", "weapon_handgrenade",
		"weapon_tripmine", "weapon_satchel", "weapon_snark", "weapon_hornetgun", "weapon_penguin"};

	for (Entity* entity : context.Entities.All())
	{
		const auto className = entity->ClassName();

		if (className != "func_breakable" && className != "func_pushable")
			continue;

		const int index = entity->GetInteger("spawnobject", 0);

		if (index == 0)
			entity->Remove("spawnobject");
		else if (index > 0 && index <= static_cast<int>(std::size(Items)))
			entity->Set("spawnobject", Items[index - 1]);
	}
}

void ReworkGamePlayerEquip(Context& context)
{
	for (Entity* entity : context.Entities.OfClass("game_player_equip"))
	{
		if (!entity->TargetName().empty())
			continue;

		const int flags = entity->SpawnFlags();

		if ((flags & 1) != 0)
			continue;

		entity->Set("targetname", "game_playerspawn");
		entity->SetSpawnFlags(flags | 1);
	}
}

void FixNonLoopingSounds(Context& context)
{
	static const char* const Sounds[] = {"ambience/alienflyby1.wav", "misc/ear_ringing.wav", "weapons/explode3.wav",
		"weapons/explode4.wav", "weapons/mortar.wav", "nihilanth/nil_alone.wav", "nihilanth/nil_win.wav",
		"Gonarch/gon_die1.wav"};

	for (Entity* entity : context.Entities.OfClass("ambient_generic"))
	{
		const auto sound = entity->GetString("message");

		if (std::none_of(std::begin(Sounds), std::end(Sounds), [&](const char* s)
				{ return sound == s; }))
			continue;

		const int flags = entity->SpawnFlags();

		if ((flags & 32) == 0)
			entity->SetSpawnFlags(flags | 32);
	}
}

void PruneExcessMultiManagerKeys(Context& context)
{
	for (Entity* manager : context.Entities.OfClass("multi_manager"))
	{
		std::vector<std::size_t> indices;

		for (std::size_t i = 0; i < manager->KeyValues.size(); ++i)
		{
			const auto& key = manager->KeyValues[i].first;

			if (key != "classname" && key != "targetname" && key != "origin" && key != "wait")
				indices.push_back(i);
		}

		if (indices.size() > 16)
		{
			for (std::size_t i = indices.size(); i-- > 16;)
				manager->KeyValues.erase(manager->KeyValues.begin() + indices[i]);
		}
	}
}

void AdjustMaxRange(Context& context)
{
	if (context.MapName == "c2a2a")
		context.Entities.Worldspawn().SetInteger("MaxRange", 8192);
}

void ConvertBarneyModel(Context& context)
{
	for (Entity* barney : context.Entities.OfClass("monster_barney_dead"))
	{
		if (!barney->Has("body"))
			continue;

		const int body = barney->GetInteger("body");
		barney->Remove("body");
		barney->SetInteger("bodystate", body == 0 ? 1 : body == 2 ? 0 : 2);
	}
}

void ChangeBell1SoundAndPitch(Context& context)
{
	for (Entity* entity : context.Entities.OfClass("ambient_generic"))
	{
		if (entity->Find("message") && *entity->Find("message") == "buttons/bell1.wav" && entity->GetInteger("pitch") == 80)
		{
			entity->Set("message", "buttons/bell1_alt.wav");
			entity->SetInteger("pitch", 100);
		}
	}
}

// ---- Map fixes (by map name, whatever the game) ----

void C2a5FixBarrelPushTriggers(Context& context)
{
	if (context.MapName != "c2a5")
		return;

	static const char* const Setups[][3] = {{"can_expl2_mm", "can_expl2_push", "can_expl2_shoot"},
		{"can_expl4_mm", "can_expl4_push", "can_expl4_shoot"}, {"can_expl5_mm", "can_expl5_push", "can_expl5_shoot"}};

	for (const auto& setup : Setups)
	{
		Entity* manager = context.Entities.Find(setup[0]);
		Entity* push = context.Entities.Find(setup[1]);
		Entity* pushable = context.Entities.Find(setup[2]);

		if (!manager || !push || !pushable)
			continue;

		manager->Remove(std::string(setup[1]) + "#1");
		push->SetSpawnFlags(push->SpawnFlags() | 1);

		auto origin = pushable->GetVector3("origin");
		++origin.Z;
		pushable->SetVector3("origin", origin);
	}
}

void C4a3FixFlareSprites(Context& context)
{
	if (context.MapName != "c4a3")
		return;

	for (Entity* entity : context.Entities.OfClass("env_sprite"))
	{
		if (entity->Find("model") && *entity->Find("model") == "sprites/XFlare3.spr")
		{
			entity->SetInteger("rendermode", 5);
			entity->SetDouble("renderamt", 255);
		}
	}
}

void C3a2bFixWaterValves(Context& context)
{
	if (context.MapName != "c3a2b")
		return;

	auto valves = [&]
	{
		std::vector<Entity*> found;
		for (Entity* e : context.Entities.All())
		{
			if (e->Target() == "ms1" || e->Target() == "ms2")
				found.push_back(e);
		}
		return found;
	};

	auto managers = [&]
	{
		std::vector<Entity*> found;
		for (Entity* e : context.Entities.All())
		{
			if (e->TargetName() == "mm3" || e->TargetName() == "mm4")
				found.push_back(e);
		}
		return found;
	};

	Entity* firstStop = context.Entities.Find("core1");
	Entity* secondStop = context.Entities.Find("core2");

	if (valves().size() != 2 || managers().size() != 2 || !firstStop || !secondStop)
		return;

	context.Entities.Create("multisource").Set("targetname", "valve_ms");

	auto& relay = context.Entities.Create("trigger_relay");
	relay.Set("targetname", "valve_ms_relay");
	relay.Set("target", "valve_ms");

	auto& triggerAuto = context.Entities.Create("trigger_auto");
	triggerAuto.SetSpawnFlags(1);
	triggerAuto.Set("target", "valve_ms_relay");

	for (Entity* valve : valves())
		valve->Set("master", "valve_ms");

	for (Entity* manager : managers())
		manager->SetDouble("valve_ms_relay", 0);

	firstStop->Set("message", "valve_ms_relay");
	secondStop->Set("message", "valve_ms_relay");
}

void C3a2FixLoadSaved(Context& context)
{
	if (context.MapName != "c3a2")
		return;

	if (Entity* restart = context.Entities.Find("c3a2_restart"))
		restart->SetInteger("loadtime", 10);
}

void C4a2FixNihilanthDialogue(Context& context)
{
	if (context.MapName != "c4a2")
		return;

	if (Entity* entity = context.Entities.First([](const Entity& e)
			{ return e.Target() == "c4a2_startaudio"; }))
		entity->SetInteger("triggerstate", 1);
}

void C2a5FixCrateGlobalName(Context& context)
{
	if (context.MapName != "c2a5")
		return;

	for (Entity* entity : context.Entities.OfClass("func_breakable"))
	{
		if (entity->TargetName() == "artillery_deploy3_expl" && entity->GetString("globalname") == "c2a4g_crate4")
			entity->Remove("globalname");
	}
}

// ---- Opposing Force ----

std::pair<int, int> ScientistItem(int body)
{
	switch (body)
	{
	case 4: return {1, 1};
	case 5: return {3, 2};
	default: return {body, 0};
	}
}

void ConvertScientistItem(Context& context)
{
	if (context.Game != JhlMapGame::OpposingForce)
		return;

	for (Entity* scientist : context.Entities.OfClass("monster_scientist"))
	{
		if (!scientist->Has("body"))
			continue;

		const auto [body, item] = ScientistItem(scientist->GetInteger("body"));
		scientist->SetInteger("item", item);
		scientist->SetInteger("body", body);
	}

	for (Entity* monster : context.Entities.OfClass("monster_generic"))
	{
		if (monster->Model() != "models/scientist.mdl")
			continue;

		const auto [body, item] = ScientistItem(monster->GetInteger("body"));
		monster->SetInteger("body", body + 1 * 4 * 2 * item);
	}
}

void ConvertSuitToPCV(Context& context)
{
	if (context.Game != JhlMapGame::OpposingForce)
		return;

	for (Entity* suit : context.Entities.OfClass("item_suit"))
		suit->Set("model", "models/w_pcv.mdl");
}

void RenameOtisAnimations(Context& context)
{
	RenameAnimations(context, "monster_otis", "models/otis.mdl", {{"fence", "otis_fence"}, {"wave", "otis_wave"}});
}

void MonsterTentacleSpawnFlag(Context& context)
{
	if (context.Game != JhlMapGame::OpposingForce)
		return;

	for (Entity* tentacle : context.Entities.OfClass("monster_tentacle"))
	{
		tentacle->Set("model", (tentacle->SpawnFlags() & (1 << 6)) != 0 ? "models/tentacle3.mdl" : "models/tentacle2_lower.mdl");
		tentacle->SetDouble("height0", 0);
		tentacle->SetDouble("height1", 136);
		tentacle->SetDouble("height2", 190);
		tentacle->SetDouble("height3", 328);
	}
}

void Of0a0DisableItemDropping(Context& context)
{
	if (context.MapName != "of0a0")
		return;

	for (Entity* entity : context.Entities.All())
	{
		const auto name = entity->TargetName();

		if (name == "ichabod" || name == "booth")
			entity->SetInteger("allow_item_dropping", 0);
	}
}

void Of1a4bChangeLoaderSkin(Context& context)
{
	if (context.MapName != "of1a4b")
		return;

	for (Entity* loader : context.Entities.OfClass("monster_op4loader"))
		loader->SetInteger("skin", 1);
}

void Of4a4Bridge(Context& context)
{
	if (context.MapName != "of4a4")
		return;

	if (Entity* manager = context.Entities.Find("kill_pitworm_mm"))
	{
		manager->SetInteger("bridge_global_trigger", 30);
		manager->SetInteger("bridge_mm", 30);
	}
}

void RenameBlackOpsAnimations(Context& context)
{
	RenameAnimations(context, "monster_male_assassin", "models/massn.mdl",
		{{"strafeleft", "strafeleft_cine"}, {"straferight", "straferight_cine"}});
}

void AdjustBlackOpsSkin(Context& context)
{
	for (Entity* entity : context.Entities.OfClass("monster_male_assassin"))
	{
		int head = entity->GetInteger("head");
		int skin = 0;

		if (head == 1)
		{
			head = 0;
			skin = 1;
		}
		else if (head == 2)
		{
			head = 1;
			skin = 1;
		}

		entity->SetInteger("head", head);
		entity->SetInteger("skin", skin);
	}
}

void RenameIntroGruntAnimations(Context& context)
{
	auto sitting = [](int count, const std::string& to)
	{
		Remap remap;
		for (int i = 1; i <= count; ++i)
			remap.emplace_back("sitting" + std::to_string(i), to + std::to_string(i));
		return remap;
	};

	RenameAnimations(context, nullptr, "models/intro_commander.mdl",
		{{"fall_out", "intro_stand_fallout"}, {"stand_react", "intro_stand_react"}, {"bark", "intro_stand_bark"}});
	RenameAnimations(context, nullptr, "models/intro_medic.mdl", sitting(7, "intro_sitting_holster"));
	RenameAnimations(context, nullptr, "models/intro_regular.mdl", sitting(5, "intro_sitting_mp5_"));

	auto saw = sitting(5, "intro_sitting_mp5_");
	saw.insert(saw.end(), {{"stiff", "intro_sitting_stiff"}, {"sit_React", "intro_sitting_react"}, {"cower", "intro_sitting_cower1"}});
	RenameAnimations(context, nullptr, "models/intro_saw.mdl", saw);

	auto torch = sitting(7, "intro_sitting_holster");
	torch.emplace_back("cower", "intro_sitting_cower2");
	RenameAnimations(context, nullptr, "models/intro_torch.mdl", torch);
}

void ConvertOtisBodyState(Context& context)
{
	for (Entity* otis : context.Entities.OfClass("monster_otis"))
	{
		if (!otis->Has("bodystate"))
			continue;

		const int state = otis->GetInteger("bodystate");
		otis->SetInteger("bodystate", state == -1 ? 0 : state);
	}
}

void FixBlackOpsSpawnDelay(Context& context)
{
	if (context.MapName != "of6a1")
		return;

	if (Entity* maker = context.Entities.First([](const Entity& e)
			{ return e.ClassName() == "monstermaker" && e.TargetName() == "assassin4_spawn"; }))
		maker->SetDouble("delay", 0);
}

void DisableFuncTankOfPersistence(Context& context)
{
	if (context.Game != JhlMapGame::OpposingForce)
		return;

	for (Entity* entity : context.Entities.All())
	{
		const auto className = entity->ClassName();

		if (className == "func_tank_of" || className == "func_tanklaser_of" || className == "func_tankrocket_of" ||
			className == "func_tankmortar_of")
			entity->SetDouble("persistence", 0);
	}
}

void ChangeFuncTankOfToFuncTank(Context& context)
{
	if (context.Game != JhlMapGame::OpposingForce)
		return;

	for (Entity* entity : context.Entities.All())
	{
		const auto className = entity->ClassName();

		for (const char* tank : {"func_tank", "func_tanklaser", "func_tankrocket", "func_tankmortar", "func_tankcontrols"})
		{
			if (className == std::string(tank) + "_of")
			{
				entity->SetClassName(tank);
				break;
			}
		}
	}
}

void Of2a2FixGruntBody(Context& context)
{
	if (context.MapName != "of2a2")
		return;

	Entity* first = context.Entities.Find("fgrunt1");
	Entity* second = context.Entities.Find("fgrunt2");

	if (first)
		first->SetInteger("body", 1);
	if (second)
		second->SetInteger("body", 1);
}

void RemoveGameModeSettings(Context& context)
{
	if (context.Game == JhlMapGame::OpposingForce)
		context.Entities.Worldspawn().Remove("defaultctf");
}

void Of1a1FixStretcherGun(Context& context)
{
	if (context.MapName != "of1a1")
		return;

	if (Entity* grunt = context.Entities.Find("stretcher_grunt"))
		grunt->SetInteger("body", 17);
}

void OfBoot1FixOspreyScript(Context& context)
{
	if (context.MapName != "ofboot1")
		return;

	if (Entity* script = context.Entities.First([](const Entity& e)
			{ return e.ClassName() == "scripted_sequence" && e.GetString("m_iszEntity") == "osprey"; }))
		script->Set("targetname", "osprey_script");
}

void Of6a5FixGenewormArriveSound(Context& context)
{
	if (context.MapName != "of6a5")
		return;

	if (Entity* sound = context.Entities.Find("genearrivesound"))
		sound->Set("message", "ambience/port_suckout1.wav");
}

void Of5a2FixXenBullsquidScripts(Context& context)
{
	if (context.MapName != "of5a2")
		return;

	Entity* first = context.Entities.Find("t27");
	Entity* second = context.Entities.Find("t6");

	if (first)
		first->Set("targetname", "feeder_1");
	if (second)
		second->Set("targetname", "feeder_2");
}

// ---- Blue Shift ----

void BaYard1FixDeadScientistModel(Context& context)
{
	if (context.MapName != "ba_yard1")
		return;

	for (Entity* entity : context.Entities.OfClass("monster_scientist_dead"))
	{
		if (entity->GetInteger("body") == 3)
			entity->SetInteger("body", 0);
	}
}

void BaYard4aSlaves(Context& context)
{
	if (context.MapName != "ba_yard4a")
		return;

	for (const std::string slave : {"frozen_slave_1", "frozen_slave_2"})
	{
		Entity* breakable = context.Entities.First([&](const Entity& e)
			{ return e.ClassName() == "func_breakable" && e.Target() == slave; });
		Entity* script = context.Entities.First([&](const Entity& e)
			{ return e.ClassName() == "scripted_sequence" && e.GetString("m_iszEntity") == slave; });

		if (!breakable || !script)
			continue;

		script->Set("targetname", slave + "_script");
		breakable->Set("target", slave + "_script");
	}
}

void RenameConsoleCivAnimations(Context& context)
{
	RenameAnimations(context, nullptr, "models/console_civ_scientist.mdl",
		{{"idle1", "console_idle1"}, {"work", "console_work"}, {"shocked", "console_shocked"}, {"sneeze", "console_sneeze"}});
}

void ChangeRosenbergModel(Context& context)
{
	if (context.Game != JhlMapGame::BlueShift)
		return;

	auto update = [](Entity& entity)
	{
		if (entity.ClassName() == "monster_rosenberg")
			entity.Remove("model");
		else
			entity.Set("model", "models/rosenberg.mdl");

		entity.Remove("body");
	};

	for (Entity* entity : context.Entities.All())
	{
		const auto className = entity->ClassName();

		if (className == "monster_rosenberg" ||
			(className == "monster_generic" && entity->Model() == "models/scientist.mdl" && entity->GetInteger("body") == 3))
			update(*entity);
	}

	for (Entity* entity : context.Entities.OfClass("monster_scientist"))
	{
		if (entity->GetInteger("body") != 3)
			continue;

		entity->SetClassName("monster_rosenberg");
		update(*entity);
	}
}

void RemapRosenbergNoUseFlag(Context& context)
{
	for (Entity* rosenberg : context.Entities.OfClass("monster_rosenberg"))
	{
		const int flags = rosenberg->SpawnFlags();
		const bool noUse = (flags & (1 << 8)) != 0;
		rosenberg->SetSpawnFlags(flags & ~(1 << 8));
		rosenberg->SetInteger("allow_follow", noUse ? 0 : 1);
	}
}

void ChangeBlueShiftSentences(Context& context)
{
	if (context.Game != JhlMapGame::BlueShift)
		return;

	static const char* const Holo[] = {"HOLO_4JUMPS", "HOLO_ARMOR", "HOLO_BREAKBOX", "HOLO_BREATH", "HOLO_BUTTON",
		"HOLO_CHARGER", "HOLO_COMMENCING", "HOLO_CONGRATS", "HOLO_DONE", "HOLO_DROWN", "HOLO_DUCK", "HOLO_FALLSHORT",
		"HOLO_FANTASTIC", "HOLO_FLASHLIGHT", "HOLO_GREATWORK", "HOLO_GRENADE", "HOLO_HITALL", "HOLO_INJURY",
		"HOLO_INTRO", "HOLO_JDUCK", "HOLO_JUMP", "HOLO_JUMPDOWN", "HOLO_JUMPGAP", "HOLO_KEEPTRYING", "HOLO_LADDER",
		"HOLO_LEADGUARD", "HOLO_LIGHTOFF", "HOLO_LONGJUMP", "HOLO_MEDKIT", "HOLO_MOVE", "HOLO_NICEJOB",
		"HOLO_PIPEDUCK", "HOLO_PULLBOX", "HOLO_PUSHBOX", "HOLO_RADIATION", "HOLO_RETRY", "HOLO_RUNSTART",
		"HOLO_SPINBRIDGE", "HOLO_STARTLIFT", "HOLO_STEAM", "HOLO_TARGET", "HOLO_TRYAGAIN", "HOLO_USETRAIN"};

	// Looked up in any case (StringComparer.OrdinalIgnoreCase).
	static const std::unordered_map<std::string, std::string> Sentences = []
	{
		std::unordered_map<std::string, std::string> map{
			{"!NA1", "!EA0"}, {"!SC_HELLO6", "NULLSENT"}, {"!SC_HELLO8", "NULLSENT"}};

		for (const char* name : Holo)
			map.emplace("!" + std::string(name), "!BS" + std::string(name));

		return map;
	}();

	auto upper = [](std::string text)
	{
		std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c)
			{ return static_cast<char>(std::toupper(c)); });
		return text;
	};

	for (Entity* entity : context.Entities.All())
	{
		const auto className = entity->ClassName();
		const char* key = className == "scripted_sentence" ? "sentence" : className == "ambient_generic" ? "message" : nullptr;

		if (!key)
			continue;

		if (const auto name = entity->Find(key))
		{
			if (const auto replacement = Sentences.find(upper(*name)); replacement != Sentences.end())
				entity->Set(key, replacement->second);
		}
	}
}

void BaSecurity2ChangeHologramModel(Context& context)
{
	if (context.MapName != "ba_security2")
		return;

	if (Entity* gina = context.Entities.Find("gina_push"))
	{
		gina->Set("model", "models/holo_cart.mdl");
		gina->SetVector3("custom_hull_min", HullMin);
		gina->SetVector3("custom_hull_max", HullMax);
	}
}

void BaOutroDisableTriggerAuto(Context& context)
{
	if (context.MapName != "ba_outro")
		return;

	for (Entity* trigger : context.Entities.OfClass("trigger_auto"))
	{
		if (trigger->Find("target") && *trigger->Find("target") == "start_outro")
		{
			trigger->SetSpawnFlags(trigger->SpawnFlags() | 1);
			break;
		}
	}
}

void BaTram1FixSuit(Context& context)
{
	if (context.MapName != "ba_tram1")
		return;

	for (Entity* suit : context.Entities.OfClass("item_suit"))
		context.Entities.Remove(suit);
}

void BaPower2RemoveChapterTitle(Context& context)
{
	if (context.MapName == "ba_power2")
		context.Entities.Worldspawn().Remove("chaptertitle");
}

void BaOutroFixGruntsBody(Context& context)
{
	if (context.MapName != "ba_outro")
		return;

	if (Entity* grunt = context.Entities.Find("drag_grunt1"))
		grunt->SetInteger("body", 4);
	if (Entity* grunt = context.Entities.Find("drag_grunt2"))
		grunt->SetInteger("body", 1);
}

void BaCanal1FixAlienSlaveSpawns(Context& context)
{
	if (context.MapName != "ba_canal1")
		return;

	for (Entity* entity : context.Entities.All())
	{
		const auto name = entity->TargetName();

		if (name == "tele5_spawner" || name == "tele4_spawner")
			entity->Remove("netname");
	}
}

void BaTram1FixScientistHead(Context& context)
{
	if (context.MapName != "ba_tram1")
		return;

	if (Entity* sitter = context.Entities.Find("sitter"))
		sitter->Remove("body");
}

void BaTram2FixScientistSkinColor(Context& context)
{
	if (context.MapName != "ba_tram2")
		return;

	for (const char* name : {"joey_normal", "joey_reflect"})
	{
		if (Entity* joey = context.Entities.Find(name))
			joey->SetInteger("skin", 1);
	}
}

// The upgrades, in the ContentInstaller's order (MapUpgradeBuilderExtensions).
using Upgrade = void (*)(Context&);

constexpr Upgrade Upgrades[] = {
	// Shared.
	ConvertAngleToAngles,
	RenameEntityClassNames,
	ConvertWorldItemsToItem,
	AdjustShotgunAngles,
	ConvertOtisModel,
	RenameMessages,
	ReworkMusicPlayback,
	ConvertWorldspawnGameTitleValue,
	ConvertSoundIndicesToNames,
	SetCustomHullForGenericMonsters,
	FixRenderColorFormat,
	RemoveDMDelayFromChargers,
	ConvertBreakableItem,
	ReworkGamePlayerEquip,
	FixNonLoopingSounds,
	PruneExcessMultiManagerKeys,
	AdjustMaxRange,
	ConvertBarneyModel,
	ChangeBell1SoundAndPitch,
	// Half-Life.
	C2a5FixBarrelPushTriggers,
	C4a3FixFlareSprites,
	C3a2bFixWaterValves,
	C3a2FixLoadSaved,
	C4a2FixNihilanthDialogue,
	C2a5FixCrateGlobalName,
	// Opposing Force.
	ConvertSuitToPCV,
	ConvertScientistItem,
	RenameOtisAnimations,
	MonsterTentacleSpawnFlag,
	Of0a0DisableItemDropping,
	Of1a4bChangeLoaderSkin,
	Of4a4Bridge,
	RenameBlackOpsAnimations,
	AdjustBlackOpsSkin,
	RenameIntroGruntAnimations,
	ConvertOtisBodyState,
	FixBlackOpsSpawnDelay,
	DisableFuncTankOfPersistence,
	ChangeFuncTankOfToFuncTank,
	Of2a2FixGruntBody,
	RemoveGameModeSettings,
	Of1a1FixStretcherGun,
	OfBoot1FixOspreyScript,
	Of6a5FixGenewormArriveSound,
	Of5a2FixXenBullsquidScripts,
	// Blue Shift.
	BaYard1FixDeadScientistModel,
	BaYard4aSlaves,
	RenameConsoleCivAnimations,
	ChangeRosenbergModel,
	RemapRosenbergNoUseFlag,
	ChangeBlueShiftSentences,
	BaSecurity2ChangeHologramModel,
	BaOutroDisableTriggerAuto,
	BaTram1FixSuit,
	BaPower2RemoveChapterTitle,
	BaOutroFixGruntsBody,
	BaCanal1FixAlienSlaveSpawns,
	BaTram1FixScientistHead,
	BaTram2FixScientistSkinColor,
};

// ---- The file ----

constexpr int BspVersion = 30;
constexpr int LumpCount = 15;
constexpr std::size_t HeaderSize = 4 + LumpCount * 8;

struct Lump
{
	std::int32_t Offset, Length;
};

bool ReadHeader(const std::string& bsp, Lump (&lumps)[LumpCount])
{
	if (bsp.size() < HeaderSize)
		return false;

	std::int32_t version;
	std::memcpy(&version, bsp.data(), 4);

	if (version != BspVersion)
		return false;

	std::memcpy(lumps, bsp.data() + 4, sizeof(lumps));

	for (const auto& lump : lumps)
	{
		if (lump.Offset < 0 || lump.Length < 0 || static_cast<std::size_t>(lump.Offset) + lump.Length > bsp.size())
			return false;
	}

	return true;
}

bool LooksLikeEntities(const std::string& bsp, const Lump& lump)
{
	for (std::int32_t i = 0; i < lump.Length; ++i)
	{
		const char c = bsp[lump.Offset + i];

		if (!IsDotNetSpace(c))
			return c == '{';
	}

	return false;
}

// Blue Shift lists the planes first and the entities second; Half-Life the other way round.
bool EntityLumpIndex(const std::string& bsp, const Lump (&lumps)[LumpCount], int& index)
{
	if (LooksLikeEntities(bsp, lumps[0]))
		index = 0;
	else if (LooksLikeEntities(bsp, lumps[1]))
		index = 1;
	else
		return false;

	return true;
}

bool ParseEntities(std::string_view text, EntityList& list)
{
	std::size_t i = 0;

	auto skipSpace = [&]
	{
		while (i < text.size() && (IsDotNetSpace(text[i]) || text[i] == '\0'))
			++i;
	};

	auto quoted = [&](std::string& out)
	{
		if (i >= text.size() || text[i] != '"')
			return false;

		const std::size_t end = text.find('"', i + 1);

		if (end == std::string_view::npos)
			return false;

		out.assign(text.substr(i + 1, end - i - 1));
		i = end + 1;
		return true;
	};

	while (true)
	{
		skipSpace();

		if (i >= text.size())
			break;

		if (text[i] != '{')
			return false;

		++i;
		auto entity = std::make_unique<Entity>();

		while (true)
		{
			skipSpace();

			if (i < text.size() && text[i] == '}')
			{
				++i;
				break;
			}

			std::string key, value;

			if (!quoted(key))
				return false;

			skipSpace();

			if (!quoted(value))
				return false;

			// As the ContentInstaller reads them (Sledge.Formats.Bsp): a key with no value is left out, and a key given
			// twice keeps its last value, in its last place (the engine would end up with the same).
			if (value.empty())
				continue;

			entity->Remove(key);
			entity->KeyValues.emplace_back(std::move(key), std::move(value));
		}

		list.Entities.push_back(std::move(entity));
	}

	return !list.Entities.empty() && list.Entities.front()->ClassName() == "worldspawn";
}

std::string WriteEntities(const EntityList& list)
{
	std::string text;

	for (const auto& entity : list.Entities)
	{
		text += "{\n";

		for (const auto& [key, value] : entity->KeyValues)
			text += "\"" + key + "\" \"" + value + "\"\n";

		text += "}\n";
	}

	text += '\0';
	return text;
}
} // namespace

bool JhlMapConvert_EntityText(const std::string& bsp, std::string& text)
{
	Lump lumps[LumpCount];
	int index;

	if (!ReadHeader(bsp, lumps) || !EntityLumpIndex(bsp, lumps, index))
		return false;

	text.assign(bsp, lumps[index].Offset, lumps[index].Length);
	return true;
}

bool JhlMapConvert(const std::string& bsp, JhlMapGame game, std::string_view mapName, std::string& converted, std::string& error)
{
	Lump lumps[LumpCount];
	int entityIndex;

	if (!ReadHeader(bsp, lumps) || !EntityLumpIndex(bsp, lumps, entityIndex))
	{
		error = "not a Half-Life map";
		return false;
	}

	if (entityIndex == 1)
		std::swap(lumps[0], lumps[1]);

	EntityList entities;

	if (!ParseEntities(std::string_view(bsp).substr(lumps[0].Offset, lumps[0].Length), entities))
	{
		error = "couldn't read its entities";
		return false;
	}

	Context context{entities, game, mapName};

	for (const auto upgrade : Upgrades)
		upgrade(context);

	entities.Worldspawn().Set("UpgradeToolVersion", "1.0.0");

	const std::string entityText = WriteEntities(entities);

	// The lumps in the order they were in the file, the entities' replaced, each on a 4 byte boundary.
	int order[LumpCount];

	for (int i = 0; i < LumpCount; ++i)
		order[i] = i;

	std::stable_sort(std::begin(order), std::end(order), [&](int a, int b)
		{ return lumps[a].Offset < lumps[b].Offset; });

	converted.assign(HeaderSize, '\0');
	Lump written[LumpCount];

	for (const int i : order)
	{
		converted.resize((converted.size() + 3) & ~std::size_t{3}, '\0');
		written[i].Offset = static_cast<std::int32_t>(converted.size());

		if (i == 0)
			converted += entityText;
		else
			converted.append(bsp, lumps[i].Offset, lumps[i].Length);

		written[i].Length = static_cast<std::int32_t>(converted.size()) - written[i].Offset;
	}

	std::memcpy(converted.data(), &BspVersion, 4);
	std::memcpy(converted.data() + 4, written, sizeof(written));
	return true;
}
