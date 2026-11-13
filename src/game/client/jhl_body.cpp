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
constexpr int ActIdle = 1;
constexpr int ActWalk = 3;
constexpr int ActRun = 4;
constexpr int ActHop = 7;
constexpr int ActCrouch = 17;
constexpr int ActCrouchIdle = 18;
constexpr int ActHover = 50;
constexpr int ActLadder = -2;
constexpr float LadderStride = 64;
constexpr float LadderSteps = 1.3f;

constexpr float RunSpeed = 220;
constexpr float StillSpeed = 5;
constexpr float BackwardsTurn = 120;

constexpr float Offset = 14;
constexpr float CrouchedOffset = 4;

constexpr float JumpArmsFrame = 21;
constexpr float JumpArmsRaise[2] = {-45, 90};
constexpr float JumpArmsForward[2] = {4, 5};
constexpr float JumpArmsUp[2] = {-2, 0};

constexpr float RunArmsRaise = 25;
constexpr float RunArmsForward = 1;
constexpr float RunArmsUp = 0;

constexpr int KeptSpine = 4;

constexpr float CrouchedSlowestLoop = 1.8f;

constexpr float FallTime = 0.15f;

constexpr float StillTime = 0.1f;

constexpr float StandingEyeHeight = 28;

constexpr const char* HalfLifePlayer = "models/player.mdl";
constexpr const char* BlueShiftPlayer = "models/bshift/player.mdl"; // Barney.
constexpr const char* OpposingForcePlayer = "models/op4/player.mdl"; // Adrian.
constexpr const char* LabCoatGordon = "models/gordon_scientist.mdl";

constexpr const char* VestlessBarney = "models/dead_barney.mdl";
constexpr const char* ArmorVest = "item_armorvest";

constexpr const char* Recruit = "models/recruit.mdl";
constexpr const char* BootCampMaps = "maps/ofboot";

constexpr const char* AtEaseMap = "maps/ofboot0.bsp";

constexpr const char* WaterArms = "models/holo.mdl";
constexpr const char* WaterArmsSequence = "treadwater";
struct Movement
{
	bool Valid = false;
	Vector Origin;
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

Movement g_Movement;
cl_entity_t g_Body;
double g_LastTime = 0;
float g_Cycle = 0;
float g_AirTime = 0; // Since leaving the ground.
float g_StillTime = 0; // Since it last moved.
float g_LegYaw = 0;	   // The way its legs last went.
bool g_Arms = false;
bool g_LeftArm = false;
bool g_RightArm = false;
bool g_Crouching = false;
int g_HandsIndex = 0;

bool g_Vest = true;
bool g_ArmorKnown = false;
int g_LastArmor = -1;

bool g_AtEase = false;
bool g_AtEaseUndecided = true;

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

model_t* PickModel(cl_entity_t* player, int& index, bool& monster, bool& dressed)
{
	index = player->curstate.modelindex;
	model_t* model = gEngfuncs.hudGetModelByIndex(index);
	monster = false;
	dressed = false;

	if (!model || model->type != mod_studio)
		return nullptr;

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
		dressed = other == Recruit;

		if (!model || model->type != mod_studio)
			return nullptr;
	}

	return model;
}

char OneHanded(const model_t* viewModel, int sequence)
{
	static constexpr const char* RightHanded[] = {"v_crowbar.mdl", "v_pipe_wrench.mdl", "v_knife.mdl", "v_bgrap.mdl",
		"v_357.mdl", "v_satchel_radio.mdl", "v_hgun.mdl", "v_grenade.mdl", "v_squeak.mdl", "v_penguin.mdl", "v_shock.mdl"};

	const char* slash = std::strrchr(viewModel->name, '/');
	const char* file = slash ? slash + 1 : viewModel->name;

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
	g_HandsIndex = 0;

	if (0 == g_Enabled->value || !g_Movement.Valid || g_Movement.Hidden || 0 != CL_IsThirdPerson())
		return;

	cl_entity_t* player = gEngfuncs.GetLocalPlayer();

	if (!player || (player->curstate.effects & EF_NODRAW) != 0)
		return;

	int modelIndex = 0;
	bool monster = false;
	bool dressed = false;
	model_t* model = PickModel(player, modelIndex, monster, dressed);

	if (!model)
		return;

	const auto* header = static_cast<const studiohdr_t*>(IEngineStudio.Mod_Extradata(model));

	if (!header || header->numseq <= 0)
		return;

	if (0 == std::strcmp(model->name, VestlessBarney))
		g_HandsIndex = player->curstate.modelindex;

	const Movement& move = g_Movement;
	const float speed = move.MoveType == MOVETYPE_FLY ? move.Velocity.Length() : move.Velocity.Length2D();
	const bool inWater = move.WaterLevel >= 2;
	const bool inAir = !move.OnGround && !inWater && move.MoveType != MOVETYPE_FLY;

	g_AirTime = inAir ? g_AirTime + frameTime : 0;
	g_StillTime = speed > StillSpeed ? 0 : g_StillTime + frameTime;
	const bool moving = g_StillTime < StillTime;

	int activity = ActIdle;

	if (inWater)
		activity = ActHover;
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

	g_Crouching = move.Crouched && activity != ActHop;

	if (g_AtEaseUndecided)
	{
		const char* level = gEngfuncs.pfnGetLevelName();
		g_AtEase = level && 0 == stricmp(level, AtEaseMap);
		g_AtEaseUndecided = false;
	}

	if (speed > StillSpeed || move.Crouched || (inAir && move.Velocity.z > 0) || inWater || move.MoveType == MOVETYPE_FLY)
		g_AtEase = false;

	if (g_AtEase)
	{
		activity = ActIdle;
		g_Crouching = false;
	}

	const bool playerPoses = !monster || (dressed && !g_AtEase);

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

	const studiohdr_t* poseHeader = header;
	int poseIndex = player->curstate.modelindex;
	int sequence = -1;

	if (waterArms)
	{
		poseHeader = waterArms;
		poseIndex = waterArmsIndex;
		sequence = waterArmsSequence;
	}
	else if (monster && (playerPoses || activity == ActHop || activity == ActCrouch || activity == ActCrouchIdle || activity == ActHover))
	{
		const auto* playerModel = static_cast<const studiohdr_t*>(IEngineStudio.Mod_Extradata(gEngfuncs.hudGetModelByIndex(player->curstate.modelindex)));

		if (playerModel && playerModel->numseq > 0)
			poseHeader = playerModel;
	}

	if (sequence < 0)
		sequence = PickSequence(poseHeader, activity == ActLadder ? ActWalk : activity);

	const auto& description = Sequences(poseHeader)[sequence];
	const float frames = static_cast<float>(std::max(description.numframes - 1, 1));

	const studiohdr_t* armsHeader = nullptr;
	int armsIndex = 0;
	int armsSequence = -1;

	if (!g_Crouching && (playerPoses ? (activity == ActIdle || activity == ActWalk || activity == ActRun) : activity == ActRun))
	{
		armsIndex = playerPoses ? gEngfuncs.pEventAPI->EV_FindModelIndex(LabCoatGordon) : modelIndex;
		model_t* labCoat = armsIndex > 0 ? gEngfuncs.hudGetModelByIndex(armsIndex) : nullptr;
		armsHeader = labCoat && labCoat->type == mod_studio ? static_cast<const studiohdr_t*>(IEngineStudio.Mod_Extradata(labCoat)) : nullptr;

		if (armsHeader && armsHeader->numseq <= 0)
			armsHeader = nullptr;
	}

	Vector viewAngles;
	gEngfuncs.GetViewAngles(viewAngles);
	const float viewYaw = viewAngles[YAW];
	float legYaw = viewYaw;
	float direction = 1;

	if (moving && !inWater && move.MoveType != MOVETYPE_FLY)
	{
		if (speed > StillSpeed)
			g_LegYaw = std::atan2(move.Velocity.y, move.Velocity.x) * 180 / static_cast<float>(PI);

		legYaw = g_LegYaw;

		if (std::abs(AngleDifference(legYaw, viewYaw)) > BackwardsTurn)
		{
			legYaw += 180;
			direction = -1;
		}
	}

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

	const bool dressedBase = dressed && poseHeader != header && armsHeader && activity != ActHop;

	if ((poseHeader != header && !dressedBase) || activity == ActHop)
	{
		g_Body.curstate.sequence = PickSequence(header, ActIdle);
		g_Body.curstate.iuser1 = poseIndex;
		g_Body.curstate.iuser2 = sequence + 1;
		g_Body.curstate.fuser3 = g_Cycle * frames;
		g_Body.curstate.fuser4 = g_Crouching ? 18 : 36;

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
		const float armsCycle = activity == ActIdle || !playerPoses ? g_Cycle : g_Cycle + 0.5f;

		if (dressedBase)
		{
			g_Body.curstate.sequence = PickSequence(header, ActIdle);
			g_Body.curstate.vuser3 = Vector{static_cast<float>(poseIndex), static_cast<float>(sequence + 1), g_Cycle * frames};
		}

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
	const Vector& origin = g_Movement.Origin;
	const Vector eye = origin + Vector{0, 0, g_Movement.EyeHeight};
	const Vector view{viewOrigin[0], viewOrigin[1], viewOrigin[2]};

	if ((view - eye).Length() > 48)
		return false;

	const float yaw = entity->curstate.fuser2 * static_cast<float>(PI) / 180;
	const float offset = Offset + (g_Crouching ? CrouchedOffset : 0);

	entity->origin = Vector{origin.x - std::cos(yaw) * offset, origin.y - std::sin(yaw) * offset, origin.z + entity->curstate.fuser1};
	entity->curstate.origin = entity->origin;

	return true;
}

void JhlBody_Pose(const cl_entity_t* entity, const studiohdr_t* header, float (*bones)[3][4], float (*lightBones)[3][4])
{
	const auto* bone = reinterpret_cast<const mstudiobone_t*>(reinterpret_cast<const byte*>(header) + header->boneindex);
	const int count = std::min(header->numbones, MAXSTUDIOBONES);
	const int keptSpine = entity->curstate.iuser3;

	int cut[MAXSTUDIOBONES];
	int spineDepth[MAXSTUDIOBONES]{};

	for (int i = 0; i < count; ++i)
	{
		const int parent = bone[i].parent >= 0 && bone[i].parent < i ? bone[i].parent : -1;
		const bool spine = std::strstr(bone[i].name, "Spine") != nullptr;
		char nameBuffer[32];
		const char* name = JhlBoneName(bone[i].name, nameBuffer);
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

		for (int row = 0; row < 3; ++row)
		{
			for (int column = 0; column < 3; ++column)
				bones[i][row][column] = lightBones[i][row][column] = 0;

			bones[i][row][3] = bones[cut[i]][row][3];
			lightBones[i][row][3] = lightBones[cut[i]][row][3];
		}
	}
}

int JhlBody_HandsModel()
{
	return g_HandsIndex;
}

void JhlBody_Reset()
{
	g_Movement = {};
	g_Cycle = 0;
	g_AirTime = 0;
	g_StillTime = 0;
	g_ArmorKnown = false;
	g_AtEaseUndecided = true;
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
