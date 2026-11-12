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

#include <algorithm>
#include <cmath>
#include <cstring>

#include "hud.h"
#include "const.h"
#include "com_model.h"
#include "studio.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "entity_types.h"
#include "event_api.h"
#include "Exports.h"
#include "pm_defs.h"
#include "r_studioint.h"
#include "jhl_body.h"

extern engine_studio_api_t IEngineStudio;

namespace
{
// The models' sequences by their activities (activity.h, the server's).
constexpr int ActIdle = 1;
constexpr int ActWalk = 3;
constexpr int ActRun = 4;
constexpr int ActHop = 7;
constexpr int ActCrouch = 17;
constexpr int ActCrouchIdle = 18;
constexpr int ActHover = 50;
// None of the engine's: on a ladder, walking's pose stepping as it climbs, up or down (a loop each LadderSteps strides
// climbed, LadderStride if the pose has none: a little slower than walking), held where it is while still; no arms
// (none of the games has a climbing pose: the hazard course hologram's "ladder" is her pointing at one).
constexpr int ActLadder = -2;
constexpr float LadderStride = 64;
constexpr float LadderSteps = 1.3f;

// As the server's CBasePlayer::SetAnimation: running above this speed, walking above nothing, the legs' turn flipped to
// walking backwards past this far from where the player looks (the engine's StudioProcessGait).
constexpr float RunSpeed = 220;
constexpr float StillSpeed = 5;
constexpr float BackwardsTurn = 120;

// How far behind the eyes it stands, so mostly its legs are seen looking down; crouched, a little further (its legs
// come forward).
constexpr float Offset = 14;
constexpr float CrouchedOffset = 4;

// Jumping, its arms no further through the player model's jump than this frame (of its 34), raised JumpArmsRaise degrees
// (negative: forward) and moved JumpArmsForward and JumpArmsUp units, closer to the eyes, as view models are: [0] both
// arms drawn (or none), [1] one (a one-handed weapon's free arm, swung back out of the weapon's way).
constexpr float JumpArmsFrame = 21;
constexpr float JumpArmsRaise[2] = {-45, 90};
constexpr float JumpArmsForward[2] = {4, 5};
constexpr float JumpArmsUp[2] = {-2, 0};

// Running, its arms (the lab coat's swing) raised RunArmsRaise degrees (positive: back) and moved RunArmsForward and
// RunArmsUp units: at a near-level view, more of the swing in sight than fingertips popping up.
constexpr float RunArmsRaise = 25;
constexpr float RunArmsForward = 1;
constexpr float RunArmsUp = 0;

// How many of its spine's bones are kept above the hips: all of them (its neck and head hidden).
constexpr int KeptSpine = 4;

// Crawling, a loop (two steps) in no more than this many seconds: slow (walk mode's crouching alone is 2.5), not far
// off its footsteps (500 ms apart at their slowest crouched, pm_shared.cpp's PM_UpdateStepSound).
constexpr float CrouchedSlowestLoop = 1.8f;

// Off the ground this long, or going up (a jump), the legs take the jump's pose; less is a step down.
constexpr float FallTime = 0.15f;

// Moving, it stays so until it's been still this long: walking along a wall, the speed can flicker to nothing and back.
constexpr float StillTime = 0.1f;

// The eyes' height above a standing player's origin (VEC_VIEW): in the air, it hangs from the eyes as a standing jump's.
constexpr float StandingEyeHeight = 28;

// Gordon before the HEV suit: Blue Shift's lab coat Gordon, precached by the server (W_Precache) where the player is
// Half-Life's own model. A monster's model: its origin at its feet, not the hull's middle.
constexpr const char* HalfLifePlayer = "models/player.mdl";
constexpr const char* BlueShiftPlayer = "models/bshift/player.mdl"; // Barney.
constexpr const char* OpposingForcePlayer = "models/op4/player.mdl"; // Adrian.
constexpr const char* LabCoatGordon = "models/gordon_scientist.mdl";

// Barney without his vest: Blue Shift's guard in his shirt and tie, no vest or helmet (Unified's dead_barney.mdl, despite
// its name with a walk, a jog and idles), likewise a monster's model. Blue Shift starts with the HUD; its vest is its
// armor: none left, he's in his shirt, and a vest picked up puts it on (a helmet alone or a charger doesn't).
constexpr const char* VestlessBarney = "models/dead_barney.mdl";
constexpr const char* ArmorVest = "item_armorvest";

// Adrian in boot camp before his vest (its suit, ofboot2): the recruits' training clothes (Opposing Force's recruit.mdl,
// its biped's newer names: JhlBoneName), likewise a monster's model. Only in boot camp: the campaign's Osprey (of0a0)
// and its crash (of1a1, before the suit's found again) have no suit either, but he's in his fatigues there.
constexpr const char* Recruit = "models/recruit.mdl";
constexpr const char* BootCampMaps = "maps/ofboot";

// Treading water, the pose the hazard course's hologram's as she treads water (holo.mdl's treadwater, both arms
// sculling; the player models' hold a weapon out, and with one out their back leaned into view), where it's precached
// (W_Precache).
constexpr const char* WaterArms = "models/holo.mdl";
constexpr const char* WaterArmsSequence = "treadwater";
struct Movement
{
	bool Valid = false;
	Vector Origin;
	Vector Velocity;
	float EyeHeight = 0; // Above the origin (view_ofs).
	float FeetDepth = 0; // Below it (the hull's bottom).
	bool Crouched = false; // Fully: going down, the camera lowers first, the hull standing.
	bool OnGround = false;
	int WaterLevel = 0;
	int MoveType = 0;
	bool Hidden = false; // Dead, a spectator, noclipping.
};

cvar_t* g_Enabled = nullptr;

Movement g_Movement;
cl_entity_t g_Body;
double g_LastTime = 0;
float g_Cycle = 0;	 // Through the legs' loop, 0 to 1.
float g_AirTime = 0; // Since leaving the ground.
float g_StillTime = 0; // Since it last moved.
float g_LegYaw = 0;	   // The way its legs last went.
bool g_Arms = false;   // Its arms drawn: no view model (no weapon, holstered), so nothing else is the player's arms.
bool g_LeftArm = false;	 // Its left arm drawn: both drawn, or a weapon held in the right hand only (OneHanded).
bool g_RightArm = false; // Its right arm drawn: both drawn, or the satchel itself (held in the left).
bool g_Crouching = false; // In a crouching pose (crouched, not in the air).

// Barney's vest (JhlBody_Armor). A map loaded, the first armor told decides: the same as before it, a level change, the
// vest as it was; otherwise (a save loaded) worn with any armor.
bool g_Vest = true;
bool g_ArmorKnown = false;
int g_LastArmor = -1;

float AngleDifference(float a, float b)
{
	float d = std::fmod(a - b, 360.f);

	if (d > 180)
		d -= 360;
	else if (d < -180)
		d += 360;

	return d;
}

const mstudioseqdesc_t* Sequences(const studiohdr_t* header)
{
	return reinterpret_cast<const mstudioseqdesc_t*>(reinterpret_cast<const byte*>(header) + header->seqindex);
}

int FindActivity(const studiohdr_t* header, int activity)
{
	const auto* sequences = Sequences(header);

	for (int i = 0; i < header->numseq; ++i)
	{
		if (sequences[i].activity == activity)
			return i;
	}

	return -1;
}

int FindSequence(const studiohdr_t* header, const char* name)
{
	const auto* sequences = Sequences(header);

	for (int i = 0; i < header->numseq; ++i)
	{
		if (0 == std::strcmp(sequences[i].label, name))
			return i;
	}

	return -1;
}

// The pose for the player's movement, and those it falls back to where a model hasn't it (the lab coat's monster has
// no jump or swim). Standing still, the player model's deep_idle, as the server's gait.
int PickSequence(const studiohdr_t* header, int activity)
{
	int sequence = FindActivity(header, activity);

	if (sequence < 0 && activity == ActCrouch)
		sequence = FindActivity(header, ActCrouchIdle);

	if (sequence < 0 || activity == ActIdle)
	{
		if (const int idle = FindSequence(header, "deep_idle"); idle >= 0 && activity == ActIdle)
			sequence = idle;
		else if (sequence < 0)
			sequence = FindActivity(header, ActIdle);
	}

	return std::max(sequence, 0);
}

model_t* PickModel(cl_entity_t* player, int& index, bool& monster)
{
	index = player->curstate.modelindex;
	model_t* model = gEngfuncs.hudGetModelByIndex(index);
	monster = false;

	if (!model || model->type != mod_studio)
		return nullptr;

	// The player's model as the map's game has it (Unified's model replacement: Barney, Adrian), Gordon's lab coat before
	// the suit, Barney's shirt without his vest; none if that isn't precached (the map had no room, or no Blue Shift for
	// the lab coat).
	const char* other = nullptr;

	if (0 == std::strcmp(model->name, HalfLifePlayer) && !gHUD.HasSuit())
		other = LabCoatGordon;
	else if (0 == std::strcmp(model->name, BlueShiftPlayer) && !g_Vest)
		other = VestlessBarney;
	else if (0 == std::strcmp(model->name, OpposingForcePlayer) && !gHUD.HasSuit())
	{
		const char* level = gEngfuncs.pfnGetLevelName();

		if (level && 0 == strnicmp(level, BootCampMaps, std::strlen(BootCampMaps)))
			other = Recruit;
	}

	if (other)
	{
		index = gEngfuncs.pEventAPI->EV_FindModelIndex(other);
		model = index > 0 ? gEngfuncs.hudGetModelByIndex(index) : nullptr;
		monster = true;

		if (!model || model->type != mod_studio)
			return nullptr;
	}

	return model;
}

// The view model's free hand, by its file (any campaign's copy): 'L' its left arm drawn (held in the right hand only),
// 'R' its right (the satchel itself, held in the left), 0 none (both hands on it).
char OneHanded(const model_t* viewModel, int sequence)
{
	static constexpr const char* RightHanded[] = {"v_crowbar.mdl", "v_pipe_wrench.mdl", "v_knife.mdl", "v_bgrap.mdl",
		"v_357.mdl", "v_satchel_radio.mdl", "v_hgun.mdl", "v_grenade.mdl", "v_squeak.mdl", "v_penguin.mdl", "v_shock.mdl"};

	const char* slash = std::strrchr(viewModel->name, '/');
	const char* file = slash ? slash + 1 : viewModel->name;

	// The grenade's pin pulled by its own left hand (pinpull, 2); the .357 reloaded with both (reload, 3).
	if ((0 == stricmp(file, "v_grenade.mdl") && sequence == 2) || (0 == stricmp(file, "v_357.mdl") && sequence == 3))
		return 0;

	for (const char* name : RightHanded)
	{
		if (0 == stricmp(file, name))
			return 'L';
	}

	return 0 == stricmp(file, "v_satchel.mdl") ? 'R' : 0;
}
}

void JhlBody_Initialize()
{
	g_Enabled = gEngfuncs.pfnRegisterVariable("jhl_visuals_firstpersonbody", "1", FCVAR_ARCHIVE);

}

void JhlBody_Moved(const playermove_t* move)
{
	if (0 == g_Enabled->value)
		return;

	g_Movement.Valid = true;
	g_Movement.Origin = move->origin;
	g_Movement.Velocity = move->velocity;
	g_Movement.EyeHeight = move->view_ofs.z;
	g_Movement.FeetDepth = -move->player_mins[std::clamp(move->usehull, 0, 3)].z;
	g_Movement.Crouched = (move->flags & FL_DUCKING) != 0;
	g_Movement.OnGround = move->onground != -1;
	g_Movement.WaterLevel = move->waterlevel;
	g_Movement.MoveType = move->movetype;
	g_Movement.Hidden = 0 != move->dead || 0 != move->spectator || 0 != move->iuser1 || move->movetype == MOVETYPE_NOCLIP;
}

void JhlBody_CreateEntities()
{
	const double time = gEngfuncs.GetClientTime();
	const float frameTime = static_cast<float>(std::clamp(time - g_LastTime, 0.0, 0.1));
	g_LastTime = time;

	if (0 == g_Enabled->value || !g_Movement.Valid || g_Movement.Hidden || 0 != CL_IsThirdPerson())
		return;

	cl_entity_t* player = gEngfuncs.GetLocalPlayer();

	if (!player || (player->curstate.effects & EF_NODRAW) != 0)
		return;

	int modelIndex = 0;
	bool monster = false;
	model_t* model = PickModel(player, modelIndex, monster);

	if (!model)
		return;

	const auto* header = static_cast<const studiohdr_t*>(IEngineStudio.Mod_Extradata(model));

	if (!header || header->numseq <= 0)
		return;

	// The pose: the movement as just predicted, changed at once (no blend), as Half-Life's movement is.
	const Movement& move = g_Movement;
	const float speed = move.MoveType == MOVETYPE_FLY ? move.Velocity.Length() : move.Velocity.Length2D();
	const bool inWater = move.WaterLevel >= 2;
	const bool inAir = !move.OnGround && !inWater && move.MoveType != MOVETYPE_FLY;

	g_AirTime = inAir ? g_AirTime + frameTime : 0;
	g_StillTime = speed > StillSpeed ? 0 : g_StillTime + frameTime;
	const bool moving = g_StillTime < StillTime;

	int activity = ActIdle;

	if (inWater)
		activity = ActHover; // Treading water, upright: swimming's pose lies along the view.
	else if (move.MoveType == MOVETYPE_FLY)
		activity = ActLadder;
	else if (inAir && (move.Velocity.z > 0 || g_AirTime > FallTime))
		activity = ActHop;
	else if (move.Crouched)
		activity = moving ? ActCrouch : ActCrouchIdle;
	else if (speed > RunSpeed && move.MoveType != MOVETYPE_FLY)
		activity = ActRun;
	else if (moving)
		activity = ActWalk;

	// A crouching jump jumps as a standing one: its pose, arms and back, hung from the eyes (StandingEyeHeight).
	g_Crouching = move.Crouched && activity != ActHop;

	// No view model (no weapon, holstered): its arms are drawn, nothing else being the player's (below).
	const cl_entity_t* viewModel = gEngfuncs.GetViewModel();
	g_Arms = (!viewModel || !viewModel->model) && !g_Crouching && activity != ActLadder;
	const char freeHand = viewModel && viewModel->model && !g_Crouching && activity != ActLadder ? OneHanded(viewModel->model, viewModel->curstate.sequence) : 0;
	g_LeftArm = g_Arms || freeHand == 'L';
	g_RightArm = g_Arms || freeHand == 'R';

	// The hologram's treading water, for its pose in water.
	int waterArmsIndex = 0;
	const studiohdr_t* waterArms = nullptr;
	int waterArmsSequence = -1;

	if (activity == ActHover)
	{
		waterArmsIndex = gEngfuncs.pEventAPI->EV_FindModelIndex(WaterArms);
		model_t* holo = waterArmsIndex > 0 ? gEngfuncs.hudGetModelByIndex(waterArmsIndex) : nullptr;
		waterArms = holo && holo->type == mod_studio ? static_cast<const studiohdr_t*>(IEngineStudio.Mod_Extradata(holo)) : nullptr;
		waterArmsSequence = waterArms ? FindSequence(waterArms, WaterArmsSequence) : -1;

		if (waterArmsSequence < 0)
			waterArms = nullptr;
	}

	// The lab coat's monster jumping, crouching or in water: the player model's pose (its poses are only walking ones),
	// drawn on his bones (CStudioModelRenderer::JhlSetupBorrowedBones). The player model is his here (PickModel). In
	// water every model takes the hologram's whole pose, her legs too, its arms drawn or not (one pose is borrowed at a
	// time; her arms over the player models' own treading legs looked worse).
	const studiohdr_t* poseHeader = header;
	int poseIndex = player->curstate.modelindex;
	int sequence = -1;

	if (waterArms)
	{
		poseHeader = waterArms;
		poseIndex = waterArmsIndex;
		sequence = waterArmsSequence;
	}
	else if (monster && (activity == ActHop || activity == ActCrouch || activity == ActCrouchIdle || activity == ActHover))
	{
		const auto* playerModel = static_cast<const studiohdr_t*>(IEngineStudio.Mod_Extradata(gEngfuncs.hudGetModelByIndex(player->curstate.modelindex)));

		if (playerModel && playerModel->numseq > 0)
			poseHeader = playerModel;
	}

	if (sequence < 0)
		sequence = PickSequence(poseHeader, activity == ActLadder ? ActWalk : activity);

	const auto& description = Sequences(poseHeader)[sequence];
	const float frames = static_cast<float>(std::max(description.numframes - 1, 1));

	// The player models' upper bodies hold a weapon out in every pose, lunging and twisting the back as they run; standing,
	// walking and running, they take the lab coat's instead (his own poses, arms at his sides), half a loop on (his loops
	// start on the other foot), where it's precached (W_Precache), a weapon out or not (its arms hidden then). Crouching,
	// none: their crouching arms hold one out too; treading water, the hologram's whole pose (above).
	const studiohdr_t* armsHeader = nullptr;
	int armsIndex = 0;
	int armsSequence = -1;

	// A monster's model (the lab coat, Barney's shirt), running, its own (for its arms moved, below).
	if (!g_Crouching && (monster ? activity == ActRun : (activity == ActIdle || activity == ActWalk || activity == ActRun)))
	{
		armsIndex = monster ? modelIndex : gEngfuncs.pEventAPI->EV_FindModelIndex(LabCoatGordon);
		model_t* labCoat = armsIndex > 0 ? gEngfuncs.hudGetModelByIndex(armsIndex) : nullptr;
		armsHeader = labCoat && labCoat->type == mod_studio ? static_cast<const studiohdr_t*>(IEngineStudio.Mod_Extradata(labCoat)) : nullptr;

		if (armsHeader && armsHeader->numseq <= 0)
			armsHeader = nullptr;
	}

	// The legs turned the way it moves (backwards past BackwardsTurn), the player model's back turned the rest of the way
	// to where it looks (its four spine controllers, as the engine's StudioProcessGait); still, the way it looks.
	Vector viewAngles;
	gEngfuncs.GetViewAngles(viewAngles);
	const float viewYaw = viewAngles[YAW];
	float legYaw = viewYaw;
	float direction = 1;

	if (moving && !inWater && move.MoveType != MOVETYPE_FLY)
	{
		// The way it goes; a moment still (StillTime), the way it last went.
		if (speed > StillSpeed)
			g_LegYaw = std::atan2(move.Velocity.y, move.Velocity.x) * 180 / static_cast<float>(PI);

		legYaw = g_LegYaw;

		if (std::abs(AngleDifference(legYaw, viewYaw)) > BackwardsTurn)
		{
			legYaw += 180;
			direction = -1;
		}
	}

	// Through its loop in time, walking and running as made (their strides far shorter than the player's speeds, their
	// steps as the footsteps); crawling as far as it moves, its stride near a crouched player's (a third of the speed),
	// slower in walk mode but no slower than its footsteps (CrouchedSlowestLoop); backwards walking backwards. A jump from
	// leaving the ground, held at its end.
	const float stride = description.linearmovement.Length();

	if (activity == ActHop)
		g_Cycle = std::min(g_AirTime * description.fps / frames, 1.f);
	else if (activity == ActLadder)
		g_Cycle += move.Velocity.z / ((stride > 0 ? stride : LadderStride) * LadderSteps) * frameTime;
	else if (activity == ActCrouch && stride > 0)
		g_Cycle += direction * std::max(speed / stride, 1 / CrouchedSlowestLoop) * frameTime;
	else
		g_Cycle += direction * description.fps * frameTime / frames;

	if (activity != ActHop)
		g_Cycle -= std::floor(g_Cycle);

	std::memset(&g_Body, 0, sizeof(g_Body));
	g_Body.model = model;
	g_Body.curstate.modelindex = modelIndex;
	g_Body.curstate.rendermode = kRenderNormal;
	g_Body.curstate.renderamt = 255;
	g_Body.curstate.sequence = sequence;
	g_Body.curstate.frame = g_Cycle * 256;

	// Jumping, the player model's pose without its root's motion (playerclass 2), its own drawn as the lab coat's is: the
	// jump has its arc in it, the whole body rising 20 units, sinking 37 below standing as it lands, and leaning 29 degrees
	// forward, its neck and shoulders brought under the eyes and far below them (a fall's wide view of its back).
	if (poseHeader != header || activity == ActHop)
	{
		g_Body.curstate.sequence = PickSequence(header, ActIdle); // For any bone the player model hasn't.
		g_Body.curstate.iuser1 = poseIndex;
		g_Body.curstate.iuser2 = sequence + 1;
		g_Body.curstate.fuser3 = g_Cycle * frames;
		g_Body.curstate.fuser4 = g_Crouching ? 18 : 36;

		// None of its rise or sinking; its back upright; its arms held at JumpArmsFrame, spread out (after it they come
		// back in to hold a weapon: the right hand's grip in a fall, which holds its last frame), and moved and raised
		// forward, closer to the eyes, as view models are (the upright shoulders are further back than the leaning ones
		// they hung from).
		if (activity == ActHop)
		{
			g_Body.curstate.playerclass = 2;
			g_Body.curstate.vuser1[1] = JumpArmsFrame;
			const int one = g_LeftArm != g_RightArm ? 1 : 0;
			g_Body.curstate.vuser1[2] = JumpArmsRaise[one];
			g_Body.curstate.vuser2[0] = JumpArmsForward[one];
			g_Body.curstate.vuser2[1] = JumpArmsUp[one];
		}
	}
	else if (armsHeader)
	{
		if (armsSequence < 0)
			armsSequence = PickSequence(armsHeader, activity);

		const auto& arms = Sequences(armsHeader)[armsSequence];
		const float armsFrames = static_cast<float>(std::max(arms.numframes - 1, 1));
		const float armsCycle = activity == ActIdle || monster ? g_Cycle : g_Cycle + 0.5f;

		g_Body.curstate.iuser1 = armsIndex;
		g_Body.curstate.iuser2 = armsSequence + 1;
		g_Body.curstate.fuser3 = (armsCycle - std::floor(armsCycle)) * armsFrames;
		g_Body.curstate.playerclass = 1; // Only its upper body.

		if (activity == ActRun)
		{
			g_Body.curstate.vuser1[2] = RunArmsRaise;
			g_Body.curstate.vuser2[0] = RunArmsForward;
			g_Body.curstate.vuser2[1] = RunArmsUp;
		}
	}

	g_Body.curstate.animtime = static_cast<float>(time);
	g_Body.curstate.framerate = 0;
	g_Body.curstate.iuser4 = JhlBodyMarker;
	// Crouched, only its legs: its back would be in view. Jumping (its back upright), with its arms drawn all its spine,
	// shoulders and arms moving together; with a weapon out, as much as shows no shoulders moving without the view model's
	// hands: the HEV suit's first three, Barney's and the lab coat's first two, Adrian's all (with three, his belly
	// vanishes; with all, his shoulders move, the better of the two).
	int jumpSpine = KeptSpine;

	if (!g_Arms)
	{
		if (monster)
			jumpSpine = 2;
		else if (0 == std::strcmp(model->name, HalfLifePlayer))
			jumpSpine = 3;
		else if (0 == std::strcmp(model->name, BlueShiftPlayer))
			jumpSpine = 3;
		else if (0 == std::strcmp(model->name, OpposingForcePlayer))
			jumpSpine = 4;
		else
			jumpSpine = 2;
	}

	g_Body.curstate.iuser3 = g_Crouching ? 0 : activity == ActHop ? jumpSpine : KeptSpine;
	g_Body.latched.prevsequence = g_Body.curstate.sequence;
	g_Body.angles = g_Body.curstate.angles = g_Body.latched.prevangles = Vector{0, legYaw, 0};

	const float twist = monster ? 127 : (AngleDifference(viewYaw, legYaw) / 4 + 30) / (60.f / 255);

	for (int i = 0; i < 4; ++i)
		g_Body.curstate.controller[i] = g_Body.latched.prevcontroller[i] = static_cast<byte>(std::clamp(twist, 0.f, 255.f));

	// Its origin above the player's feet (JhlBody_Place): the player model's is the hull's middle as the engine draws
	// players (its crouching poses made for the crouching hull's), the monster's its feet. In the air, below the eyes as a
	// standing player's: crouching in a jump tucks the hull's feet up, the camera low in it.
	if (activity == ActHop)
		g_Body.curstate.fuser1 = move.EyeHeight - StandingEyeHeight - (monster ? 36 : 0);
	else
		g_Body.curstate.fuser1 = -move.FeetDepth + (monster ? 0 : (g_Crouching ? 18 : 36));
	g_Body.curstate.fuser2 = viewYaw;
	g_Body.origin = move.Origin;

	gEngfuncs.CL_CreateVisibleEntity(ET_TEMPENTITY, &g_Body);
}

bool JhlBody_Place(cl_entity_t* entity, const float* viewOrigin)
{
	// Drawn only from the player's own eyes: not a camera elsewhere (a map's, third person).
	const Vector& origin = g_Movement.Origin;
	const Vector eye = origin + Vector{0, 0, g_Movement.EyeHeight};
	const Vector view{viewOrigin[0], viewOrigin[1], viewOrigin[2]};

	if ((view - eye).Length() > 48)
		return false;

	// On the player's feet where the movement has them, whatever the camera's smoothing does (stairs, crouching, landing),
	// behind the eyes (Offset) along where the player looks.
	const float yaw = entity->curstate.fuser2 * static_cast<float>(PI) / 180;
	const float offset = Offset + (g_Crouching ? CrouchedOffset : 0);

	entity->origin = Vector{origin.x - std::cos(yaw) * offset, origin.y - std::sin(yaw) * offset, origin.z + entity->curstate.fuser1};
	entity->curstate.origin = entity->origin;

	return true;
}

void JhlBody_Pose(const cl_entity_t* entity, const studiohdr_t* header, float (*bones)[3][4], float (*lightBones)[3][4])
{
	// Its head hidden, and its arms from the shoulders while there's a view model (its arms are the player's; the
	// collarbones kept, Barney's vest hanging from them), and its spine above its iuser3 bones over the hips with all
	// above but its arms while they're drawn. Its bones come after their parents.
	const auto* bone = reinterpret_cast<const mstudiobone_t*>(reinterpret_cast<const byte*>(header) + header->boneindex);
	const int count = std::min(header->numbones, MAXSTUDIOBONES);
	const int keptSpine = entity->curstate.iuser3;

	int cut[MAXSTUDIOBONES];		  // The first hidden bone it's under (itself), -1 if shown.
	int spineDepth[MAXSTUDIOBONES]{}; // Its place up the spine: 1 its first bone above the hips.

	for (int i = 0; i < count; ++i)
	{
		const int parent = bone[i].parent >= 0 && bone[i].parent < i ? bone[i].parent : -1;
		const bool spine = std::strstr(bone[i].name, "Spine") != nullptr;
		char nameBuffer[32];
		const char* name = JhlBoneName(bone[i].name, nameBuffer); // The recruit's as the player models'.
		const std::size_t length = std::strlen(name);
		const bool head = std::strstr(name, "Head") != nullptr;
		const bool shown = std::strstr(name, " L ") ? g_LeftArm : std::strstr(name, " R ") ? g_RightArm : g_Arms;
		const bool arm = !shown && length >= 4 && 0 == std::strcmp(name + length - 4, "Arm1");
		const bool collarbone = length >= 6 && (0 == std::strcmp(name + length - 6, " L Arm") || 0 == std::strcmp(name + length - 6, " R Arm"));

		spineDepth[i] = spine && parent >= 0 ? spineDepth[parent] + 1 : 0;

		if (parent >= 0 && cut[parent] >= 0 && !(shown && collarbone))
			cut[i] = cut[parent];
		else
			cut[i] = head || arm || (spine && spineDepth[i] > keptSpine) ? i : -1;

		if (cut[i] < 0)
			continue;

		// Every bone hidden together shrunk to one point, where it's cut off: a triangle between them is nothing then, not
		// one stretched between their joints (a hand's, the head's), and the shown body closes up to that point.
		for (int row = 0; row < 3; ++row)
		{
			for (int column = 0; column < 3; ++column)
				bones[i][row][column] = lightBones[i][row][column] = 0;

			bones[i][row][3] = bones[cut[i]][row][3];
			lightBones[i][row][3] = lightBones[cut[i]][row][3];
		}
	}
}

void JhlBody_Reset()
{
	g_Movement = {};
	g_Cycle = 0;
	g_AirTime = 0;
	g_StillTime = 0;
	g_ArmorKnown = false;
}

void JhlBody_Armor(int armor)
{
	if (armor <= 0)
		g_Vest = false;
	else if (!g_ArmorKnown && armor != g_LastArmor)
		g_Vest = true;

	g_ArmorKnown = true;
	g_LastArmor = armor;
}

void JhlBody_ItemPickedUp(const char* className)
{
	if (0 == std::strcmp(className, ArmorVest))
		g_Vest = true;
}
