/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The overlays' side of the renderer. This is the only file in the shell that includes
// bgfx, and every renderer handle the shell owns lives here, the way bgfxbackend.cpp holds
// the frame's handles. It carries RmlUi's render interface and the ImGui renderer, which
// share the program, the views and the blend state.
//
// docs/UI_DESIGN.md, "Rendering", owns the contract.

#include "uiinternal.h"

#include "backendviews.hh"
#include "dbgprint.h"

#include <RmlUi/Core/RenderInterface.h>

#include <imgui.h>

#include <bgfx/bgfx.h>
#include <bgfx/embedded_shader.h>

#include <vs_ocornut_imgui.bin.h>
#include <fs_ocornut_imgui.bin.h>

#include <algorithm>
#include <cstring>


static const bgfx::EmbeddedShader _EmbeddedShaders[] = {
	BGFX_EMBEDDED_SHADER(vs_ocornut_imgui),
	BGFX_EMBEDDED_SHADER(fs_ocornut_imgui),
	BGFX_EMBEDDED_SHADER_END()
};


static bool _Initialized = false;

static bgfx::ProgramHandle _Program = BGFX_INVALID_HANDLE;
static bgfx::UniformHandle _TextureSampler = BGFX_INVALID_HANDLE;
static bgfx::TextureHandle _WhiteTexture = BGFX_INVALID_HANDLE;
static bgfx::VertexLayout _RmlLayout;
static bgfx::VertexLayout _ImGuiLayout;

// Where the frame landed in the window, which is also the overlays' viewport. Scissor
// rectangles arrive relative to this origin and are made absolute before they are set.
static int _OriginX = 0;
static int _OriginY = 0;
static int _Width = 0;
static int _Height = 0;

static const uint64_t _BlendState =
	BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_MSAA
	| BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_INV_SRC_ALPHA);


// One compiled geometry. RmlUi 6 compiles geometry once and re-submits it, so these are
// static buffers rather than transient ones, which must not outlive the frame they were
// filled in.
struct UIGeometry
{
	bgfx::VertexBufferHandle Vertices = BGFX_INVALID_HANDLE;
	bgfx::IndexBufferHandle Indices = BGFX_INVALID_HANDLE;
	uint32_t IndexCount = 0;
};


/// <summary>
/// Builds an orthographic projection over a target measured in pixels, origin top left.
/// </summary>
static void Build_Ortho_Projection(float * result, int width, int height)
{
	const float depthnear = 0.0f;
	const float depthfar = 1000.0f;
	const bool homogeneous = bgfx::getCaps()->homogeneousDepth;

	std::memset(result, 0, sizeof(float) * 16);

	result[0] = 2.0f / (float)width;
	result[5] = -2.0f / (float)height;
	result[10] = homogeneous ? 2.0f / (depthfar - depthnear) : 1.0f / (depthfar - depthnear);
	result[12] = -1.0f;
	result[13] = 1.0f;
	result[14] = homogeneous ? -(depthfar + depthnear) / (depthfar - depthnear) : -depthnear / (depthfar - depthnear);
	result[15] = 1.0f;
}


/// <summary>
/// Turns a texture handle the toolkits carry back into the renderer's own.
/// Zero is reserved for "no texture", so the index is stored one higher than it is.
/// </summary>
static bgfx::TextureHandle Texture_From_Handle(uintptr_t handle)
{
	bgfx::TextureHandle texture = BGFX_INVALID_HANDLE;
	if (handle != 0) {
		texture.idx = (uint16_t)(handle - 1);
	}
	return(texture);
}


static uintptr_t Handle_From_Texture(bgfx::TextureHandle texture)
{
	return(bgfx::isValid(texture) ? (uintptr_t)texture.idx + 1 : 0);
}


class UIRenderInterface : public Rml::RenderInterface
{
	public:
		virtual Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) override;
		virtual void RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture) override;
		virtual void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override;

		virtual Rml::TextureHandle LoadTexture(Rml::Vector2i & dimensions, const Rml::String & source) override;
		virtual Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i dimensions) override;
		virtual void ReleaseTexture(Rml::TextureHandle texture) override;

		virtual void EnableScissorRegion(bool enable) override;
		virtual void SetScissorRegion(Rml::Rectanglei region) override;

	private:
		bool ScissorEnabled = false;
		Rml::Rectanglei Scissor = Rml::Rectanglei::FromPosition({0, 0});
};

static UIRenderInterface _RenderInterface;


Rml::CompiledGeometryHandle UIRenderInterface::CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices)
{
	if (!_Initialized || vertices.empty() || indices.empty()) {
		return(0);
	}

	UIGeometry * geometry = new UIGeometry;

	geometry->Vertices = bgfx::createVertexBuffer(
		bgfx::copy(vertices.data(), (uint32_t)(vertices.size() * sizeof(Rml::Vertex))), _RmlLayout);
	geometry->Indices = bgfx::createIndexBuffer(
		bgfx::copy(indices.data(), (uint32_t)(indices.size() * sizeof(int))), BGFX_BUFFER_INDEX32);
	geometry->IndexCount = (uint32_t)indices.size();

	if (!bgfx::isValid(geometry->Vertices) || !bgfx::isValid(geometry->Indices)) {
		ReleaseGeometry((Rml::CompiledGeometryHandle)geometry);
		return(0);
	}

	return((Rml::CompiledGeometryHandle)geometry);
}


void UIRenderInterface::RenderGeometry(Rml::CompiledGeometryHandle handle, Rml::Vector2f translation, Rml::TextureHandle texture)
{
	UIGeometry const * geometry = (UIGeometry const *)handle;
	if (!_Initialized || geometry == nullptr || _Width <= 0 || _Height <= 0) {
		return;
	}

	float transform[16];
	std::memset(transform, 0, sizeof(transform));
	transform[0] = transform[5] = transform[10] = transform[15] = 1.0f;
	transform[12] = translation.x;
	transform[13] = translation.y;

	bgfx::setTransform(transform);
	bgfx::setVertexBuffer(0, geometry->Vertices);
	bgfx::setIndexBuffer(geometry->Indices, 0, geometry->IndexCount);

	bgfx::TextureHandle bound = Texture_From_Handle((uintptr_t)texture);
	bgfx::setTexture(0, _TextureSampler, bgfx::isValid(bound) ? bound : _WhiteTexture,
		BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);

	if (ScissorEnabled) {
		// RmlUi reports the region relative to the context, which sits at the frame's top
		// left corner. bgfx wants it in the target's own coordinates.
		int left = std::max(Scissor.Left() + _OriginX, _OriginX);
		int top = std::max(Scissor.Top() + _OriginY, _OriginY);
		int right = std::min(Scissor.Right() + _OriginX, _OriginX + _Width);
		int bottom = std::min(Scissor.Bottom() + _OriginY, _OriginY + _Height);

		if (right <= left || bottom <= top) {
			return;
		}

		bgfx::setScissor((uint16_t)left, (uint16_t)top, (uint16_t)(right - left), (uint16_t)(bottom - top));
	}

	bgfx::setState(_BlendState);
	bgfx::submit(BACKEND_VIEW_UI, _Program);
}


void UIRenderInterface::ReleaseGeometry(Rml::CompiledGeometryHandle handle)
{
	UIGeometry * geometry = (UIGeometry *)handle;
	if (geometry == nullptr) {
		return;
	}

	if (bgfx::isValid(geometry->Vertices)) {
		bgfx::destroy(geometry->Vertices);
	}
	if (bgfx::isValid(geometry->Indices)) {
		bgfx::destroy(geometry->Indices);
	}

	delete geometry;
}


Rml::TextureHandle UIRenderInterface::LoadTexture(Rml::Vector2i & dimensions, const Rml::String & source)
{
	UIImageData image;
	if (!UI_Decode_Image(source.c_str(), image)) {
		DebugString("[UI] Image %s could not be read.\n", source.c_str());
		return(0);
	}

	dimensions.x = image.Width;
	dimensions.y = image.Height;

	return(GenerateTexture(Rml::Span<const Rml::byte>(image.Pixels.data(), image.Pixels.size()),
		Rml::Vector2i(image.Width, image.Height)));
}


Rml::TextureHandle UIRenderInterface::GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i dimensions)
{
	if (!_Initialized || dimensions.x <= 0 || dimensions.y <= 0) {
		return(0);
	}

	bgfx::TextureHandle texture = bgfx::createTexture2D(
		(uint16_t)dimensions.x, (uint16_t)dimensions.y, false, 1, bgfx::TextureFormat::RGBA8,
		BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
		bgfx::copy(source.data(), (uint32_t)source.size()));

	return((Rml::TextureHandle)Handle_From_Texture(texture));
}


void UIRenderInterface::ReleaseTexture(Rml::TextureHandle handle)
{
	bgfx::TextureHandle texture = Texture_From_Handle((uintptr_t)handle);
	if (bgfx::isValid(texture)) {
		bgfx::destroy(texture);
	}
}


void UIRenderInterface::EnableScissorRegion(bool enable)
{
	ScissorEnabled = enable;
}


void UIRenderInterface::SetScissorRegion(Rml::Rectanglei region)
{
	Scissor = region;
}


Rml::RenderInterface * UI_Render_Interface(void)
{
	return(&_RenderInterface);
}


/// <summary>
/// Creates the program, the sampler and the untextured stand-in the overlays draw with.
/// </summary>
/// <returns>bool; Is the overlay renderer ready to draw?</returns>
bool UI_Render_Init(void)
{
	if (_Initialized) {
		return(true);
	}

	// RmlUi's vertex is position, then a premultiplied RGBA byte colour, then the texture
	// coordinate. bgfx builds the layout in the order the attributes are added, so this
	// order is what makes the layout match the structure without a copy.
	_RmlLayout.begin()
		.add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
		.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
		.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
		.end();

	_ImGuiLayout.begin()
		.add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
		.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
		.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
		.end();

	bgfx::RendererType::Enum type = bgfx::getRendererType();
	bgfx::ShaderHandle vertexshader = bgfx::createEmbeddedShader(_EmbeddedShaders, type, "vs_ocornut_imgui");
	bgfx::ShaderHandle fragmentshader = bgfx::createEmbeddedShader(_EmbeddedShaders, type, "fs_ocornut_imgui");

	if (!bgfx::isValid(vertexshader) || !bgfx::isValid(fragmentshader)) {
		return(false);
	}

	_Program = bgfx::createProgram(vertexshader, fragmentshader, true);
	_TextureSampler = bgfx::createUniform("s_uitex", bgfx::UniformType::Sampler);

	// The program always samples, so untextured geometry is drawn against an opaque white
	// pixel and takes its colour from the vertices alone.
	const uint32_t white = 0xFFFFFFFF;
	_WhiteTexture = bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8,
		BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP, bgfx::copy(&white, sizeof(white)));

	if (!bgfx::isValid(_Program) || !bgfx::isValid(_TextureSampler) || !bgfx::isValid(_WhiteTexture)) {
		UI_Render_Shutdown();
		return(false);
	}

	_Initialized = true;
	return(true);
}


void UI_Render_Shutdown(void)
{
	if (bgfx::isValid(_WhiteTexture)) {
		bgfx::destroy(_WhiteTexture);
		_WhiteTexture = BGFX_INVALID_HANDLE;
	}
	if (bgfx::isValid(_TextureSampler)) {
		bgfx::destroy(_TextureSampler);
		_TextureSampler = BGFX_INVALID_HANDLE;
	}
	if (bgfx::isValid(_Program)) {
		bgfx::destroy(_Program);
		_Program = BGFX_INVALID_HANDLE;
	}

	_Initialized = false;
}


/// <summary>
/// Points both overlay views at the rectangle the frame was drawn into.
/// </summary>
void UI_Render_Begin(int destx, int desty, int width, int height)
{
	_OriginX = destx;
	_OriginY = desty;
	_Width = width;
	_Height = height;

	if (!_Initialized || width <= 0 || height <= 0) {
		return;
	}

	float projection[16];
	Build_Ortho_Projection(projection, width, height);

	for (bgfx::ViewId view : {(bgfx::ViewId)BACKEND_VIEW_UI, (bgfx::ViewId)BACKEND_VIEW_DEV}) {
		bgfx::setViewFrameBuffer(view, BGFX_INVALID_HANDLE);
		bgfx::setViewClear(view, BGFX_CLEAR_NONE);
		bgfx::setViewRect(view, (uint16_t)destx, (uint16_t)desty, (uint16_t)width, (uint16_t)height);
		bgfx::setViewTransform(view, nullptr, projection);
	}
}


void UI_Render_End(void)
{
}


/// <summary>
/// Draws one ImGui frame on the developer view.
/// The pinned ImGui asks the renderer to create, update and destroy its textures through
/// the draw data rather than owning a font atlas of its own.
/// </summary>
void UI_Render_ImGui(ImDrawData * data)
{
	if (!_Initialized || data == nullptr || data->CmdListsCount <= 0) {
		return;
	}

	for (ImTextureData * texture : *data->Textures) {
		if (texture->Status == ImTextureStatus_WantCreate) {
			bgfx::TextureHandle created = bgfx::createTexture2D(
				(uint16_t)texture->Width, (uint16_t)texture->Height, false, 1, bgfx::TextureFormat::RGBA8,
				BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
				bgfx::copy(texture->GetPixels(), (uint32_t)(texture->Width * texture->Height * 4)));

			texture->SetTexID((ImTextureID)Handle_From_Texture(created));
			texture->SetStatus(ImTextureStatus_OK);
		} else if (texture->Status == ImTextureStatus_WantUpdates) {
			bgfx::TextureHandle existing = Texture_From_Handle((uintptr_t)texture->TexID);
			if (bgfx::isValid(existing)) {
				bgfx::updateTexture2D(existing, 0, 0, 0, 0,
					(uint16_t)texture->Width, (uint16_t)texture->Height,
					bgfx::copy(texture->GetPixels(), (uint32_t)(texture->Width * texture->Height * 4)),
					(uint16_t)(texture->Width * 4));
			}
			texture->SetStatus(ImTextureStatus_OK);
		} else if (texture->Status == ImTextureStatus_WantDestroy) {
			bgfx::TextureHandle existing = Texture_From_Handle((uintptr_t)texture->TexID);
			if (bgfx::isValid(existing)) {
				bgfx::destroy(existing);
			}
			texture->SetTexID(ImTextureID_Invalid);
			texture->SetStatus(ImTextureStatus_Destroyed);
		}
	}

	for (int list = 0; list < data->CmdListsCount; list++) {
		ImDrawList const * commands = data->CmdLists[list];

		const uint32_t vertexcount = (uint32_t)commands->VtxBuffer.size();
		const uint32_t indexcount = (uint32_t)commands->IdxBuffer.size();

		if (bgfx::getAvailTransientVertexBuffer(vertexcount, _ImGuiLayout) < vertexcount
			|| bgfx::getAvailTransientIndexBuffer(indexcount) < indexcount) {
			break;
		}

		bgfx::TransientVertexBuffer vertices;
		bgfx::TransientIndexBuffer indices;
		bgfx::allocTransientVertexBuffer(&vertices, vertexcount, _ImGuiLayout);
		bgfx::allocTransientIndexBuffer(&indices, indexcount);

		std::memcpy(vertices.data, commands->VtxBuffer.begin(), vertexcount * sizeof(ImDrawVert));
		std::memcpy(indices.data, commands->IdxBuffer.begin(), indexcount * sizeof(ImDrawIdx));

		for (ImDrawCmd const & command : commands->CmdBuffer) {
			if (command.ElemCount == 0) {
				continue;
			}

			int left = std::max((int)command.ClipRect.x + _OriginX, _OriginX);
			int top = std::max((int)command.ClipRect.y + _OriginY, _OriginY);
			int right = std::min((int)command.ClipRect.z + _OriginX, _OriginX + _Width);
			int bottom = std::min((int)command.ClipRect.w + _OriginY, _OriginY + _Height);

			if (right <= left || bottom <= top) {
				continue;
			}

			bgfx::setScissor((uint16_t)left, (uint16_t)top, (uint16_t)(right - left), (uint16_t)(bottom - top));

			bgfx::TextureHandle texture = Texture_From_Handle((uintptr_t)command.GetTexID());
			bgfx::setTexture(0, _TextureSampler, bgfx::isValid(texture) ? texture : _WhiteTexture,
				BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);

			bgfx::setState(_BlendState);
			bgfx::setVertexBuffer(0, &vertices, command.VtxOffset, vertexcount - command.VtxOffset);
			bgfx::setIndexBuffer(&indices, command.IdxOffset, command.ElemCount);
			bgfx::submit(BACKEND_VIEW_DEV, _Program);
		}
	}
}
