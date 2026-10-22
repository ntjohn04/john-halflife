/***
 *
 *	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
 *
 *	This product contains software technology licensed from Id
 *	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
 *	All Rights Reserved.
 *
 *   Use, distribution, and modification of this source code and/or resulting
 *   object code is restricted to non-commercial enhancements to products from
 *   Valve LLC.  All other use, distribution, or modification is prohibited
 *   without written permission from Valve LLC.
 *
 ****/

#pragma once

/**
 *	@brief Registers jhl_recommended and the cvars that track and request recommended settings.
 */
void JhlSettings_Initialize();

/**
 *	@brief Applies the recommended settings once per version, after the saved config has loaded,
 *	and again whenever the settings dialog's reset box is ticked. Call every frame.
 */
void JhlSettings_RunFrame();
