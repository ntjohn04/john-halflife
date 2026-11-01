/***
 *
 *	johnhalflife: the first start (docs/johnhalflife/release.md#first-start).
 *
 *	The release ships johnhalflife's files and the Unified SDK's; what's Valve's comes from the player's own games
 *	(valve, gearbox, bshift, next to the mod). On the first start, and whenever SetupVersion is raised, this puts
 *	what the mod folders need from them in place and writes the version to jhl_setup.txt so later starts skip it:
 *
 *	* the files in jhl_setup_files.txt (made by scripts/make_setup_files.py): Opposing Force's and Blue Shift's
 *	  files, their soundtracks, each folder's own versions of files the games share names for;
 *	* the menu: Half-Life's main menu without its hint text, its background, logo and icon, then each folder's own
 *	  Valve game's over them, so they match the player's version of the game;
 *	* the maps in jhl_setup_maps.txt, converted for the Unified SDK's game code (jhl_mapconvert.cpp).
 *
 *	It first works out every file it would write, then writes only those that differ from what's there, so running
 *	it again (jhl_setup) changes nothing on a folder set up before, and running it on a development install checks
 *	it against the install.
 *
 ****/

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <system_error>
#include <variant>
#include <vector>

#include "hud.h"
#include "cl_util.h"
#include "jhl_mapconvert.h"
#include "jhl_setup.h"
#include "utils/filesystem_utils.h"

namespace fs = std::filesystem;

namespace
{
// Raise when what the setup makes changes, so installed folders are set up again.
constexpr int SetupVersion = 5;

constexpr const char* BaseFolder = "johnhalflife";
constexpr const char* StampFile = "jhl_setup.txt";
constexpr const char* FilesList = "jhl_setup_files.txt";
constexpr const char* MapsList = "jhl_setup_maps.txt";

// The setup runs as the client loads, before the console keeps anything: its messages wait a few frames, as
// jhl_settings.cpp's first launch message does.
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

// Each game folder's menu: its own Valve game's art over Half-Life's, and whether it has Half-Life's widescreen
// background (hd_background in its liblist.gam).
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

// What a file is made from: another file, or contents worked out here.
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

// The files to write, by path in lower case: the games name the same file in different cases (Opposing Force's
// game_menu.TGA is Half-Life's game_menu.tga), and a later source replaces an earlier one, keeping the first name.
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

// The files in a folder whose names start and end as given (any case), as a file name pattern "start*end".
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

// The files from the player's games: each line the game folder, the file, then where it comes from, in order.
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

// Half-Life's main menu, shared by every folder, without the 25th anniversary's hint text next to each button.
bool MakeGameMenu(const fs::path& valve, std::string& menu)
{
	std::string original;

	if (!ReadFile(valve / "resource" / "GameMenu.res", original))
	{
		return false;
	}

	std::size_t start = 0;

	while (start < original.size())
	{
		std::size_t end = original.find('\n', start);
		end = end == std::string::npos ? original.size() : end + 1;

		const std::string_view line(original.data() + start, end - start);
		const std::size_t text = line.find_first_not_of(" \t");

		if (text == std::string_view::npos || !line.substr(text).starts_with("\"HelpText\""))
		{
			menu += line;
		}

		start = end;
	}

	return true;
}

// The menu art, Half-Life's first and then the folder's own game's over it.
void PlanMenu(const fs::path& root, Plan& plan)
{
	const fs::path valve = root / "valve";

	if (!IsFolder(valve / "resource"))
	{
		Log("jhl_setup: Half-Life's resource folder not found; the menus keep what they have\n");
		return;
	}

	std::string gameMenu;
	const bool haveGameMenu = MakeGameMenu(valve, gameMenu);

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

			for (const char* icon : {"game.ico", "game.tga"})
			{
				if (IsFile(game / icon))
				{
					plan.Add(target / icon, game / icon);
				}
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

// The maps, converted one at a time (together they're hundreds of megabytes): each line the game folder and the map.
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

		const fs::path bsp = root / BaseFolder / "maps" / (map + ".bsp");
		WriteContents(bsp, converted, stats);

		// The map's node graph (the release has the Unified SDK's) must be newer than the map, or the game takes it
		// for out of date and builds it again the first time the map loads (server nodes.cpp CheckNODFile).
		const fs::path graph = root / BaseFolder / "maps" / "graphs" / (map + ".nod");
		std::error_code timeError;

		if (IsFile(graph) && fs::last_write_time(graph, timeError) <= fs::last_write_time(bsp, timeError))
			fs::last_write_time(graph, fs::file_time_type::clock::now(), timeError);
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
		return;
	}

	const auto start = std::chrono::steady_clock::now();
	SetupStats stats;
	Plan plan;

	// On the first start (not jhl_setup in the game) the console isn't up yet: the messages wait for it.
	g_DeferMessages = !force;

	PlanGameFiles(root, plan, stats);
	PlanMenu(root, plan);

	for (const auto& [key, file] : plan.Files())
	{
		Write(file.first, file.second, stats);
	}

	ConvertMaps(root, stats);

	// The Unified SDK's key bindings for a game folder with no config of its own yet (the ContentInstaller did this);
	// never over the player's own.
	if (std::string defaults; ReadFile(root / BaseFolder / "default_config.cfg", defaults))
	{
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
