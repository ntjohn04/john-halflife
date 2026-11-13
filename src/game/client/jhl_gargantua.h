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

class BufferReader;
struct cl_entity_t;

void JhlGargantua_ParseFlame(BufferReader& reader);

void JhlGargantua_Track(cl_entity_t* ent, const char* modelname);

void JhlVoltigore_Track(cl_entity_t* ent, const char* modelname);

void JhlGargantua_Drawn(cl_entity_t* ent);

void JhlPitWorm_Drawn(cl_entity_t* ent);

void JhlGeneWorm_Drawn(cl_entity_t* ent);

void JhlGonarchSpit_Track(cl_entity_t* ent, const char* modelname);

void JhlGargantua_ParseStomp(BufferReader& reader);

void JhlGargantua_Update(double time);

void JhlGargantua_Reset();
