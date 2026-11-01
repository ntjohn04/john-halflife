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

#include <cstdint>

#include "hud.h"
#include "jhl_gl.h"
#include "jhl_softwarelook.h"

namespace
{
cvar_t* g_16BitColor = nullptr;
cvar_t* g_ModelOverbright = nullptr;

JhlGL& gl = g_JhlGL;

enum class State
{
	NotTried,
	Ready,
	Failed
};

State g_State = State::NotTried;

// The frame copied: a texture the size of the view, in a framebuffer to blit into (blitting resolves a multisampled
// frame, which can't be copied directly).
GLuint g_Texture = 0;
GLuint g_Framebuffer = 0;
GLint g_TextureSize[2]{};

GLuint g_Program = 0;
GLint g_OriginUniform = -1;

constexpr const char* VertexShader = R"(#version 120
void main()
{
	gl_Position = gl_Vertex;
}
)";

// Each colour cut down to the software renderer's 16 bits: red and blue to multiples of 8, green of 4 (565).
constexpr const char* FragmentShader = R"(#version 130
uniform sampler2D frame;
uniform ivec2 origin;
void main()
{
	vec3 colour = texelFetch(frame, ivec2(gl_FragCoord.xy) - origin, 0).rgb;
	vec3 levels = floor(colour * 255.0 + 0.001);
	levels = levels - mod(levels, vec3(8.0, 4.0, 8.0));
	gl_FragColor = vec4(levels / 255.0, 1.0);
}
)";

GLuint CompileShader(GLenum type, const char* source)
{
	const GLuint shader = gl.CreateShader(type);
	gl.ShaderSource(shader, 1, &source, nullptr);
	gl.CompileShader(shader);

	GLint compiled = GL_FALSE;
	gl.GetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

	if (compiled != GL_TRUE)
	{
		char log[1024]{};
		gl.GetShaderInfoLog(shader, sizeof(log) - 1, nullptr, log);
		gEngfuncs.Con_Printf("johnhalflife: software look shader failed to compile:\n%s\n", log);
		gl.DeleteShader(shader);
		return 0;
	}

	return shader;
}

bool CreateProgram()
{
	const GLuint vertex = CompileShader(GL_VERTEX_SHADER, VertexShader);
	const GLuint fragment = CompileShader(GL_FRAGMENT_SHADER, FragmentShader);

	if (0 == vertex || 0 == fragment)
	{
		return false;
	}

	g_Program = gl.CreateProgram();
	gl.AttachShader(g_Program, vertex);
	gl.AttachShader(g_Program, fragment);
	gl.LinkProgram(g_Program);
	gl.DeleteShader(vertex);
	gl.DeleteShader(fragment);

	GLint linked = GL_FALSE;
	gl.GetProgramiv(g_Program, GL_LINK_STATUS, &linked);

	if (linked != GL_TRUE)
	{
		char log[1024]{};
		gl.GetProgramInfoLog(g_Program, sizeof(log) - 1, nullptr, log);
		gEngfuncs.Con_Printf("johnhalflife: software look shader failed to link:\n%s\n", log);
		return false;
	}

	g_OriginUniform = gl.GetUniformLocation(g_Program, "origin");

	GLint previous = 0;
	gl.GetIntegerv(GL_CURRENT_PROGRAM, &previous);
	gl.UseProgram(g_Program);
	gl.Uniform1i(gl.GetUniformLocation(g_Program, "frame"), 0);
	gl.UseProgram(previous);
	return true;
}

// Only on the OpenGL renderer (the software one draws in 16 bits itself), and only once it's known to work.
bool Ready()
{
	if (g_State == State::NotTried)
	{
		g_State = State::Failed;

		if (!JhlGL_Available() || !CreateProgram())
		{
			return false;
		}

		gl.GenTextures(1, &g_Texture);
		gl.GenFramebuffers(1, &g_Framebuffer);
		g_State = State::Ready;
	}

	return g_State == State::Ready;
}

// The copy's texture sized to the view, attached to its framebuffer.
bool SizeCopy(GLint width, GLint height)
{
	if (g_TextureSize[0] == width && g_TextureSize[1] == height)
	{
		return true;
	}

	GLint previousTexture = 0;
	gl.GetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
	gl.BindTexture(GL_TEXTURE_2D, g_Texture);
	gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	gl.BindTexture(GL_TEXTURE_2D, previousTexture);

	GLint previousDraw = 0;
	gl.GetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previousDraw);
	gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, g_Framebuffer);
	gl.FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_Texture, 0);
	const bool complete = gl.CheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
	gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, previousDraw);

	if (!complete)
	{
		gEngfuncs.Con_Printf("johnhalflife: software look couldn't make its copy of the frame\n");
		g_State = State::Failed;
		return false;
	}

	g_TextureSize[0] = width;
	g_TextureSize[1] = height;
	return true;
}
}

// Models' light: the second texture unit, set to multiply what the first gives by a constant 2/3 and scale it by 2 (a
// combiner can only scale by 1, 2 or 4), sampling a white texel. What it was is put back after.
constexpr float ModelLightScale = 4.0f / 3.0f;

GLuint g_White = 0;

struct UnitState
{
	GLint ActiveTexture = GL_TEXTURE0;
	GLboolean Enabled = GL_FALSE;
	GLint Texture = 0;
	GLint Mode = GL_MODULATE;
	GLint CombineRgb = GL_MODULATE;
	GLint Source0 = GL_TEXTURE;
	GLint Source1 = GL_PREVIOUS;
	GLint CombineAlpha = GL_MODULATE;
	GLint SourceAlpha = GL_TEXTURE;
	GLfloat Scale = 1;
	GLfloat Color[4]{};
};

UnitState g_SavedUnit;

void JhlSoftwareLook_Initialize()
{
	g_16BitColor = gEngfuncs.pfnRegisterVariable("jhl_visuals_16bitcolor", "0", FCVAR_ARCHIVE);
	g_ModelOverbright = gEngfuncs.pfnRegisterVariable("jhl_visuals_modeloverbright", "1", FCVAR_ARCHIVE);
}

bool JhlSoftwareLook_BeginModel(int renderMode, int renderFx)
{
	// Lit models only: additive ones and glow shells aren't lit.
	if (0 == g_ModelOverbright->value || renderMode == kRenderTransAdd || renderMode == kRenderGlow ||
		renderFx == kRenderFxGlowShell || !JhlGL_Available())
	{
		return false;
	}

	if (0 == g_White)
	{
		constexpr std::uint32_t white = 0xFFFFFFFF;
		GLint previous = 0;
		gl.GetIntegerv(GL_TEXTURE_BINDING_2D, &previous);
		gl.GenTextures(1, &g_White);
		gl.BindTexture(GL_TEXTURE_2D, g_White);
		gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &white);
		gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		gl.BindTexture(GL_TEXTURE_2D, previous);
	}

	auto& saved = g_SavedUnit;
	gl.GetIntegerv(GL_ACTIVE_TEXTURE, &saved.ActiveTexture);
	gl.ActiveTexture(GL_TEXTURE1);
	saved.Enabled = gl.IsEnabled(GL_TEXTURE_2D);
	gl.GetIntegerv(GL_TEXTURE_BINDING_2D, &saved.Texture);
	gl.GetTexEnviv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &saved.Mode);
	gl.GetTexEnviv(GL_TEXTURE_ENV, GL_COMBINE_RGB, &saved.CombineRgb);
	gl.GetTexEnviv(GL_TEXTURE_ENV, GL_SOURCE0_RGB, &saved.Source0);
	gl.GetTexEnviv(GL_TEXTURE_ENV, GL_SOURCE1_RGB, &saved.Source1);
	gl.GetTexEnviv(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, &saved.CombineAlpha);
	gl.GetTexEnviv(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, &saved.SourceAlpha);
	gl.GetTexEnvfv(GL_TEXTURE_ENV, GL_RGB_SCALE, &saved.Scale);
	gl.GetTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, saved.Color);

	const GLfloat half[4] = {ModelLightScale / 2, ModelLightScale / 2, ModelLightScale / 2, 1};
	gl.Enable(GL_TEXTURE_2D);
	gl.BindTexture(GL_TEXTURE_2D, g_White);
	gl.TexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
	gl.TexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
	gl.TexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PREVIOUS);
	gl.TexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
	gl.TexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_CONSTANT);
	gl.TexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
	gl.TexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
	gl.TexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PREVIOUS);
	gl.TexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
	gl.TexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, half);
	gl.TexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE, 2);

	// The engine draws on the first unit, as it was.
	gl.ActiveTexture(saved.ActiveTexture);
	return true;
}

void JhlSoftwareLook_EndModel()
{
	const auto& saved = g_SavedUnit;
	GLint active = GL_TEXTURE0;
	gl.GetIntegerv(GL_ACTIVE_TEXTURE, &active);
	gl.ActiveTexture(GL_TEXTURE1);
	gl.TexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE, saved.Scale);
	gl.TexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, saved.Color);
	gl.TexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, saved.CombineRgb);
	gl.TexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, saved.Source0);
	gl.TexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, saved.Source1);
	gl.TexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, saved.CombineAlpha);
	gl.TexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, saved.SourceAlpha);
	gl.TexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, saved.Mode);
	gl.BindTexture(GL_TEXTURE_2D, saved.Texture);

	if (GL_TRUE != saved.Enabled)
	{
		gl.Disable(GL_TEXTURE_2D);
	}

	gl.ActiveTexture(active);
}

void JhlSoftwareLook_FrameDrawn()
{
	if (0 == g_16BitColor->value || !Ready())
	{
		return;
	}

	GLint viewport[4]{};
	gl.GetIntegerv(GL_VIEWPORT, viewport);

	if (viewport[2] <= 0 || viewport[3] <= 0 || !SizeCopy(viewport[2], viewport[3]))
	{
		return;
	}

	// Everything this changes, put back as it was.
	GLint readFramebuffer = 0, drawFramebuffer = 0, program = 0, activeTexture = 0, texture = 0;
	gl.GetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer);
	gl.GetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFramebuffer);
	gl.GetIntegerv(GL_CURRENT_PROGRAM, &program);
	gl.GetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);

	// Copy the frame (resolving it, if multisampled).
	while (gl.GetError() != GL_NO_ERROR)
	{
	}

	gl.BindFramebuffer(GL_READ_FRAMEBUFFER, drawFramebuffer);
	gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, g_Framebuffer);
	gl.BlitFramebuffer(viewport[0], viewport[1], viewport[0] + viewport[2], viewport[1] + viewport[3],
		0, 0, viewport[2], viewport[3], GL_COLOR_BUFFER_BIT, GL_NEAREST);
	gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFramebuffer);
	gl.BindFramebuffer(GL_READ_FRAMEBUFFER, readFramebuffer);

	if (const GLenum error = gl.GetError(); error != GL_NO_ERROR)
	{
		gEngfuncs.Con_Printf("johnhalflife: software look couldn't copy the frame (OpenGL error %#x); off until restart\n", error);
		g_State = State::Failed;
		return;
	}

	// Draw it back, cut down, over the view.
	gl.PushAttrib(GL_ALL_ATTRIB_BITS);
	gl.Disable(GL_DEPTH_TEST);
	gl.Disable(GL_BLEND);
	gl.Disable(GL_ALPHA_TEST);
	gl.Disable(GL_CULL_FACE);
	gl.Disable(GL_SCISSOR_TEST);
	gl.Disable(GL_FOG);
	gl.DepthMask(GL_FALSE);
	gl.ColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	gl.MatrixMode(GL_PROJECTION);
	gl.PushMatrix();
	gl.LoadIdentity();
	gl.MatrixMode(GL_MODELVIEW);
	gl.PushMatrix();
	gl.LoadIdentity();

	gl.ActiveTexture(GL_TEXTURE0);
	gl.GetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
	gl.BindTexture(GL_TEXTURE_2D, g_Texture);

	gl.UseProgram(g_Program);
	gl.Uniform2i(g_OriginUniform, viewport[0], viewport[1]);

	gl.Begin(GL_QUADS);
	gl.Vertex2f(-1, -1);
	gl.Vertex2f(1, -1);
	gl.Vertex2f(1, 1);
	gl.Vertex2f(-1, 1);
	gl.End();

	gl.UseProgram(program);
	gl.BindTexture(GL_TEXTURE_2D, texture);
	gl.ActiveTexture(activeTexture);

	gl.MatrixMode(GL_PROJECTION);
	gl.PopMatrix();
	gl.MatrixMode(GL_MODELVIEW);
	gl.PopMatrix();
	gl.PopAttrib();
}
