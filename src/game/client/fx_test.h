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
 *	@brief Registers jhl_fx_test, which plays effects where the player is aiming for designing particle effects
 *	(see docs/johnhalflife/effects_test.md).
 */
void FxTest_Initialize();

/**
 *	@brief Plays the parts of a test effect that are due (delayed smoke, sequences). Call every frame.
 */
void FxTest_RunFrame();
