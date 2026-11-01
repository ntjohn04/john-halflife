# Files that Half-Life, Opposing Force and Blue Shift each have their own version of under the same name. The Unified
# SDK's asset installer leaves one game's in the mod folder for all of them (Opposing Force's scientist lines, for
# example), so each game folder gets its own Valve game's, copied from the player's own install and never committed.
# Expansion folders without their own copy borrow John Half-Life's (Half-Life's) through fallback_dir, as their
# Valve games borrow valve's.
# Runs at install time.

set(root "${CMAKE_INSTALL_PREFIX}/..")

set(files
	sound/barney/icanhear.wav
	sound/common/null.wav
	sound/misc/talk.wav
	sound/scientist/needsleep.wav
	sound/scientist/survival.wav
	sprites/640_logo.spr
	sprites/laserdot.spr
)

foreach(pair "valve;johnhalflife" "gearbox;johnhalflife_op4" "bshift;johnhalflife_bs")
	list(GET pair 0 source)
	list(GET pair 1 target)

	if (NOT EXISTS "${root}/${source}" OR NOT EXISTS "${root}/${target}")
		continue()
	endif()

	foreach(file ${files})
		if (EXISTS "${root}/${source}/${file}")
			get_filename_component(dir "${root}/${target}/${file}" DIRECTORY)
			file(MAKE_DIRECTORY "${dir}")
			file(COPY_FILE "${root}/${source}/${file}" "${root}/${target}/${file}" ONLY_IF_DIFFERENT)
		endif()
	endforeach()
endforeach()
