# Copies the Valve menu files John Half-Life uses unchanged from the player's own install into one of its game
# folders, so they always match the player's version of the game and are never committed: Half-Life's main menu,
# background layouts and tiles, logo and icon, then the folder's own Valve game's over them.
# Runs at install time with:
#	JHL_TARGET: the game folder, next to the install prefix (e.g. johnhalflife_op4)
#	JHL_OWN: optional, the matching Valve game folder (e.g. gearbox)
#	JHL_HD: copy Half-Life's widescreen background as well (for folders with hd_background in liblist.gam)

set(root "${CMAKE_INSTALL_PREFIX}/..")
set(target "${root}/${JHL_TARGET}")
set(valve "${root}/valve")

if (NOT EXISTS "${valve}/resource")
	message(WARNING "Half-Life not found next to the mod; ${JHL_TARGET} keeps the menu files it has")
	return()
endif()

# One main menu for every game.
file(COPY "${valve}/resource/GameMenu.res" DESTINATION "${target}/resource")

foreach(source valve ${JHL_OWN})
	set(from "${root}/${source}")

	if (NOT EXISTS "${from}/resource")
		message(WARNING "${source} not found next to the mod; ${JHL_TARGET} uses Half-Life's menu art")
		continue()
	endif()

	file(GLOB files "${from}/resource/Background*Layout.txt" "${from}/resource/game_menu*" "${from}/resource/menu_*")
	set(tiles "${from}/resource/background/800_*")

	if (JHL_HD AND source STREQUAL "valve")
		file(GLOB hdLayouts "${from}/resource/HD_Background*Layout.txt")
		list(APPEND files ${hdLayouts})
		set(tiles "${from}/resource/background/*")
	endif()

	if (files)
		file(COPY ${files} DESTINATION "${target}/resource")
	endif()

	file(GLOB tiles ${tiles})

	if (tiles)
		file(COPY ${tiles} DESTINATION "${target}/resource/background")
	endif()

	file(GLOB icons "${from}/game.ico" "${from}/game.tga")

	if (icons)
		file(COPY ${icons} DESTINATION "${target}")
	endif()
endforeach()
