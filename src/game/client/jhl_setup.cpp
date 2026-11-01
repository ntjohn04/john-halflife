/***
 *
 *	johnhalflife: the first start (docs/johnhalflife/release.md#first-start).
 *
 *	The release ships johnhalflife's files and the Unified SDK's; what's Valve's comes from the player's own games
 *	(valve, gearbox, bshift, next to the mod). On the first start, and whenever SetupVersion is raised, this copies
 *	what the mod folders need from them (jhl_setup_files.txt, made by scripts/make_setup_files.py) and writes the
 *	version to jhl_setup.txt so later starts skip it.
 *
 *	Every step writes a file only if it differs from what it would write, so running it again (jhl_setup) changes
 *	nothing on a folder set up before, and running it on a development install checks it against the install.
 *
 ****/

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
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

struct SetupStats
{
	int Copied = 0;
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

bool SameContents(const fs::path& a, const fs::path& b)
{
	std::error_code error;

	if (!fs::exists(b, error) || fs::file_size(a, error) != fs::file_size(b, error))
	{
		return false;
	}

	std::string first, second;
	return ReadFile(a, first) && ReadFile(b, second) && first == second;
}

// Copies source to destination unless it's already there, as is.
void CopyIfDifferent(const fs::path& source, const fs::path& destination, SetupStats& stats)
{
	if (SameContents(source, destination))
	{
		++stats.Unchanged;
		return;
	}

	std::error_code error;
	fs::create_directories(destination.parent_path(), error);
	fs::copy_file(source, destination, fs::copy_options::overwrite_existing, error);

	if (error)
	{
		gEngfuncs.Con_Printf("jhl_setup: couldn't copy %s to %s (%s)\n",
			source.generic_string().c_str(), destination.generic_string().c_str(), error.message().c_str());
		++stats.Failed;
		return;
	}

	++stats.Copied;
}

// The files from the player's games: each line the game folder, the file, then where it comes from, in order.
void CopyGameFiles(const fs::path& root, SetupStats& stats)
{
	std::ifstream list(root / BaseFolder / FilesList);

	if (!list)
	{
		gEngfuncs.Con_Printf("jhl_setup: %s/%s not found\n", BaseFolder, FilesList);
		++stats.Failed;
		return;
	}

	std::error_code error;
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

		if (fields.size() < 3)
		{
			continue;
		}

		const fs::path folder = root / fields[0];

		// An expansion folder only exists if it was installed.
		if (!fs::is_directory(folder, error))
		{
			continue;
		}

		bool found = false;

		for (std::size_t i = 2; i < fields.size() && !found; ++i)
		{
			const fs::path source = root / fields[i];

			if (fs::is_regular_file(source, error))
			{
				CopyIfDifferent(source, folder / fields[1], stats);
				found = true;
			}
		}

		if (!found)
		{
			++stats.Missing;
		}
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

	CopyGameFiles(root, stats);

	const auto seconds = std::chrono::duration<float>(std::chrono::steady_clock::now() - start).count();

	gEngfuncs.Con_Printf("jhl_setup: %d files copied, %d already in place, %d not in the installed games, %d failed (%.1f s)\n",
		stats.Copied, stats.Unchanged, stats.Missing, stats.Failed, seconds);

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
