/***
 *
 *	johnhalflife: the first start (docs/release.md#first-start).
 *
 ****/

#pragma once

/**
 *	@brief Sets John Half-Life's folders up from the player's own games, on the first start and after an update
 *	changes what it makes; otherwise does nothing. Called once the file system is loaded.
 */
void JhlSetup_Run();

/**
 *	@brief Adds jhl_setup, which runs the setup again whatever the stamp says.
 */
void JhlSetup_Init();

/**
 *	@brief Shows the first start's messages in the console, a few frames in (before then it doesn't keep them).
 */
void JhlSetup_RunFrame();
