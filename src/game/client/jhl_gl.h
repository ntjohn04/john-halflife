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

#include "PlatformHeaders.h"
#include <SDL2/SDL_opengl.h>

// johnhalflife: OpenGL calls in the engine's context, for drawing it doesn't offer (the client doesn't link OpenGL;
// they're looked up through SDL). Only with the OpenGL renderer.
struct JhlGL
{
	decltype(&::glGetIntegerv) GetIntegerv = nullptr;
	decltype(&::glGetFloatv) GetFloatv = nullptr;
	decltype(&::glGetError) GetError = nullptr;
	decltype(&::glIsTexture) IsTexture = nullptr;
	decltype(&::glGenTextures) GenTextures = nullptr;
	decltype(&::glDeleteTextures) DeleteTextures = nullptr;
	decltype(&::glBindTexture) BindTexture = nullptr;
	decltype(&::glTexImage2D) TexImage2D = nullptr;
	decltype(&::glTexSubImage2D) TexSubImage2D = nullptr;
	decltype(&::glGetTexImage) GetTexImage = nullptr;
	decltype(&::glGetTexLevelParameteriv) GetTexLevelParameteriv = nullptr;
	decltype(&::glGetTexParameteriv) GetTexParameteriv = nullptr;
	decltype(&::glTexParameteri) TexParameteri = nullptr;
	decltype(&::glPushAttrib) PushAttrib = nullptr;
	decltype(&::glPopAttrib) PopAttrib = nullptr;
	decltype(&::glMatrixMode) MatrixMode = nullptr;
	decltype(&::glPushMatrix) PushMatrix = nullptr;
	decltype(&::glPopMatrix) PopMatrix = nullptr;
	decltype(&::glLoadIdentity) LoadIdentity = nullptr;
	decltype(&::glTranslatef) Translatef = nullptr;
	decltype(&::glRotatef) Rotatef = nullptr;
	decltype(&::glEnable) Enable = nullptr;
	decltype(&::glDisable) Disable = nullptr;
	decltype(&::glDepthMask) DepthMask = nullptr;
	decltype(&::glDepthFunc) DepthFunc = nullptr;
	decltype(&::glColorMask) ColorMask = nullptr;
	decltype(&::glBlendFunc) BlendFunc = nullptr;
	decltype(&::glPolygonOffset) PolygonOffset = nullptr;
	decltype(&::glTexEnvi) TexEnvi = nullptr;
	decltype(&::glFogi) Fogi = nullptr;
	PFNGLFOGCOORDFPROC FogCoordf = nullptr;
	decltype(&::glTexEnvf) TexEnvf = nullptr;
	decltype(&::glTexEnvfv) TexEnvfv = nullptr;
	decltype(&::glGetTexEnviv) GetTexEnviv = nullptr;
	decltype(&::glGetTexEnvfv) GetTexEnvfv = nullptr;
	decltype(&::glIsEnabled) IsEnabled = nullptr;
	decltype(&::glColor4f) Color4f = nullptr;
	decltype(&::glBegin) Begin = nullptr;
	decltype(&::glEnd) End = nullptr;
	decltype(&::glVertex2f) Vertex2f = nullptr;
	decltype(&::glVertex3fv) Vertex3fv = nullptr;
	decltype(&::glTexCoord2f) TexCoord2f = nullptr;
	PFNGLACTIVETEXTUREPROC ActiveTexture = nullptr;
	PFNGLCREATESHADERPROC CreateShader = nullptr;
	PFNGLSHADERSOURCEPROC ShaderSource = nullptr;
	PFNGLCOMPILESHADERPROC CompileShader = nullptr;
	PFNGLGETSHADERIVPROC GetShaderiv = nullptr;
	PFNGLGETSHADERINFOLOGPROC GetShaderInfoLog = nullptr;
	PFNGLDELETESHADERPROC DeleteShader = nullptr;
	PFNGLCREATEPROGRAMPROC CreateProgram = nullptr;
	PFNGLATTACHSHADERPROC AttachShader = nullptr;
	PFNGLLINKPROGRAMPROC LinkProgram = nullptr;
	PFNGLGETPROGRAMIVPROC GetProgramiv = nullptr;
	PFNGLGETPROGRAMINFOLOGPROC GetProgramInfoLog = nullptr;
	PFNGLUSEPROGRAMPROC UseProgram = nullptr;
	PFNGLGETUNIFORMLOCATIONPROC GetUniformLocation = nullptr;
	PFNGLUNIFORM1IPROC Uniform1i = nullptr;
	PFNGLUNIFORM2IPROC Uniform2i = nullptr;
	PFNGLUNIFORM2FPROC Uniform2f = nullptr;
	PFNGLUNIFORM4FPROC Uniform4f = nullptr;
	PFNGLGENFRAMEBUFFERSPROC GenFramebuffers = nullptr;
	PFNGLBINDFRAMEBUFFERPROC BindFramebuffer = nullptr;
	PFNGLFRAMEBUFFERTEXTURE2DPROC FramebufferTexture2D = nullptr;
	PFNGLCHECKFRAMEBUFFERSTATUSPROC CheckFramebufferStatus = nullptr;
	PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC GetFramebufferAttachmentParameteriv = nullptr;
	PFNGLBLITFRAMEBUFFERPROC BlitFramebuffer = nullptr;
	PFNGLGENERATEMIPMAPPROC GenerateMipmap = nullptr;
};

extern JhlGL g_JhlGL;

/**
 *	@brief Whether the calls are there to use: the OpenGL renderer, and all of them found (looked up once).
 */
bool JhlGL_Available();
