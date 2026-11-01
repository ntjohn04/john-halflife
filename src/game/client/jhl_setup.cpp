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
 *	  Valve game's over them, so they match the player's version of the game.
 *
 *	It first works out every file it would write, then writes only those that differ from what's there, so running
 *	it again (jhl_setup) changes nothing on a folder set up before, and running it on a development install checks
 *	it against the install.
 *
 ****/

#include <algorithm>
#include <chrono>
#include <cctype>
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
#include "jhl_setup.h"
#include "utils/filesystem_utils.h"

namespace fs = std::filesystem;

namespace
{
// Raise when what the setup makes changes, so installed folders are set up again.
constexpr int SetupVersion = 1;

constexpr const char* BaseFolder = "johnhalflife";
constexpr const char* StampFile = "jhl_setup.txt";
constexpr const char* FilesList = "jhl_setup_files.txt";

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
		gEngfuncs.Con_Printf("jhl_setup: %s/%s not found\n", BaseFolder, FilesList);
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
		gEngfuncs.Con_Printf("jhl_setup: Half-Life's resource folder not found; the menus keep what they have\n");
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
				gEngfuncs.Con_Printf("jhl_setup: %s not installed; %s uses Half-Life's menu art\n", menu.OwnGame, menu.Folder);
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
void Write(const fs::path& destination, const Source& source, SetupStats& stats)
{
	std::string contents;

	if (const auto path = std::get_if<fs::path>(&source))
	{
		if (!ReadFile(*path, contents))
		{
			gEngfuncs.Con_Printf("jhl_setup: couldn't read %s\n", path->generic_string().c_str());
			++stats.Failed;
			return;
		}
	}
	else
	{
		contents = std::get<std::string>(source);
	}

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
		gEngfuncs.Con_Printf("jhl_setup: couldn't write %s\n", destination.generic_string().c_str());
		++stats.Failed;
		return;
	}

	++stats.Written;
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

	PlanGameFiles(root, plan, stats);
	PlanMenu(root, plan);

	for (const auto& [key, file] : plan.Files())
	{
		Write(file.first, file.second, stats);
	}

	const auto seconds = std::chrono::duration<float>(std::chrono::steady_clock::now() - start).count();

	gEngfuncs.Con_Printf("jhl_setup: %d files written, %d already in place, %d not in the installed games, %d failed (%.1f s)\n",
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
