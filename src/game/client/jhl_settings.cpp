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

// Recommended settings for johnhalflife (see docs/johnhalflife/README.md#settings).
// Vanilla settings are saved in config.cfg, so their defaults can't simply be changed without overriding what the
// player chose. Instead the recommended values are applied once, the first time a version of them is seen.

#include "hud.h"
#include "jhl_settings.h"

namespace
{
struct RecommendedSetting
{
	const char* Name;
	const char* Value;
};

// Applied on first launch. Raise the version when changing this list so existing players get the new values once.
constexpr int RecommendedSettingsVersion = 1;

constexpr RecommendedSetting FirstLaunchSettings[] = {
	{"crosshair_scale", "2"},
	{"m_rawinput", "1"},
	// The content lock isn't used; make sure blood and gibs are on.
	{"violence_ablood", "1"},
	{"violence_agibs", "1"},
	{"violence_hblood", "1"},
	{"violence_hgibs", "1"},
};

// Applied by a reset: every setting listed in the John Settings dialog (config/mod/user.scr), and only those.
// Keep in sync with that file, whose defaults should match.
constexpr RecommendedSetting DialogSettings[] = {
	{"jhl_camera_bob", "1"},
	{"jhl_camera_viewroll", "1"},
	{"cl_rollangle", "0.65"},
	{"cl_rollspeed", "300"},
	{"jhl_camera_verticallag", "1"},
	{"jhl_camera_jumplandjolt", "1"},
	{"jhl_visuals_weaponlag", "1"},
	{"jhl_visuals_weaponwallpush", "1"},
	{"jhl_weapons_smoothrecoil", "1"},
	{"jhl_fx_bulletimpacts", "1"},
	{"jhl_fx_muzzleflash", "1"},
	{"jhl_hud_scale", "1"},
	{"crosshair_scale", "2"},
	{"default_fov", "105"},
	{"fps_override", "1"},
	{"fps_max", "240"},
	{"gl_vsync", "0"},
	{"m_rawinput", "1"},
	{"mp_decals", "300"},
	{"cl_bobtilt", "0"},
	{"cl_snd_hrtf_enabled", "0"},
	{"cl_snd_hrtf_implementation", ""},
};

// The engine runs config.cfg from the command buffer before the client's first frames; wait a few to be safe.
constexpr int FramesBeforeFirstLaunchCheck = 5;

cvar_t* g_RecommendedVersion = nullptr;
cvar_t* g_ResetRecommended = nullptr;
int g_FrameCount = 0;

void Apply(std::span<const RecommendedSetting> settings)
{
	for (const auto& setting : settings)
	{
		gEngfuncs.Cvar_Set(setting.Name, setting.Value);
	}
}

void ApplyAllRecommended()
{
	Apply(DialogSettings);
	gEngfuncs.Con_Printf("johnhalflife: recommended settings applied\n");
}
}

void JhlSettings_Initialize()
{
	// Archived, so it survives in config.cfg and the first launch values are only applied once per version.
	g_RecommendedVersion = gEngfuncs.pfnRegisterVariable("jhl_recommended_version", "0", FCVAR_ARCHIVE);

	// Ticked from the settings dialog; cleared again once applied.
	g_ResetRecommended = gEngfuncs.pfnRegisterVariable("jhl_reset_recommended", "0", 0);

	gEngfuncs.pfnAddCommand("jhl_recommended", &ApplyAllRecommended);
}

void JhlSettings_RunFrame()
{
	if (g_FrameCount < FramesBeforeFirstLaunchCheck)
	{
		if (++g_FrameCount == FramesBeforeFirstLaunchCheck && g_RecommendedVersion->value < RecommendedSettingsVersion)
		{
			Apply(FirstLaunchSettings);
			gEngfuncs.Cvar_Set(g_RecommendedVersion->name, std::to_string(RecommendedSettingsVersion).c_str());
			gEngfuncs.Con_Printf("johnhalflife: first launch, recommended settings applied\n");
		}
	}

	if (0 != g_ResetRecommended->value)
	{
		gEngfuncs.Cvar_Set(g_ResetRecommended->name, "0");
		ApplyAllRecommended();
	}
}
