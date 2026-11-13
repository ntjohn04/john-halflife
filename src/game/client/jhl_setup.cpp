#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdarg>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <variant>
#include <vector>

#include "hud.h"
#include "cl_util.h"
#include "jhl_mapconvert.h"
#include "jhl_setup.h"
#include "studio.h"
#include "utils/filesystem_utils.h"

namespace fs = std::filesystem;

namespace
{
constexpr int SetupVersion = 13;

constexpr const char* BaseFolder = "johnhalflife";
constexpr const char* StampFile = "jhl_setup.txt";
constexpr const char* FilesList = "jhl_setup_files.txt";
constexpr const char* MapsList = "jhl_setup_maps.txt";

constexpr int FramesBeforeMessages = 5;
bool g_DeferMessages = false;
std::string g_DeferredMessages;
int g_FrameCount = 0;

void Log(const char* format, ...)
{
	char text[1024];
	va_list args;
	va_start(args, format);
	std::vsnprintf(text, sizeof(text), format, args);
	va_end(args);

	if (g_DeferMessages)
		g_DeferredMessages += text;
	else
		gEngfuncs.Con_Printf("%s", text);
}

struct MenuFolder
{
	const char* Folder;
	const char* OwnGame;
	bool Widescreen;
};

constexpr MenuFolder MenuFolders[] = {
	{"johnhalflife", nullptr, true},
	{"johnhalflife_op4", "gearbox", false},
	{"johnhalflife_bs", "bshift", false},
	{"johnhalflife_uplink", nullptr, true},
};

struct MenuScheme
{
	const char* Folder;
	const char* FontFile;
	const char* FontName;
	const char* Color;
	const char* ArmedColor;
	const char* BlurColor;
};

constexpr MenuScheme MenuSchemes[] = {
	// The gold of its logo, in a military stencil.
	{"johnhalflife_op4", "resource/fonts/StardosStencil-Bold.ttf", "Stardos Stencil", "226 178 84 255", "255 214 128 255", "230 170 70 255"},
	// A typewriter's, as its logo, in ice blue.
	{"johnhalflife_bs", "resource/fonts/CourierPrime-Bold.ttf", "Courier Prime", "150 190 255 255", "215 232 255 255", "90 150 255 255"},
};

using Source = std::variant<fs::path, std::string>;

struct SetupStats
{
	int Written = 0;
	int Unchanged = 0;
	int Missing = 0;
	int Failed = 0;
};

bool ReadFile(const fs::path& path, std::string& contents)
{
	std::ifstream file(path, std::ios::binary);

	if (!file)
	{
		return false;
	}

	std::ostringstream stream;
	stream << file.rdbuf();
	contents = std::move(stream).str();
	return true;
}

bool IsFile(const fs::path& path)
{
	std::error_code error;
	return fs::is_regular_file(path, error);
}

bool IsFolder(const fs::path& path)
{
	std::error_code error;
	return fs::is_directory(path, error);
}

std::string Lower(std::string text)
{
	std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c)
		{ return static_cast<char>(std::tolower(c)); });
	return text;
}

class Plan
{
public:
	void Add(const fs::path& destination, Source source)
	{
		auto [it, added] = m_Files.try_emplace(Lower(destination.generic_string()), destination, Source{});
		it->second.second = std::move(source);
	}

	const auto& Files() const { return m_Files; }

private:
	std::map<std::string, std::pair<fs::path, Source>> m_Files;
};

std::vector<fs::path> Matching(const fs::path& folder, const std::string& start, const std::string& end = {})
{
	std::vector<fs::path> files;
	std::error_code error;

	for (const auto& entry : fs::directory_iterator(folder, error))
	{
		const std::string name = Lower(entry.path().filename().string());

		if (entry.is_regular_file(error) && name.size() >= start.size() + end.size() &&
			name.starts_with(Lower(start)) && name.ends_with(Lower(end)))
		{
			files.push_back(entry.path());
		}
	}

	return files;
}

void PlanGameFiles(const fs::path& root, Plan& plan, SetupStats& stats)
{
	std::ifstream list(root / BaseFolder / FilesList);

	if (!list)
	{
		Log("jhl_setup: %s/%s not found\n", BaseFolder, FilesList);
		++stats.Failed;
		return;
	}

	std::string line;

	while (std::getline(list, line))
	{
		if (line.empty() || line.starts_with("//"))
		{
			continue;
		}

		std::vector<std::string> fields;
		std::istringstream stream(line);

		for (std::string field; std::getline(stream, field, '\t');)
		{
			fields.push_back(field);
		}

		// An expansion folder only exists if it was installed.
		if (fields.size() < 3 || !IsFolder(root / fields[0]))
		{
			continue;
		}

		const auto source = std::find_if(fields.begin() + 2, fields.end(), [&](const std::string& field)
			{ return IsFile(root / field); });

		if (source == fields.end())
		{
			++stats.Missing;
			continue;
		}

		plan.Add(root / fields[0] / fields[1], root / *source);
	}
}

void PlanGlassGibs(const fs::path& root, Plan& plan)
{
	std::string model;

	if (!ReadFile(root / "valve" / "models" / "glassgibs.mdl", model) || model.size() < sizeof(studiohdr_t))
	{
		return;
	}

	auto* header = reinterpret_cast<studiohdr_t*>(model.data());

	if (std::memcmp(model.data(), "IDST", 4) != 0 || header->textureindex <= 0 || header->numtextures < 0 ||
		static_cast<std::size_t>(header->textureindex) + header->numtextures * sizeof(mstudiotexture_t) > model.size())
	{
		return;
	}

	auto* textures = reinterpret_cast<mstudiotexture_t*>(model.data() + header->textureindex);
	bool fixed = false;

	for (int i = 0; i < header->numtextures; ++i)
	{
		if (Lower(std::string(textures[i].name, strnlen(textures[i].name, sizeof(textures[i].name)))) == "chome_glass.bmp")
		{
			textures[i].flags |= STUDIO_NF_CHROME;
			fixed = true;
		}
	}

	if (fixed)
	{
		plan.Add(root / BaseFolder / "models" / "glassgibs.mdl", std::move(model));
	}
}

bool SetValue(std::string& text, const std::string& key, const std::string& value, std::size_t from = 0, std::size_t to = std::string::npos)
{
	const std::size_t at = text.find('"' + key + '"', from);
	const std::size_t open = at == std::string::npos ? at : text.find('"', at + key.size() + 2);
	const std::size_t close = open == std::string::npos ? open : text.find('"', open + 1);

	if (close == std::string::npos || close > to || text.find('\n', at) < close)
	{
		return false;
	}

	text.replace(open + 1, close - open - 1, value);
	return true;
}

bool MakeScheme(std::string& text, const MenuScheme& scheme)
{
	for (const char* font : {"MenuLarge", "MenuBlurLarge"})
	{
		// Within the font's block: "MenuLarge" { "1" { ... } }.
		const std::size_t at = text.find('"' + std::string(font) + '"');
		const std::size_t end = at == std::string::npos ? at : text.find('}', text.find('}', at) + 1);

		if (end == std::string::npos || !SetValue(text, "name", scheme.FontName, at, end))
		{
			return false;
		}
	}

	if (!SetValue(text, "MenuColor", scheme.Color) || !SetValue(text, "ArmedMenuColor", scheme.ArmedColor) ||
		!SetValue(text, "BlurMenuColor", scheme.BlurColor))
	{
		return false;
	}

	const std::size_t files = text.find("CustomFontFiles");
	const std::size_t end = files == std::string::npos ? files : text.find('}', files);

	if (end == std::string::npos)
	{
		return false;
	}

	text.insert(end, std::string("\t\"100\"\t\t\"") + scheme.FontFile + "\"\n\t");
	return true;
}

void PlanMenu(const fs::path& root, Plan& plan)
{
	const fs::path valve = root / "valve";

	if (!IsFolder(valve / "resource"))
	{
		Log("jhl_setup: Half-Life's resource folder not found; the menus keep what they have\n");
		return;
	}

	const fs::path gameMenu = valve / "resource" / "GameMenu.res";
	const bool haveGameMenu = IsFile(gameMenu);

	for (const auto& menu : MenuFolders)
	{
		const fs::path target = root / menu.Folder;

		if (!IsFolder(target))
		{
			continue;
		}

		if (haveGameMenu)
		{
			plan.Add(target / "resource" / "GameMenu.res", gameMenu);
		}

		std::vector<fs::path> games{valve};

		if (menu.OwnGame)
		{
			if (IsFolder(root / menu.OwnGame / "resource"))
			{
				games.push_back(root / menu.OwnGame);
			}
			else
			{
				Log("jhl_setup: %s not installed; %s uses Half-Life's menu art\n", menu.OwnGame, menu.Folder);
			}
		}

		for (const auto& game : games)
		{
			const fs::path resource = game / "resource";
			const bool widescreen = menu.Widescreen && game == valve;

			std::vector<fs::path> files = Matching(resource, "Background", "Layout.txt");

			for (const char* start : {"game_menu", "menu_"})
			{
				const auto more = Matching(resource, start);
				files.insert(files.end(), more.begin(), more.end());
			}

			if (widescreen)
			{
				const auto layouts = Matching(resource, "HD_Background", "Layout.txt");
				files.insert(files.end(), layouts.begin(), layouts.end());
			}

			for (const auto& file : files)
			{
				plan.Add(target / "resource" / file.filename(), file);
			}

			for (const auto& tile : Matching(resource / "background", widescreen ? "" : "800_"))
			{
				plan.Add(target / "resource" / "background" / tile.filename(), tile);
			}
		}

		for (const auto& scheme : MenuSchemes)
		{
			std::string text;

			if (std::string_view(scheme.Folder) != menu.Folder || !ReadFile(valve / "resource" / "TrackerScheme.res", text))
			{
				continue;
			}

			if (MakeScheme(text, scheme))
			{
				plan.Add(target / "resource" / "TrackerScheme.res", std::move(text));
			}
			else
			{
				Log("jhl_setup: Half-Life's TrackerScheme.res not as expected; %s's menu keeps its font\n", menu.Folder);
			}
		}
	}
}

// Writes a file unless it's already there, as is.
void WriteContents(const fs::path& destination, const std::string& contents, SetupStats& stats)
{
	if (std::string current; ReadFile(destination, current) && current == contents)
	{
		++stats.Unchanged;
		return;
	}

	std::error_code error;
	fs::create_directories(destination.parent_path(), error);

	std::ofstream file(destination, std::ios::binary | std::ios::trunc);
	file.write(contents.data(), static_cast<std::streamsize>(contents.size()));

	if (!file)
	{
		Log("jhl_setup: couldn't write %s\n", destination.generic_string().c_str());
		++stats.Failed;
		return;
	}

	++stats.Written;
}

void Write(const fs::path& destination, const Source& source, SetupStats& stats)
{
	if (const auto path = std::get_if<fs::path>(&source))
	{
		std::string contents;

		if (!ReadFile(*path, contents))
		{
			Log("jhl_setup: couldn't read %s\n", path->generic_string().c_str());
			++stats.Failed;
			return;
		}

		WriteContents(destination, contents, stats);
	}
	else
	{
		WriteContents(destination, std::get<std::string>(source), stats);
	}
}

void ConvertMaps(const fs::path& root, SetupStats& stats)
{
	std::ifstream list(root / BaseFolder / MapsList);

	if (!list)
	{
		Log("jhl_setup: %s/%s not found\n", BaseFolder, MapsList);
		++stats.Failed;
		return;
	}

	std::vector<std::string> lines;

	for (std::string line; std::getline(list, line);)
	{
		if (!line.starts_with("//") && line.find('\t') != std::string::npos)
			lines.push_back(line);
	}

	std::string original, converted, error;

	for (std::size_t i = 0; i < lines.size(); ++i)
	{
		const std::string& line = lines[i];
		const auto tab = line.find('\t');


		const std::string game = line.substr(0, tab);
		const std::string map = line.substr(tab + 1);
		const JhlMapGame kind = game == "gearbox" ? JhlMapGame::OpposingForce : game == "bshift" ? JhlMapGame::BlueShift : JhlMapGame::HalfLife;

		// An expansion's maps only if it's installed.
		if (!ReadFile(root / game / "maps" / (map + ".bsp"), original))
		{
			++stats.Missing;
			continue;
		}

		if (!JhlMapConvert(original, kind, map, converted, error))
		{
			Log("jhl_setup: %s/maps/%s.bsp: %s\n", game.c_str(), map.c_str(), error.c_str());
			++stats.Failed;
			continue;
		}

		WriteContents(root / BaseFolder / "maps" / (map + ".bsp"), converted, stats);
	}
}

void RefreshGraphs(const fs::path& root)
{
	const fs::path maps = root / BaseFolder / "maps";
	std::error_code error;

	for (const auto& entry : fs::directory_iterator(maps / "graphs", error))
	{
		if (entry.path().extension() != ".nod")
			continue;

		const fs::path bsp = maps / entry.path().stem().concat(".bsp");
		std::error_code timeError;
		const auto mapTime = fs::last_write_time(bsp, timeError);

		if (!timeError && entry.last_write_time(timeError) <= mapTime && !timeError)
			fs::last_write_time(entry.path(), fs::file_time_type::clock::now(), timeError);
	}
}

int ReadStamp(const fs::path& path)
{
	std::ifstream file(path);
	int version = 0;
	file >> version;
	return file ? version : 0;
}

void Setup(bool force)
{
	const fs::path root = FileSystem_GetGameDirectory();
	const fs::path stamp = root / BaseFolder / StampFile;

	if (!force && ReadStamp(stamp) == SetupVersion)
	{
		RefreshGraphs(root);
		return;
	}

	const auto start = std::chrono::steady_clock::now();
	SetupStats stats;
	Plan plan;

	g_DeferMessages = !force;

	PlanGameFiles(root, plan, stats);
	PlanMenu(root, plan);
	PlanGlassGibs(root, plan);

	for (const auto& [key, file] : plan.Files())
	{
		Write(file.first, file.second, stats);
	}

	ConvertMaps(root, stats);
	RefreshGraphs(root);

	std::string keys;

	if (ReadFile(root / BaseFolder / "gfx" / "shell" / "kb_def.lst", keys) || ReadFile(root / "valve" / "gfx" / "shell" / "kb_def.lst", keys))
	{
		std::string defaults = "unbindall\n";
		std::istringstream lines(keys);

		for (std::string line; std::getline(lines, line);)
		{
			std::vector<std::string> quoted;

			for (std::size_t start = line.find('"'); start != std::string::npos && quoted.size() < 2; start = line.find('"', start))
			{
				const std::size_t end = line.find('"', start + 1);

				if (end == std::string::npos)
					break;

				quoted.push_back(line.substr(start + 1, end - start - 1));
				start = end + 1;
			}

			if (quoted.size() == 2 && !quoted[0].empty() && !quoted[1].empty())
			{
				if (quoted[0] == "\\\\")
					quoted[0] = "\\";

				defaults += "bind \"" + quoted[0] + "\" \"" + quoted[1] + "\"\n";
			}
		}

		defaults += "+mlook\n";

		for (const auto& menu : MenuFolders)
		{
			const fs::path config = root / menu.Folder / "config.cfg";

			if (IsFolder(root / menu.Folder) && !IsFile(config))
				WriteContents(config, defaults, stats);
		}
	}

	const auto seconds = std::chrono::duration<float>(std::chrono::steady_clock::now() - start).count();

	Log("jhl_setup: %d files written, %d already in place, %d not in the installed games, %d failed (%.1f s)\n",
		stats.Written, stats.Unchanged, stats.Missing, stats.Failed, seconds);

	// A failure is tried again on the next start.
	if (stats.Failed == 0)
	{
		std::ofstream(stamp) << SetupVersion << '\n';
	}
}
} // namespace

void JhlSetup_Run()
{
	Setup(false);
}

void JhlSetup_Init()
{
	gEngfuncs.pfnAddCommand("jhl_setup", []()
		{ Setup(true); });
}

void JhlSetup_RunFrame()
{
	if (!g_DeferMessages || ++g_FrameCount < FramesBeforeMessages)
		return;

	g_DeferMessages = false;
	gEngfuncs.Con_Printf("%s", g_DeferredMessages.c_str());
	g_DeferredMessages.clear();
}
