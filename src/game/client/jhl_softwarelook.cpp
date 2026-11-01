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
#include <cstdint>

#include "hud.h"
#include "view.h"
#include "jhl_gl.h"
#include "jhl_softwarewater.h"
#include "jhl_softwarelook.h"

namespace
{
cvar_t* g_16BitColor = nullptr;
cvar_t* g_ModelOverbright = nullptr;
cvar_t* g_UnderwaterView = nullptr;
cvar_t* g_UnderwaterFog = nullptr;

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

// A way of drawing the copied frame back over the view: its program and where its uniforms are.
struct Pass
{
	const char* FragmentShader;
	GLuint Program = 0;
	GLint Origin = -1;
	GLint Scale = -1;
	GLint Projection = -1;
	GLint SoftwareFog = -1;
};

constexpr const char* VertexShader = R"(#version 120
void main()
{
	gl_Position = gl_Vertex;
}
)";

// Each colour cut down to the software renderer's 16 bits: red and blue to multiples of 8, green of 4 (565).
Pass g_16BitPass{R"(#version 130
uniform sampler2D frame;
uniform ivec2 origin;
void main()
{
	vec3 colour = texelFetch(frame, ivec2(gl_FragCoord.xy) - origin, 0).rgb;
	vec3 levels = floor(colour * 255.0 + 0.001);
	levels = levels - mod(levels, vec3(8.0, 4.0, 8.0));
	gl_FragColor = vec4(levels / 255.0, 1.0);
}
)"};

// The view under water as the software renderer drew it: at a lower resolution, stretched back (scale: the frame's
// pixels a block; each block takes its middle pixel), and fogged its way (softwareFog.w > 0: colour 0-255 and density;
// the engine's own fog kept off, see JhlSoftwareLook_SetupView). Depth gives each pixel's distance: projection is the 3D
// view's matrix [10] and [14] and its depth range.
Pass g_UnderwaterPass{R"(#version 130
uniform sampler2D frame;
uniform sampler2D depth;
uniform ivec2 origin;
uniform vec2 scale;
uniform vec4 projection;
uniform vec4 softwareFog;
void main()
{
	vec2 block = floor((gl_FragCoord.xy - vec2(origin)) / scale);
	ivec2 at = ivec2((block + 0.5) * scale);
	vec3 colour = texelFetch(frame, at, 0).rgb * 255.0;

	if (softwareFog.w > 0.0)
	{
		float ndc = 2.0 * (texelFetch(depth, at, 0).r - projection.z) / (projection.w - projection.z) - 1.0;
		float distance = max(projection.y / (projection.x + ndc), 1.0);

		// sw.dll's: 64 blends by 32768 / distance (63 from 520 units in), ((64 - that) * far >> 6) + near of 256,
		// at least 32, near = density / 2, far = (512 - near) * density >> 8.
		float near = floor(softwareFog.w * 128.0 / 256.0);
		float far = floor((512.0 - near) * softwareFog.w / 256.0);
		float table = min(63.0, floor(32768.0 / distance));
		float amount = floor((64.0 - table) * far / 64.0) + near;
		amount = amount >= 256.0 ? 255.0 : max(amount, 32.0);
		colour = colour + floor(amount * (softwareFog.rgb - colour) / 256.0);
	}

	gl_FragColor = vec4(colour / 255.0, 1.0);
}
)"};

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

bool CreateProgram(Pass& pass)
{
	const GLuint vertex = CompileShader(GL_VERTEX_SHADER, VertexShader);
	const GLuint fragment = CompileShader(GL_FRAGMENT_SHADER, pass.FragmentShader);

	if (0 == vertex || 0 == fragment)
	{
		return false;
	}

	pass.Program = gl.CreateProgram();
	gl.AttachShader(pass.Program, vertex);
	gl.AttachShader(pass.Program, fragment);
	gl.LinkProgram(pass.Program);
	gl.DeleteShader(vertex);
	gl.DeleteShader(fragment);

	GLint linked = GL_FALSE;
	gl.GetProgramiv(pass.Program, GL_LINK_STATUS, &linked);

	if (linked != GL_TRUE)
	{
		char log[1024]{};
		gl.GetProgramInfoLog(pass.Program, sizeof(log) - 1, nullptr, log);
		gEngfuncs.Con_Printf("johnhalflife: software look shader failed to link:\n%s\n", log);
		return false;
	}

	pass.Origin = gl.GetUniformLocation(pass.Program, "origin");
	pass.Scale = gl.GetUniformLocation(pass.Program, "scale");
	pass.Projection = gl.GetUniformLocation(pass.Program, "projection");
	pass.SoftwareFog = gl.GetUniformLocation(pass.Program, "softwareFog");

	GLint previous = 0;
	gl.GetIntegerv(GL_CURRENT_PROGRAM, &previous);
	gl.UseProgram(pass.Program);
	gl.Uniform1i(gl.GetUniformLocation(pass.Program, "frame"), 0);

	if (const GLint depth = gl.GetUniformLocation(pass.Program, "depth"); depth >= 0)
	{
		gl.Uniform1i(depth, 1);
	}
	gl.UseProgram(previous);
	return true;
}

// Only on the OpenGL renderer (the software one draws in 16 bits itself), and only once it's known to work.
bool Ready()
{
	if (g_State == State::NotTried)
	{
		g_State = State::Failed;

		if (!JhlGL_Available() || !CreateProgram(g_16BitPass) || !CreateProgram(g_UnderwaterPass))
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

// Software's warp buffer (sw.dll sets it at every mode change).
constexpr float WarpWidth = 320;
constexpr float WarpHeight = 200;

// The underwater fog's values for its pass (see g_UnderwaterPass).
struct UnderwaterFog
{
	float Projection[4]{};
	float SoftwareFog[4]{};
};

// The 3D view as the engine draws it, taken while it's drawing the world (HUD_DrawNormalTriangles).
struct CapturedView
{
	float Time = -1;
	float EngineFogColour[3]{};
	float EngineFogEnd = 0;
	float Projection[4]{};
};

CapturedView g_CapturedView;

// The view's depth, copied.
GLuint g_DepthTexture = 0;
GLuint g_DepthFramebuffer = 0;
GLint g_DepthSize[2]{};
GLenum g_DepthFormat = 0;
bool g_FogFailed = false;

// The engine's fog kept off for this frame (see JhlSoftwareLook_SetupView).
bool g_EngineFogOff = false;

void JhlSoftwareLook_Initialize()
{
	g_16BitColor = gEngfuncs.pfnRegisterVariable("jhl_visuals_16bitcolor", "0", FCVAR_ARCHIVE);
	g_ModelOverbright = gEngfuncs.pfnRegisterVariable("jhl_visuals_modeloverbright", "1", FCVAR_ARCHIVE);
	g_UnderwaterView = gEngfuncs.pfnRegisterVariable("jhl_visuals_underwaterview", "0", FCVAR_ARCHIVE);
	g_UnderwaterFog = gEngfuncs.pfnRegisterVariable("jhl_visuals_underwaterfog", "0", FCVAR_ARCHIVE);
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

// Copies the view (the viewport) and draws it back over itself through a pass.
static void RedrawView(const Pass& pass, const GLint (&viewport)[4], float scaleX, float scaleY, const UnderwaterFog* fog = nullptr)
{
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

	// Draw it back, through the pass, over the view.
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

	gl.UseProgram(pass.Program);
	gl.Uniform2i(pass.Origin, viewport[0], viewport[1]);

	if (pass.Scale >= 0)
	{
		gl.Uniform2f(pass.Scale, scaleX, scaleY);
	}

	// The fog's values, and the view's depth on the second unit; none, without fog.
	const UnderwaterFog none;
	const UnderwaterFog& values = fog ? *fog : none;
	GLint depthUnitTexture = 0;

	if (pass.SoftwareFog >= 0)
	{
		gl.Uniform4f(pass.Projection, values.Projection[0], values.Projection[1], values.Projection[2], values.Projection[3]);
		gl.Uniform4f(pass.SoftwareFog, values.SoftwareFog[0], values.SoftwareFog[1], values.SoftwareFog[2], values.SoftwareFog[3]);
	}

	if (fog)
	{
		gl.ActiveTexture(GL_TEXTURE1);
		gl.GetIntegerv(GL_TEXTURE_BINDING_2D, &depthUnitTexture);
		gl.BindTexture(GL_TEXTURE_2D, g_DepthTexture);
		gl.ActiveTexture(GL_TEXTURE0);
	}

	gl.Begin(GL_QUADS);
	gl.Vertex2f(-1, -1);
	gl.Vertex2f(1, -1);
	gl.Vertex2f(1, 1);
	gl.Vertex2f(-1, 1);
	gl.End();

	gl.UseProgram(program);
	gl.BindTexture(GL_TEXTURE_2D, texture);

	if (fog)
	{
		gl.ActiveTexture(GL_TEXTURE1);
		gl.BindTexture(GL_TEXTURE_2D, depthUnitTexture);
	}

	gl.ActiveTexture(activeTexture);

	gl.MatrixMode(GL_PROJECTION);
	gl.PopMatrix();
	gl.MatrixMode(GL_MODELVIEW);
	gl.PopMatrix();
	gl.PopAttrib();
}

// The view's depth, copied for the underwater fog: a depth texture in the format of the frame's depth buffer (a blit
// needs them the same).
static bool CopyDepth(const GLint (&viewport)[4], GLint drawFramebuffer)
{
	GLint depthBits = 0, stencilBits = 0, type = GL_UNSIGNED_NORMALIZED;
	const GLenum attachment = 0 == drawFramebuffer ? GL_DEPTH : GL_DEPTH_ATTACHMENT;
	gl.BindFramebuffer(GL_READ_FRAMEBUFFER, drawFramebuffer);
	gl.GetFramebufferAttachmentParameteriv(GL_READ_FRAMEBUFFER, attachment, GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE, &depthBits);
	gl.GetFramebufferAttachmentParameteriv(GL_READ_FRAMEBUFFER, 0 == drawFramebuffer ? GL_STENCIL : GL_STENCIL_ATTACHMENT,
		GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE, &stencilBits);
	gl.GetFramebufferAttachmentParameteriv(GL_READ_FRAMEBUFFER, attachment, GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE, &type);
	gl.GetError(); // A combined depth and stencil buffer can't answer for its stencil alone everywhere.

	GLenum format = GL_DEPTH_COMPONENT24;

	if (stencilBits > 0)
	{
		format = type == GL_FLOAT ? GL_DEPTH32F_STENCIL8 : GL_DEPTH24_STENCIL8;
	}
	else if (type == GL_FLOAT)
	{
		format = GL_DEPTH_COMPONENT32F;
	}
	else if (depthBits == 16)
	{
		format = GL_DEPTH_COMPONENT16;
	}
	else if (depthBits == 32)
	{
		format = GL_DEPTH_COMPONENT32;
	}

	if (0 == g_DepthTexture)
	{
		gl.GenTextures(1, &g_DepthTexture);
		gl.GenFramebuffers(1, &g_DepthFramebuffer);
	}

	if (g_DepthSize[0] != viewport[2] || g_DepthSize[1] != viewport[3] || g_DepthFormat != format)
	{
		const bool withStencil = format == GL_DEPTH24_STENCIL8 || format == GL_DEPTH32F_STENCIL8;
		GLint previous = 0;
		gl.GetIntegerv(GL_TEXTURE_BINDING_2D, &previous);
		gl.BindTexture(GL_TEXTURE_2D, g_DepthTexture);
		gl.TexImage2D(GL_TEXTURE_2D, 0, format, viewport[2], viewport[3], 0, withStencil ? GL_DEPTH_STENCIL : GL_DEPTH_COMPONENT,
			withStencil ? (format == GL_DEPTH32F_STENCIL8 ? GL_FLOAT_32_UNSIGNED_INT_24_8_REV : GL_UNSIGNED_INT_24_8) : GL_FLOAT, nullptr);
		gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
		gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
		gl.BindTexture(GL_TEXTURE_2D, previous);

		gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, g_DepthFramebuffer);
		gl.FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, withStencil ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, g_DepthTexture, 0);
		const bool complete = gl.CheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
		gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFramebuffer);

		if (!complete)
		{
			return false;
		}

		g_DepthSize[0] = viewport[2];
		g_DepthSize[1] = viewport[3];
		g_DepthFormat = format;
	}

	while (gl.GetError() != GL_NO_ERROR)
	{
	}

	gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, g_DepthFramebuffer);
	gl.BlitFramebuffer(viewport[0], viewport[1], viewport[0] + viewport[2], viewport[1] + viewport[3],
		0, 0, viewport[2], viewport[3], GL_DEPTH_BUFFER_BIT, GL_NEAREST);
	gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFramebuffer);
	return gl.GetError() == GL_NO_ERROR;
}

void JhlSoftwareLook_CaptureView()
{
	if ((0 == g_UnderwaterFog->value) || !Ready())
	{
		return;
	}

	// The engine's fog is what it drew the world with (under water: linear, its colour and end, set every frame; it
	// turns fog off around this callback, so whether it's on can't be asked here).
	auto& view = g_CapturedView;
	view.Time = gEngfuncs.GetClientTime();
	view.EngineFogEnd = 0;

	GLint mode = 0;
	gl.GetIntegerv(GL_FOG_MODE, &mode);

	if (mode == GL_LINEAR)
	{
		GLfloat colour[4]{};
		gl.GetFloatv(GL_FOG_COLOR, colour);
		gl.GetFloatv(GL_FOG_END, &view.EngineFogEnd);

		for (int i = 0; i < 3; ++i)
		{
			view.EngineFogColour[i] = colour[i] * 255;
		}
	}

	GLfloat projection[16]{};
	GLfloat range[2]{0, 1};
	gl.GetFloatv(GL_PROJECTION_MATRIX, projection);
	gl.GetFloatv(GL_DEPTH_RANGE, range);
	view.Projection[0] = projection[10];
	view.Projection[1] = projection[14];
	view.Projection[2] = range[0];
	view.Projection[3] = range[1];
}

void JhlSoftwareLook_SetupView()
{
	// The engine fogs the view under water itself (linear, from the eye, every frame it's under water). For software's
	// fog instead, OpenGL is told to fog by a fog coordinate of 0 (nothing drawn uses another): linear fog starting at 0
	// then covers nothing, and the engine's set up stays as it is. Put back as soon as it's not wanted.
	bool off = false;

	if (0 != g_UnderwaterFog->value && !g_FogFailed && JhlGL_Available())
	{
		const int contents = gEngfuncs.PM_PointContents(v_origin, nullptr);
		off = contents <= CONTENTS_WATER && contents != CONTENTS_TRANSLUCENT;
	}

	if (off != g_EngineFogOff)
	{
		gl.Fogi(GL_FOG_COORD_SRC, off ? GL_FOG_COORD : GL_FRAGMENT_DEPTH);

		if (off)
		{
			gl.FogCoordf(0);
		}

		g_EngineFogOff = off;
	}
}

void JhlSoftwareLook_ViewDrawn()
{
	// Software's view under water (sw.dll's R_SetupFrame): in water, slime or lava, drawn into its warp buffer, 320 x
	// 200 (Quake's, never raised): narrowed to 320 wide keeping its shape, then no more than 200 tall (its width kept,
	// so its pixels aren't square), and stretched back over the view; fogged as it's stretched (see g_UnderwaterPass).
	// The HUD is drawn after, as sharp as ever.
	const bool lowResolution = 0 != g_UnderwaterView->value;
	bool fog = 0 != g_UnderwaterFog->value && !g_FogFailed;

	if ((!lowResolution && !fog) || !Ready())
	{
		return;
	}

	const int contents = gEngfuncs.PM_PointContents(v_origin, nullptr);

	if (contents > CONTENTS_WATER || contents == CONTENTS_TRANSLUCENT)
	{
		return;
	}

	GLint viewport[4]{};
	gl.GetIntegerv(GL_VIEWPORT, viewport);

	float scaleX = 1;
	float scaleY = 1;

	if (lowResolution)
	{
		float width = static_cast<float>(viewport[2]);
		float height = static_cast<float>(viewport[3]);

		if (width > WarpWidth)
		{
			height *= WarpWidth / width;
			width = WarpWidth;
		}

		height = std::min(height, WarpHeight);
		scaleX = viewport[2] / std::floor(width);
		scaleY = viewport[3] / std::floor(height);
	}

	// The fog: software's, from the water's palette as the engine would take it (the water drawn last: here if it's
	// drawn here, else the engine's own fog's colour and where it ends, 1536 - 4 x density), the engine's kept off.
	UnderwaterFog underwater;
	const auto& view = g_CapturedView;
	fog = fog && g_EngineFogOff && view.Time == gEngfuncs.GetClientTime();

	if (fog)
	{
		std::copy(std::begin(view.Projection), std::end(view.Projection), underwater.Projection);

		if (int water[4]{}; JhlSoftwareWater_LastFog(water))
		{
			std::copy(std::begin(water), std::end(water), underwater.SoftwareFog);
		}
		else if (view.EngineFogEnd > 0)
		{
			std::copy(std::begin(view.EngineFogColour), std::end(view.EngineFogColour), underwater.SoftwareFog);
			underwater.SoftwareFog[3] = std::max(0.0f, (1536 - view.EngineFogEnd) / 4);
		}

		GLint drawFramebuffer = 0, readFramebuffer = 0;
		gl.GetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFramebuffer);
		gl.GetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer);
		const bool copied = CopyDepth(viewport, drawFramebuffer);
		gl.BindFramebuffer(GL_READ_FRAMEBUFFER, readFramebuffer);
		gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFramebuffer);

		if (!copied)
		{
			gEngfuncs.Con_Printf("johnhalflife: underwater fog couldn't copy the view's depth; off until restart\n");
			g_FogFailed = true;
			fog = false;
		}
	}

	if (!lowResolution && !fog)
	{
		return;
	}

	RedrawView(g_UnderwaterPass, viewport, scaleX, scaleY, fog ? &underwater : nullptr);
}

void JhlSoftwareLook_FrameDrawn()
{
	if (0 == g_16BitColor->value || !Ready())
	{
		return;
	}

	GLint viewport[4]{};
	gl.GetIntegerv(GL_VIEWPORT, viewport);
	RedrawView(g_16BitPass, viewport, 1, 1);
}
