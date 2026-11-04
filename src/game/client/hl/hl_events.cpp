/***
 *
 *	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
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
#include "hud.h"
#include "event_api.h"
#include "pmtrace.h"
#include "ev_hldm.h"
#include "jhl_hitch.h"

/*
======================
Game_HookEvents

Associate script file name with callback functions. Note that the format is
 always the same.  Of course, a clever mod team could actually embed parameters, behavior
 into the actual .sc files and create a .sc file parser and hook their functionality through
 that.. i.e., a scripting system.

That was what we were going to do, but we ran out of time...oh well.
======================
*/
void Game_HookEvents()
{
	gEngfuncs.pfnHookEvent("events/glock1.sc", JHL_TIMED(EV_FireGlock1));
	gEngfuncs.pfnHookEvent("events/glock2.sc", JHL_TIMED(EV_FireGlock2));
	gEngfuncs.pfnHookEvent("events/shotgun1.sc", JHL_TIMED(EV_FireShotGunSingle));
	gEngfuncs.pfnHookEvent("events/shotgun2.sc", JHL_TIMED(EV_FireShotGunDouble));
	gEngfuncs.pfnHookEvent("events/mp5.sc", JHL_TIMED(EV_FireMP5));
	gEngfuncs.pfnHookEvent("events/mp52.sc", JHL_TIMED(EV_FireMP52));
	gEngfuncs.pfnHookEvent("events/python.sc", JHL_TIMED(EV_FirePython));
	gEngfuncs.pfnHookEvent("events/gauss.sc", JHL_TIMED(EV_FireGauss));
	gEngfuncs.pfnHookEvent("events/gaussspin.sc", JHL_TIMED(EV_SpinGauss));
	gEngfuncs.pfnHookEvent("events/crowbar.sc", JHL_TIMED(EV_Crowbar));
	gEngfuncs.pfnHookEvent("events/crossbow1.sc", JHL_TIMED(EV_FireCrossbow));
	gEngfuncs.pfnHookEvent("events/crossbow2.sc", JHL_TIMED(EV_FireCrossbow2));
	gEngfuncs.pfnHookEvent("events/rpg.sc", JHL_TIMED(EV_FireRpg));
	gEngfuncs.pfnHookEvent("events/egon_fire.sc", JHL_TIMED(EV_EgonFire));
	gEngfuncs.pfnHookEvent("events/egon_stop.sc", JHL_TIMED(EV_EgonStop));
	gEngfuncs.pfnHookEvent("events/firehornet.sc", JHL_TIMED(EV_HornetGunFire));
	gEngfuncs.pfnHookEvent("events/tripfire.sc", JHL_TIMED(EV_TripmineFire));
	gEngfuncs.pfnHookEvent("events/snarkfire.sc", JHL_TIMED(EV_SnarkFire));

	gEngfuncs.pfnHookEvent("events/eagle.sc", JHL_TIMED(EV_FireEagle));
	gEngfuncs.pfnHookEvent("events/pipewrench.sc", JHL_TIMED(EV_Pipewrench));
	gEngfuncs.pfnHookEvent("events/m249.sc", JHL_TIMED(EV_FireM249));
	gEngfuncs.pfnHookEvent("events/displacer.sc", JHL_TIMED(EV_FireDisplacer));
	gEngfuncs.pfnHookEvent("events/shock.sc", JHL_TIMED(EV_FireShockRifle));
	gEngfuncs.pfnHookEvent("events/spore.sc", JHL_TIMED(EV_FireSpore));
	gEngfuncs.pfnHookEvent("events/sniper.sc", JHL_TIMED(EV_SniperRifle));
	gEngfuncs.pfnHookEvent("events/knife.sc", JHL_TIMED(EV_Knife));
	gEngfuncs.pfnHookEvent("events/penguinfire.sc", JHL_TIMED(EV_PenguinFire));
}
