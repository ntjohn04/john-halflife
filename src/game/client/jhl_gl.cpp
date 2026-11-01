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

#include "hud.h"
#include "com_model.h"
#include "r_studioint.h"
#include "jhl_gl.h"

#include <SDL2/SDL_video.h>

extern engine_studio_api_t IEngineStudio;

JhlGL g_JhlGL;

namespace
{
enum class State
{
	NotTried,
	Ready,
	Failed
};

State g_State = State::NotTried;

template <typename T>
bool Load(T& function, const char* name)
{
	function = reinterpret_cast<T>(SDL_GL_GetProcAddress(name));
	return function != nullptr;
}

bool LoadAll()
{
	auto& gl = g_JhlGL;
	bool ok = true;
	ok = Load(gl.GetIntegerv, "glGetIntegerv") && ok;
	ok = Load(gl.GetError, "glGetError") && ok;
	ok = Load(gl.IsTexture, "glIsTexture") && ok;
	ok = Load(gl.GenTextures, "glGenTextures") && ok;
	ok = Load(gl.DeleteTextures, "glDeleteTextures") && ok;
	ok = Load(gl.BindTexture, "glBindTexture") && ok;
	ok = Load(gl.TexImage2D, "glTexImage2D") && ok;
	ok = Load(gl.TexSubImage2D, "glTexSubImage2D") && ok;
	ok = Load(gl.GetTexImage, "glGetTexImage") && ok;
	ok = Load(gl.GetTexLevelParameteriv, "glGetTexLevelParameteriv") && ok;
	ok = Load(gl.GetTexParameteriv, "glGetTexParameteriv") && ok;
	ok = Load(gl.TexParameteri, "glTexParameteri") && ok;
	ok = Load(gl.PushAttrib, "glPushAttrib") && ok;
	ok = Load(gl.PopAttrib, "glPopAttrib") && ok;
	ok = Load(gl.MatrixMode, "glMatrixMode") && ok;
	ok = Load(gl.PushMatrix, "glPushMatrix") && ok;
	ok = Load(gl.PopMatrix, "glPopMatrix") && ok;
	ok = Load(gl.LoadIdentity, "glLoadIdentity") && ok;
	ok = Load(gl.Translatef, "glTranslatef") && ok;
	ok = Load(gl.Rotatef, "glRotatef") && ok;
	ok = Load(gl.Enable, "glEnable") && ok;
	ok = Load(gl.Disable, "glDisable") && ok;
	ok = Load(gl.DepthMask, "glDepthMask") && ok;
	ok = Load(gl.DepthFunc, "glDepthFunc") && ok;
	ok = Load(gl.ColorMask, "glColorMask") && ok;
	ok = Load(gl.BlendFunc, "glBlendFunc") && ok;
	ok = Load(gl.PolygonOffset, "glPolygonOffset") && ok;
	ok = Load(gl.TexEnvi, "glTexEnvi") && ok;
	ok = Load(gl.Color4f, "glColor4f") && ok;
	ok = Load(gl.Begin, "glBegin") && ok;
	ok = Load(gl.End, "glEnd") && ok;
	ok = Load(gl.Vertex2f, "glVertex2f") && ok;
	ok = Load(gl.Vertex3fv, "glVertex3fv") && ok;
	ok = Load(gl.TexCoord2f, "glTexCoord2f") && ok;
	ok = Load(gl.ActiveTexture, "glActiveTexture") && ok;
	ok = Load(gl.CreateShader, "glCreateShader") && ok;
	ok = Load(gl.ShaderSource, "glShaderSource") && ok;
	ok = Load(gl.CompileShader, "glCompileShader") && ok;
	ok = Load(gl.GetShaderiv, "glGetShaderiv") && ok;
	ok = Load(gl.GetShaderInfoLog, "glGetShaderInfoLog") && ok;
	ok = Load(gl.DeleteShader, "glDeleteShader") && ok;
	ok = Load(gl.CreateProgram, "glCreateProgram") && ok;
	ok = Load(gl.AttachShader, "glAttachShader") && ok;
	ok = Load(gl.LinkProgram, "glLinkProgram") && ok;
	ok = Load(gl.GetProgramiv, "glGetProgramiv") && ok;
	ok = Load(gl.GetProgramInfoLog, "glGetProgramInfoLog") && ok;
	ok = Load(gl.UseProgram, "glUseProgram") && ok;
	ok = Load(gl.GetUniformLocation, "glGetUniformLocation") && ok;
	ok = Load(gl.Uniform1i, "glUniform1i") && ok;
	ok = Load(gl.Uniform2i, "glUniform2i") && ok;
	ok = Load(gl.GenFramebuffers, "glGenFramebuffers") && ok;
	ok = Load(gl.BindFramebuffer, "glBindFramebuffer") && ok;
	ok = Load(gl.FramebufferTexture2D, "glFramebufferTexture2D") && ok;
	ok = Load(gl.CheckFramebufferStatus, "glCheckFramebufferStatus") && ok;
	ok = Load(gl.BlitFramebuffer, "glBlitFramebuffer") && ok;
	ok = Load(gl.GenerateMipmap, "glGenerateMipmap") && ok;
	return ok;
}
}

bool JhlGL_Available()
{
	if (g_State == State::NotTried)
	{
		g_State = 1 == IEngineStudio.IsHardware() && LoadAll() ? State::Ready : State::Failed;

		if (g_State == State::Failed)
		{
			gEngfuncs.Con_DPrintf("johnhalflife: OpenGL drawing unavailable (needs the OpenGL renderer)\n");
		}
	}

	return g_State == State::Ready;
}
