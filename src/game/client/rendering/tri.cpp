//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

// Triangle rendering, if any

#include "hud.h"

// Triangle rendering apis are in gEngfuncs.pTriAPI

#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "triangleapi.h"
#include "Exports.h"

#include "particleman.h"
#include "tri.h"
#include "view.h"
#include "jhl_blastring.h"
#include "jhl_softwarewater.h"
#include "jhl_softwarelook.h"

void RenderFog()
{
	if (g_WaterLevel <= WaterLevel::Feet && g_FogDensity >= 0)
	{
		gEngfuncs.pTriAPI->FogParams(g_FogDensity, g_FogSkybox);
		gEngfuncs.pTriAPI->Fog(g_FogColor, g_FogStartDistance, g_FogStopDistance, int(g_RenderFog));
	}
}

/**
 *	@brief Non-transparent triangles-- add them here
 */
void DLLEXPORT HUD_DrawNormalTriangles()
{
	// johnhalflife: the engine's fog and view, for the underwater fog; water as the software renderer drew it.
	JhlSoftwareLook_CaptureView();
	JhlSoftwareWater_DrawOpaque();

	gHUD.m_Spectator.DrawOverview();
}

/**
 *	@brief Render any triangles with transparent rendermode needs here
 */
void DLLEXPORT HUD_DrawTransparentTriangles()
{
	if (g_pParticleMan)
		g_pParticleMan->Update();

	// johnhalflife: blast rings.
	JhlBlastRing_Draw();

	// johnhalflife: see-through water as the software renderer drew it.
	JhlSoftwareWater_DrawTransparent();

	// Handled in V_CalcRefdef.
	// RenderFog();
}
