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

// The walk played at this of its speed (pm_shared.cpp's footsteps: 400 ms each walking); the others as made. In time,
// not by how far it moves: the player's speeds aren't the models' strides.
constexpr float WalkRate = 1.0f;

// Off the ground this long, or going up (a jump), the legs take the jump's pose; less is a step down.
constexpr float FallTime = 0.15f;

// Gordon before the HEV suit: Blue Shift's lab coat Gordon, precached by the server (W_Precache) where the player is
// Half-Life's own model. A monster's model: its origin at its feet, not the hull's middle.
constexpr const char* HalfLifePlayer = "models/player.mdl";
constexpr const char* LabCoatGordon = "models/gordon_scientist.mdl";

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

model_t* PickModel(cl_entity_t* player, int& index, bool& monster)
{
	index = player->curstate.modelindex;
	model_t* model = gEngfuncs.hudGetModelByIndex(index);
	monster = false;

	if (!model || model->type != mod_studio)
		return nullptr;

	// The player's model as the map's game has it (Unified's model replacement: Barney, Adrian), Gordon's lab coat
	// before the suit; none if the lab coat isn't precached (the map had no room, or no Blue Shift).
	if (0 == std::strcmp(model->name, HalfLifePlayer) && !gHUD.HasSuit())
	{
		index = gEngfuncs.pEventAPI->EV_FindModelIndex(LabCoatGordon);
		model = index > 0 ? gEngfuncs.hudGetModelByIndex(index) : nullptr;
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
	g_Offset = gEngfuncs.pfnRegisterVariable("jhl_debug_body_offset", "17", 0);
	g_Spine = gEngfuncs.pfnRegisterVariable("jhl_debug_body_spine", "4", 0);
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

	// The lab coat's monster jumping, crouching or in water: the player model's pose (its poses are only walking ones),
	// drawn on his bones (CStudioModelRenderer::JhlSetupBorrowedBones). The player model is his here (PickModel).
	const studiohdr_t* poseHeader = header;

	if (monster && (activity == ActHop || activity == ActCrouch || activity == ActCrouchIdle || activity == ActHover))
	{
		const auto* playerModel = static_cast<const studiohdr_t*>(IEngineStudio.Mod_Extradata(gEngfuncs.hudGetModelByIndex(player->curstate.modelindex)));

		if (playerModel && playerModel->numseq > 0)
			poseHeader = playerModel;
	}

	const int sequence = PickSequence(poseHeader, activity);
	const auto& description = Sequences(poseHeader)[sequence];
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

	// Through its loop in time (backwards walking backwards); a jump from leaving the ground, held at its end.
	if (activity == ActHop)
	{
		g_Cycle = std::min(g_AirTime * description.fps / frames, 1.f);
	}
	else
	{
		g_Cycle += direction * description.fps * frameTime / frames * (activity == ActWalk ? WalkRate : 1);
		g_Cycle -= std::floor(g_Cycle);
	}

	std::memset(&g_Body, 0, sizeof(g_Body));
	g_Body.model = model;
	g_Body.curstate.modelindex = modelIndex;
	g_Body.curstate.rendermode = kRenderNormal;
	g_Body.curstate.renderamt = 255;
	g_Body.curstate.sequence = sequence;
	g_Body.curstate.frame = g_Cycle * 256;

	if (poseHeader != header)
	{
		g_Body.curstate.sequence = PickSequence(header, ActIdle); // For any bone the player model hasn't.
		g_Body.curstate.iuser1 = player->curstate.modelindex;
		g_Body.curstate.iuser2 = sequence + 1;
		g_Body.curstate.fuser3 = g_Cycle * frames;
		g_Body.curstate.fuser4 = move.Crouched ? 18 : 36;
	}

	g_Body.curstate.animtime = static_cast<float>(time);
	g_Body.curstate.framerate = 0;
	g_Body.curstate.iuser4 = JhlBodyMarker;
	// Crouched, only its legs: its back would be in view. Jumping, its first spine bone too, the bottom of its torso:
	// its landing bends the rest over into view.
	g_Body.curstate.iuser3 = move.Crouched ? 0 : activity == ActHop ? 1 : static_cast<int>(g_Spine->value);
	g_Body.latched.prevsequence = g_Body.curstate.sequence;
	g_Body.angles = g_Body.curstate.angles = g_Body.latched.prevangles = Vector{0, legYaw, 0};

	const float twist = monster ? 127 : (AngleDifference(viewYaw, legYaw) / 4 + 30) / (60.f / 255);

	for (int i = 0; i < 4; ++i)
		g_Body.curstate.controller[i] = g_Body.latched.prevcontroller[i] = static_cast<byte>(std::clamp(twist, 0.f, 255.f));

	// Its origin above the player's feet (JhlBody_Place): the player model's is the hull's middle as the engine draws
	// players (its crouching poses made for the crouching hull's), the monster's its feet.
	g_Body.curstate.fuser1 = -move.FeetDepth + (monster ? 0 : (move.Crouched ? 18 : 36));
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
	// behind the eyes by jhl_debug_body_offset along where the player looks.
	const float yaw = entity->curstate.fuser2 * static_cast<float>(PI) / 180;
	const float offset = g_Offset->value;

	entity->origin = Vector{origin.x - std::cos(yaw) * offset, origin.y - std::sin(yaw) * offset, origin.z + entity->curstate.fuser1};
	entity->curstate.origin = entity->origin;

	return true;
}

void JhlBody_Pose(const cl_entity_t* entity, const studiohdr_t* header, float (*bones)[3][4], float (*lightBones)[3][4])
{
	// Its head and arms hidden from the shoulders (the view model's arms are the player's; its collarbones kept, Barney's
	// vest hanging from them), and its spine above its iuser3 bones over the hips with all above. Its bones come after
	// their parents.
	const auto* bone = reinterpret_cast<const mstudiobone_t*>(reinterpret_cast<const byte*>(header) + header->boneindex);
	const int count = std::min(header->numbones, MAXSTUDIOBONES);
	const int keptSpine = entity->curstate.iuser3;

	int cut[MAXSTUDIOBONES];		  // The first hidden bone it's under (itself), -1 if shown.
	int spineDepth[MAXSTUDIOBONES]{}; // Its place up the spine: 1 its first bone above the hips.

	for (int i = 0; i < count; ++i)
	{
		const int parent = bone[i].parent >= 0 && bone[i].parent < i ? bone[i].parent : -1;
		const bool spine = std::strstr(bone[i].name, "Spine") != nullptr;
		const char* name = bone[i].name;
		const std::size_t length = std::strlen(name);
		const bool limb = std::strstr(name, "Head") != nullptr || (length >= 4 && 0 == std::strcmp(name + length - 4, "Arm1"));

		spineDepth[i] = spine && parent >= 0 ? spineDepth[parent] + 1 : 0;

		if (parent >= 0 && cut[parent] >= 0)
			cut[i] = cut[parent];
		else
			cut[i] = limb || (spine && spineDepth[i] > keptSpine) ? i : -1;

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
}
