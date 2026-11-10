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

// As the server's CBasePlayer::SetAnimation: running above this speed, walking above nothing, the legs' turn flipped to
// walking backwards past this far from where the player looks (the engine's StudioProcessGait).
constexpr float RunSpeed = 220;
constexpr float StillSpeed = 5;
constexpr float BackwardsTurn = 120;

// Off the ground this long, or going up (a jump), the legs take the jump's pose; less is a step down.
constexpr float FallTime = 0.15f;

// Gordon before the HEV suit: Blue Shift's lab coat Gordon, precached by the server (W_Precache) where the player is
// Half-Life's own model. A monster's model: its origin at its feet, not the hull's middle.
constexpr const char* HalfLifePlayer = "models/player.mdl";
constexpr const char* LabCoatGordon = "models/gordon_scientist.mdl";

struct Movement
{
	bool Valid = false;
	Vector Velocity;
	float EyeHeight = 0; // Above the origin (view_ofs).
	float FeetDepth = 0; // Below it (the hull's bottom).
	bool Crouched = false;
	bool OnGround = false;
	int WaterLevel = 0;
	int MoveType = 0;
	bool Hidden = false; // Dead, a spectator, noclipping.
};

cvar_t* g_Enabled = nullptr;
cvar_t* g_Offset = nullptr; // How far behind the eyes it stands, so mostly the legs are seen looking down.
cvar_t* g_Spine = nullptr;	// How many of its spine's bones are kept above the hips.

Movement g_Movement;
cl_entity_t g_Body;
double g_LastTime = 0;
float g_Cycle = 0;	 // Through the legs' loop, 0 to 1.
float g_AirTime = 0; // Since leaving the ground.

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

model_t* PickModel(cl_entity_t* player, bool& monster)
{
	model_t* model = gEngfuncs.hudGetModelByIndex(player->curstate.modelindex);
	monster = false;

	if (!model || model->type != mod_studio)
		return nullptr;

	// The player's model as the map's game has it (Unified's model replacement: Barney, Adrian), Gordon's lab coat
	// before the suit; none if the lab coat isn't precached (the map had no room, or no Blue Shift).
	if (0 == std::strcmp(model->name, HalfLifePlayer) && !gHUD.HasSuit())
	{
		const int labCoat = gEngfuncs.pEventAPI->EV_FindModelIndex(LabCoatGordon);
		model = labCoat > 0 ? gEngfuncs.hudGetModelByIndex(labCoat) : nullptr;
		monster = true;

		if (!model || model->type != mod_studio)
			return nullptr;
	}

	return model;
}
}

void JhlBody_Initialize()
{
	g_Enabled = gEngfuncs.pfnRegisterVariable("jhl_visuals_firstpersonbody", "1", FCVAR_ARCHIVE);
	g_Offset = gEngfuncs.pfnRegisterVariable("jhl_debug_body_offset", "6", 0);
	g_Spine = gEngfuncs.pfnRegisterVariable("jhl_debug_body_spine", "2", 0);
}

void JhlBody_Moved(const playermove_t* move)
{
	if (0 == g_Enabled->value)
		return;

	g_Movement.Valid = true;
	g_Movement.Velocity = move->velocity;
	g_Movement.EyeHeight = move->view_ofs.z;
	g_Movement.FeetDepth = -move->player_mins[std::clamp(move->usehull, 0, 3)].z;
	g_Movement.Crouched = (move->flags & FL_DUCKING) != 0 || 0 != move->bInDuck;
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

	bool monster = false;
	model_t* model = PickModel(player, monster);

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

	int activity = ActIdle;

	if (inWater)
		activity = ActHover; // Treading water, upright: swimming's pose lies along the view.
	else if (inAir && (move.Velocity.z > 0 || g_AirTime > FallTime))
		activity = ActHop;
	else if (move.Crouched)
		activity = speed > StillSpeed ? ActCrouch : ActCrouchIdle;
	else if (speed > RunSpeed && move.MoveType != MOVETYPE_FLY)
		activity = ActRun;
	else if (speed > StillSpeed)
		activity = ActWalk;

	const int sequence = PickSequence(header, activity);
	const auto& description = Sequences(header)[sequence];
	const float frames = static_cast<float>(std::max(description.numframes - 1, 1));

	// The legs turned the way it moves (backwards past BackwardsTurn), the player model's back turned the rest of the way
	// to where it looks (its four spine controllers, as the engine's StudioProcessGait); still, the way it looks.
	Vector viewAngles;
	gEngfuncs.GetViewAngles(viewAngles);
	const float viewYaw = viewAngles[YAW];
	float legYaw = viewYaw;
	float direction = 1;

	if (speed > StillSpeed && !inWater && move.MoveType != MOVETYPE_FLY)
	{
		legYaw = std::atan2(move.Velocity.y, move.Velocity.x) * 180 / static_cast<float>(PI);

		if (std::abs(AngleDifference(legYaw, viewYaw)) > BackwardsTurn)
		{
			legYaw += 180;
			direction = -1;
		}
	}

	// Through its loop as far as it moved (its feet kept on the ground: its sequence's own move a loop), else in time; a
	// jump from leaving the ground, held at its end.
	if (activity == ActHop)
	{
		g_Cycle = std::min(g_AirTime * description.fps / frames, 1.f);
	}
	else
	{
		const float loop = description.linearmovement[0];
		g_Cycle += loop > 0 ? direction * speed * frameTime / loop : description.fps * frameTime / frames;
		g_Cycle -= std::floor(g_Cycle);
	}


	std::memset(&g_Body, 0, sizeof(g_Body));
	g_Body.model = model;
	g_Body.curstate.modelindex = player->curstate.modelindex;
	g_Body.curstate.rendermode = kRenderNormal;
	g_Body.curstate.renderamt = 255;
	g_Body.curstate.sequence = sequence;
	g_Body.curstate.frame = g_Cycle * 256;
	g_Body.curstate.animtime = static_cast<float>(time);
	g_Body.curstate.framerate = 0;
	g_Body.curstate.iuser4 = JhlBodyMarker;
	g_Body.latched.prevsequence = sequence;
	g_Body.angles = g_Body.curstate.angles = g_Body.latched.prevangles = Vector{0, legYaw, 0};

	const float twist = monster ? 127 : (AngleDifference(viewYaw, legYaw) / 4 + 30) / (60.f / 255);

	for (int i = 0; i < 4; ++i)
		g_Body.curstate.controller[i] = g_Body.latched.prevcontroller[i] = static_cast<byte>(std::clamp(twist, 0.f, 255.f));

	// Its origin below the camera (JhlBody_Place): the player model's is the hull's middle as the engine draws players
	// (its crouching poses made for the crouching hull's), the monster's its feet.
	const float feetBelowEye = move.EyeHeight + move.FeetDepth;
	g_Body.curstate.fuser1 = -feetBelowEye + (monster ? 0 : (move.Crouched ? 18 : 36));
	g_Body.curstate.fuser2 = viewYaw;
	g_Body.origin = player->origin;

	gEngfuncs.CL_CreateVisibleEntity(ET_TEMPENTITY, &g_Body);
}

bool JhlBody_Place(cl_entity_t* entity, const float* viewOrigin)
{
	cl_entity_t* player = gEngfuncs.GetLocalPlayer();

	// Drawn only from the player's own eyes: not a camera elsewhere (a map's, third person).
	if (!player)
		return false;

	const Vector eye = player->origin + Vector{0, 0, g_Movement.EyeHeight};
	const Vector view{viewOrigin[0], viewOrigin[1], viewOrigin[2]};

	if ((view - eye).Length() > 48)
		return false;

	// Hung under the camera, not the player's origin: the camera's smoothing (stairs, landing, bob) moves it too, so it
	// never rises into the view. Behind the eyes by jhl_debug_body_offset along where the player looks.
	const float yaw = entity->curstate.fuser2 * static_cast<float>(PI) / 180;
	const float offset = g_Offset->value;

	entity->origin = Vector{view.x - std::cos(yaw) * offset, view.y - std::sin(yaw) * offset, view.z + entity->curstate.fuser1};
	entity->curstate.origin = entity->origin;

	return true;
}

void JhlBody_Pose(const studiohdr_t* header, float (*bones)[3][4], float (*lightBones)[3][4])
{
	// Its neck and all above it shrunk to a point (its head, its arms: the view model's are the player's), and its spine
	// from jhl_debug_body_spine bones above the hips. Its bones come after their parents.
	const auto* bone = reinterpret_cast<const mstudiobone_t*>(reinterpret_cast<const byte*>(header) + header->boneindex);
	const int count = std::min(header->numbones, MAXSTUDIOBONES);
	const int keptSpine = static_cast<int>(g_Spine->value);

	bool hidden[MAXSTUDIOBONES]{};
	int spineDepth[MAXSTUDIOBONES]{}; // Its place up the spine: 1 its first bone above the hips.

	for (int i = 0; i < count; ++i)
	{
		const int parent = bone[i].parent;
		const bool spine = std::strstr(bone[i].name, "Spine") != nullptr;
		const bool neck = std::strstr(bone[i].name, "Neck") != nullptr;

		spineDepth[i] = spine && parent >= 0 ? spineDepth[parent] + 1 : 0;
		hidden[i] = (parent >= 0 && parent < count && hidden[parent]) || neck || (spine && spineDepth[i] > keptSpine);

		if (hidden[i])
		{
			for (int row = 0; row < 3; ++row)
			{
				for (int column = 0; column < 3; ++column)
					bones[i][row][column] = lightBones[i][row][column] = 0;
			}
		}
	}
}

void JhlBody_Reset()
{
	g_Movement = {};
	g_Cycle = 0;
	g_AirTime = 0;
}
