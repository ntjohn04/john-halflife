// Checks jhl_mapconvert.cpp's maps against the ContentInstaller's.
// Usage: mapconvert_check <Half-Life folder> <jhl_setup_maps.txt>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

#include "jhl_mapconvert.h"

static bool ReadFile(const std::string& path, std::string& contents)
{
	std::ifstream file(path, std::ios::binary);
	if (!file)
		return false;
	std::ostringstream stream;
	stream << file.rdbuf();
	contents = std::move(stream).str();
	return true;
}

static std::string UpToNul(const std::string& text)
{
	return text.substr(0, text.find('\0'));
}

// The first entity block that differs, from each.
static void ShowDifference(const std::string& a, const std::string& b)
{
	std::size_t i = 0;
	while (i < a.size() && i < b.size() && a[i] == b[i])
		++i;
	const std::size_t start = a.rfind('{', i) == std::string::npos ? 0 : a.rfind('{', i);
	const std::size_t endA = a.find('}', i), endB = b.find('}', i);
	std::printf("    ours:   %s\n", a.substr(start, endA == std::string::npos ? 300 : endA - start + 1).c_str());
	std::printf("    theirs: %s\n", b.substr(start, endB == std::string::npos ? 300 : endB - start + 1).c_str());
}

int main(int argc, char** argv)
{
	if (argc < 3)
	{
		std::printf("Usage: mapconvert_check <Half-Life folder> <jhl_setup_maps.txt>\n");
		return 2;
	}

	const std::string root = argv[1];
	std::ifstream list(argv[2]);
	std::string line;
	int same = 0, different = 0, failed = 0;

	while (std::getline(list, line))
	{
		if (line.empty() || line.starts_with("//"))
			continue;

		const auto tab = line.find('\t');
		const std::string game = line.substr(0, tab), map = line.substr(tab + 1);
		const JhlMapGame kind = game == "gearbox" ? JhlMapGame::OpposingForce : game == "bshift" ? JhlMapGame::BlueShift : JhlMapGame::HalfLife;

		std::string vanilla, theirs, ours, error, ourText, theirText;

		if (!ReadFile(root + "/" + game + "/maps/" + map + ".bsp", vanilla) || !ReadFile(root + "/johnhalflife/maps/" + map + ".bsp", theirs))
		{
			std::printf("%s: missing\n", map.c_str());
			++failed;
			continue;
		}

		if (!JhlMapConvert(vanilla, kind, map, ours, error) || !JhlMapConvert_EntityText(ours, ourText) || !JhlMapConvert_EntityText(theirs, theirText))
		{
			std::printf("%s: %s\n", map.c_str(), error.c_str());
			++failed;
			continue;
		}

		bool lumpsSame = true;
		{
			std::int32_t v[30], o[30];
			std::memcpy(v, vanilla.data() + 4, sizeof(v));
			std::memcpy(o, ours.data() + 4, sizeof(o));
			const bool swapped = vanilla[v[0]] != '{' && vanilla[v[2]] == '{';
			for (int i = 1; i < 15; ++i)
			{
				const int src = swapped && i == 1 ? 0 : i;
				if (vanilla.compare(v[src * 2], v[src * 2 + 1], ours, o[i * 2], o[i * 2 + 1]) != 0)
					lumpsSame = false;
			}
		}

		if (UpToNul(ourText) == UpToNul(theirText) && lumpsSame)
		{
			++same;
			continue;
		}

		++different;
		std::printf("%s (%s): entities %s, other lumps %s\n", map.c_str(), game.c_str(),
			UpToNul(ourText) == UpToNul(theirText) ? "same" : "DIFFER", lumpsSame ? "same" : "DIFFER");

		if (UpToNul(ourText) != UpToNul(theirText))
			ShowDifference(UpToNul(ourText), UpToNul(theirText));
	}

	std::printf("%d maps the same, %d different, %d failed\n", same, different, failed);
	return different || failed ? 1 : 0;
}
