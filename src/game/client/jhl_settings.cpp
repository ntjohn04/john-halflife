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

// The dialog's settings are applied on first launch too (the defaults are the recommended values). Raise the version
// when changing the list so existing players get the new values once. 2: all of them, not only crosshair_scale,
// m_rawinput and jhl_gore (a clean install's config had someone's own settings: 60 fps, a strafe roll three times ours).
// 3: crosshair, the engine's default being off. 4: auto-aim off (sv_aim, on by the engine's default), field of
// view 100. 5: gamma 1.92 and brightness 0 (else a new game folder took its first config's, Half-Life's, often an
// old default: gamma 2.5, brightness 1; the engine's are 2.2 and 0, and gamma can't go below 1.8).
constexpr int RecommendedSettingsVersion = 5;

// Applied on first launch and by a reset: every setting listed in the John Settings dialog (config/mod/user.scr), and
// only those. Keep in sync with that file, whose defaults should match. jhl_gore: the content lock isn't used, so
// blood and gibs are on.
constexpr RecommendedSetting DialogSettings[] = {
	{"jhl_camera_bob", "1"},
	{"jhl_camera_viewroll", "1"},
	{"cl_rollangle", "0.65"},
	{"cl_rollspeed", "300"},
	{"jhl_camera_verticallag", "1"},
	{"jhl_camera_jumplandjolt", "1"},
	{"jhl_visuals_weaponlag", "1"},
	{"jhl_visuals_weaponwallpush", "1"},
	{"jhl_visuals_flushfaces", "1"},
	{"jhl_visuals_16bitcolor", "1"},
	{"jhl_visuals_softwarewater", "1"},
	{"jhl_visuals_renderfixes", "1"},
	{"jhl_weapons_smoothrecoil", "1"},
	{"jhl_physics_fpsfix", "1"},
	{"jhl_fx_bulletimpacts", "1"},
	{"jhl_fx_muzzleflash", "1"},
	{"jhl_fx_particlesystem", "1"},
	{"jhl_fx_tracers", "1"},
	{"jhl_hud_scale", "1"},
	{"jhl_snd_vanilla", "1"},
	{"jhl_snd_musicsaves", "1"},
	{"crosshair", "1"},
	{"crosshair_scale", "2"},
	{"default_fov", "100"},
	{"gamma", "1.92"},
	{"brightness", "0"},
	{"sv_aim", "0"},
	{"fps_override", "1"},
	{"fps_max", "240"},
	{"gl_vsync", "0"},
	{"m_rawinput", "1"},
	{"jhl_gore", "1"},
	{"mp_decals", "300"},
	{"cl_projectinfo_show", "0"},
	{"cl_bobtilt", "0"},
	{"cl_snd_hrtf_enabled", "0"},
	{"cl_snd_hrtf_implementation", ""},
};

// The engine runs config.cfg from the command buffer before the client's first frames; wait a few to be safe.
constexpr int FramesBeforeFirstLaunchCheck = 5;

// The content lock's settings: Valve's lock sets all four to 0 and unlocking sets them back to 1, nothing else.
constexpr const char* GoreSettings[] = {"violence_hblood", "violence_hgibs", "violence_ablood", "violence_agibs"};

cvar_t* g_RecommendedVersion = nullptr;
cvar_t* g_ParticleSystem = nullptr;
cvar_t* g_FpsFix = nullptr;
cvar_t* g_Gore = nullptr;
float g_AppliedGore = -1;
cvar_t* g_ResetRecommended = nullptr;
int g_FrameCount = 0;

void Apply(std::span<const RecommendedSetting> settings)
{
	for (const auto& setting : settings)
	{
		gEngfuncs.Cvar_Set(setting.Name, setting.Value);
	}
}

// jhl_gore stands in for the four content lock settings, so the dialog can show them as one checkbox.
void SyncGoreFromContentLock()
{
	bool allOn = true;

	for (const char* name : GoreSettings)
	{
		allOn = allOn && 0 != gEngfuncs.pfnGetCvarFloat(name);
	}

	gEngfuncs.Cvar_Set(g_Gore->name, allOn ? "1" : "0");
	g_AppliedGore = allOn ? 1 : 0;
}

void ApplyGoreIfChanged()
{
	if (g_Gore->value == g_AppliedGore)
	{
		return;
	}

	g_AppliedGore = g_Gore->value;

	for (const char* name : GoreSettings)
	{
		gEngfuncs.Cvar_Set(name, 0 != g_AppliedGore ? "1" : "0");
	}
}

void ApplyAllRecommended()
{
	Apply(DialogSettings);
	gEngfuncs.Con_Printf("johnhalflife: recommended settings applied\n");
}
}

bool JhlParticleSystemEnabled()
{
	return 0 != g_ParticleSystem->value;
}

bool JhlFpsFixEnabled()
{
	return 0 != g_FpsFix->value;
}

void JhlSettings_Initialize()
{
	// One switch for all of johnhalflife's particle system changes (see docs/johnhalflife/effects_plan.md).
	g_ParticleSystem = gEngfuncs.pfnRegisterVariable("jhl_fx_particlesystem", "1", FCVAR_ARCHIVE);

	// Half-Life's game code acting at any frame rate as it did at 72 fps (see jhl_framerate.h); the server reads it too.
	g_FpsFix = gEngfuncs.pfnRegisterVariable("jhl_physics_fpsfix", "1", FCVAR_ARCHIVE);

	// johnhalflife's tracers on their own (jhl_tracers.cpp); they also need the particle system.
	gEngfuncs.pfnRegisterVariable("jhl_fx_tracers", "1", FCVAR_ARCHIVE);

	// Archived, so it survives in config.cfg and the first launch values are only applied once per version.
	g_RecommendedVersion = gEngfuncs.pfnRegisterVariable("jhl_recommended_version", "0", FCVAR_ARCHIVE);

	// Ticked from the settings dialog; cleared again once applied.
	g_ResetRecommended = gEngfuncs.pfnRegisterVariable("jhl_reset_recommended", "0", 0);

	// Not archived: its value comes from the content lock settings, which the engine saves.
	g_Gore = gEngfuncs.pfnRegisterVariable("jhl_gore", "1", 0);

	gEngfuncs.pfnAddCommand("jhl_recommended", &ApplyAllRecommended);
}

void JhlSettings_RunFrame()
{
	if (g_FrameCount < FramesBeforeFirstLaunchCheck)
	{
		if (++g_FrameCount < FramesBeforeFirstLaunchCheck)
		{
			return;
		}

		SyncGoreFromContentLock();

		if (g_RecommendedVersion->value < RecommendedSettingsVersion)
		{
			Apply(DialogSettings);
			gEngfuncs.Cvar_Set(g_RecommendedVersion->name, std::to_string(RecommendedSettingsVersion).c_str());
			gEngfuncs.Con_Printf("johnhalflife: first launch, recommended settings applied\n");
		}
	}

	if (0 != g_ResetRecommended->value)
	{
		gEngfuncs.Cvar_Set(g_ResetRecommended->name, "0");
		ApplyAllRecommended();
	}

	ApplyGoreIfChanged();
}
