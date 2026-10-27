//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#pragma once

// TODO: could just make a copy of ref_params_t when it gets passed in instead.
inline bool g_Paused = false;
inline WaterLevel g_WaterLevel = WaterLevel::Dry;
inline int g_ViewEntity = 0;

inline Vector v_origin;
inline Vector v_angles;
inline Vector v_cl_angles;
inline Vector v_sim_org;
inline Vector v_sim_vel; // johnhalflife: the local player's predicted velocity, for what's thrown off their weapon.

/**
 *	@brief johnhalflife: a point on the view model where it has no attachment: @p local in its bone @p bone's space,
 *	in the world: where it was on the view model as last drawn (StudioModelRenderer), where the view model is now. False
 *	if it isn't @p modelName or wasn't just drawn (third person, zoomed).
 */
bool JhlViewModelPoint(const char* modelName, int bone, const Vector& local, Vector& world);
inline Vector v_client_aimangles;
inline Vector v_crosshairangle;

void V_StartPitchDrift();

/**
 *	@brief A smooth view kick from a weapon action (melee swing, throw), skipped with jhl_weapons_smoothrecoil 0.
 */
void V_WeaponKick(const Vector& kick, float spring, float damping);

/**
 *	@brief Starts or stops the gluon gun's sustained view drift for the local player.
 */
void V_SetEgonFiring(bool firing);
void V_StopPitchDrift();

void V_GetInEyePos(int entity, Vector& origin, Vector& angles);
void V_ResetChaseCam();
void V_GetChasePos(int target, Vector* cl_angles, Vector& origin, Vector& angles);
