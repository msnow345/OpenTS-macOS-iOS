/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The <surface> element and the providers behind it. This is where engine-drawn pixels
// enter a document: the map preview, the desync host icons and the progress bar are all
// surfaces the game draws for itself, and none of them can be a file a document names.
//
// A document writes <surface src="name"/>. The name is looked up among the live providers
// at render time rather than at parse time, so a document may be shown before the screen
// that owns its pixels has registered them, and an element whose provider went away simply
// draws nothing rather than failing to lay out.
//
// The element uploads only when the provider's generation moves, so a document holding a
// surface costs one quad per present while nothing changes.
//
// docs/UI_DESIGN.md, "Assets and strings" and "Rendering", own the contracts here.

#include "always.h"

#include "uisurface.h"

#include "uiinternal.h"

#include "bsurface.h"
#include "dsurface.h"
#include "rgb.h"

#include <RmlUi/Core/ComputedValues.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementInstancer.h>
#include <RmlUi/Core/ElementUtilities.h>
#include <RmlUi/Core/Factory.h>
#include <RmlUi/Core/Geometry.h>
#include <RmlUi/Core/Mesh.h>
#include <RmlUi/Core/MeshUtilities.h>
#include <RmlUi/Core/RenderBox.h>
#include <RmlUi/Core/RenderManager.h>
#include <RmlUi/Core/Texture.h>

#include <map>
#include <memory>
#include <string>
#include <vector>


UISurfaceProviderClass::~UISurfaceProviderClass(void)
{
}


//---------------------------------------------------------------------------------------
// The registry. A provider is reached by name and nothing holds a pointer to one past its
// registration, so a screen that closes takes its pixels with it.
//---------------------------------------------------------------------------------------

static std::map<std::string, UISurfaceProviderClass *> _Providers;


void UI_Register_Surface(char const * name, UISurfaceProviderClass * provider)
{
	if (name == nullptr || name[0] == '\0' || provider == nullptr) {
		return;
	}

	_Providers[name] = provider;
}


void UI_Unregister_Surface(char const * name)
{
	if (name == nullptr) {
		return;
	}

	_Providers.erase(name);
}


static UISurfaceProviderClass * Find_Provider(std::string const & name)
{
	std::map<std::string, UISurfaceProviderClass *>::const_iterator found = _Providers.find(name);
	return(found == _Providers.end() ? nullptr : found->second);
}


//---------------------------------------------------------------------------------------
// The surface-backed provider.
//---------------------------------------------------------------------------------------

UISurfaceBufferClass::UISurfaceBufferClass(int width, int height) :
	Width(width > 0 ? width : 1),
	Height(height > 0 ? height : 1)
{
	Buffer = new BSurface(Width, Height, 2);
	Buffer->Fill(Transparent);
}


UISurfaceBufferClass::~UISurfaceBufferClass(void)
{
	delete Buffer;
	Buffer = nullptr;
}


void UISurfaceBufferClass::Clear(void)
{
	Buffer->Fill(Transparent);
	Mark_Dirty();
}


/// <summary>
/// Converts the engine surface into the premultiplied RGBA8 pixels a texture wants.
/// </summary>
/// <param name="pixels">Receives Get_Width() by Get_Height() pixels, top row first.</param>
/// <returns>bool; Were the pixels written?</returns>
bool UISurfaceBufferClass::Read_Pixels(unsigned char * pixels) const
{
	if (pixels == nullptr || Buffer == nullptr) {
		return(false);
	}

	unsigned short const * const source = (unsigned short const *)Buffer->Lock();
	if (source == nullptr) {
		return(false);
	}

	int const stride = Buffer->Stride() / (int)sizeof(unsigned short);
	unsigned short const transparent = (unsigned short)Transparent;

	for (int y = 0; y < Height; y++) {
		unsigned short const * row = source + (std::size_t)y * stride;
		unsigned char * out = pixels + (std::size_t)y * Width * 4;

		for (int x = 0; x < Width; x++) {
			unsigned short const pixel = row[x];

			if (pixel == transparent) {
				out[0] = 0;
				out[1] = 0;
				out[2] = 0;
				out[3] = 0;
			} else {
				// Opaque, so the premultiplied color the render interface expects is the
				// color itself.
				RGBClass const color = DSurface::Deconstruct_Hicolor_Pixel(pixel);
				out[0] = (unsigned char)color.Get_Red();
				out[1] = (unsigned char)color.Get_Green();
				out[2] = (unsigned char)color.Get_Blue();
				out[3] = 255;
			}

			out += 4;
		}
	}

	Buffer->Unlock();
	return(true);
}


//---------------------------------------------------------------------------------------
// The element.
//---------------------------------------------------------------------------------------

namespace {

class UISurfaceElement : public Rml::Element
{
	public:
		UISurfaceElement(Rml::String const & tag) : Rml::Element(tag) {}

		virtual bool GetIntrinsicDimensions(Rml::Vector2f & dimensions, float & ratio) override;

	protected:
		virtual void OnUpdate(void) override;
		virtual void OnDpRatioChange(void) override { DirtyLayout(); }
		virtual void OnRender(void) override;
		virtual void OnResize(void) override { GeometryIsStale = true; }
		virtual void OnAttributeChange(Rml::ElementAttributes const & changed) override;

	private:
		void Generate_Geometry(void);
		bool Refresh_Texture(UISurfaceProviderClass & provider);

		std::string Source;
		Rml::Geometry Quad;
		Rml::CallbackTexture Pixels;

		// The provider generation the texture was made from. Zero means there is no texture.
		unsigned int Uploaded = 0;

		// The provider extents the last layout was made from. A provider that registers
		// after the document was laid out, or one that changes size, is what these catch.
		int LaidOutWidth = -1;
		int LaidOutHeight = -1;

		bool GeometryIsStale = true;
};


/// <summary>
/// Notices a provider arriving, leaving or changing size, none of which the layout would
/// otherwise hear about, and asks for the element to be laid out again.
/// </summary>
void UISurfaceElement::OnUpdate(void)
{
	Rml::Element::OnUpdate();

	UISurfaceProviderClass const * const provider = Find_Provider(GetAttribute<Rml::String>("src", ""));
	int const width = (provider != nullptr) ? provider->Get_Width() : 0;
	int const height = (provider != nullptr) ? provider->Get_Height() : 0;

	if (width != LaidOutWidth || height != LaidOutHeight) {
		LaidOutWidth = width;
		LaidOutHeight = height;
		DirtyLayout();
	}
}


bool UISurfaceElement::GetIntrinsicDimensions(Rml::Vector2f & dimensions, float & ratio)
{
	UISurfaceProviderClass const * const provider = Find_Provider(GetAttribute<Rml::String>("src", ""));

	// A surface is sized in game logical units, which is what one authored density
	// independent pixel is, so the provider's own extents are its intrinsic size.
	float const width = (provider != nullptr) ? (float)provider->Get_Width() : 0.0f;
	float const height = (provider != nullptr) ? (float)provider->Get_Height() : 0.0f;

	if (height > 0.0f) {
		ratio = width / height;
	}

	// A surface's pixels are game logical units, and one authored density independent pixel
	// is one of those, so the extents follow the document's scale rather than staying at
	// their own pixel count.
	dimensions = Rml::Vector2f(width, height) * Rml::ElementUtilities::GetDensityIndependentPixelRatio(this);

	return(true);
}


void UISurfaceElement::OnAttributeChange(Rml::ElementAttributes const & changed)
{
	Rml::Element::OnAttributeChange(changed);

	if (changed.find("src") != changed.end()) {
		Pixels.Release();
		Uploaded = 0;
		GeometryIsStale = true;
		DirtyLayout();
	}
}


void UISurfaceElement::Generate_Geometry(void)
{
	Rml::Mesh mesh = Quad.Release(Rml::Geometry::ReleaseMode::ClearMesh);

	Rml::ComputedValues const & computed = GetComputedValues();
	Rml::ColourbPremultiplied const colour = computed.image_color().ToPremultiplied(computed.opacity());
	Rml::RenderBox const box = GetRenderBox(Rml::BoxArea::Content);

	Rml::MeshUtilities::GenerateQuad(mesh, box.GetFillOffset(), box.GetFillSize(), colour,
		Rml::Vector2f(0.0f, 0.0f), Rml::Vector2f(1.0f, 1.0f));

	if (Rml::RenderManager * manager = GetRenderManager()) {
		Quad = manager->MakeGeometry(std::move(mesh));
	}

	GeometryIsStale = false;
}


/// <summary>
/// Brings the texture up to the provider's current pixels, uploading only when they moved.
/// </summary>
/// <returns>bool; Is there a texture to draw?</returns>
bool UISurfaceElement::Refresh_Texture(UISurfaceProviderClass & provider)
{
	unsigned int const generation = provider.Get_Generation();
	if (Uploaded == generation && Pixels) {
		return(true);
	}

	Rml::RenderManager * const manager = GetRenderManager();
	if (manager == nullptr) {
		return(false);
	}

	int const width = provider.Get_Width();
	int const height = provider.Get_Height();
	if (width <= 0 || height <= 0) {
		return(false);
	}

	// The callback runs when the texture is first needed for drawing, which keeps the
	// conversion off the path that only lays the document out.
	Pixels.Release();
	Pixels = manager->MakeCallbackTexture(
		[&provider, width, height](Rml::CallbackTextureInterface const & texture) -> bool {
			std::vector<unsigned char> rgba((std::size_t)width * height * 4, 0);
			if (!provider.Read_Pixels(rgba.data())) {
				return(false);
			}
			return(texture.GenerateTexture(Rml::Span<const Rml::byte>(rgba.data(), rgba.size()),
				Rml::Vector2i(width, height)));
		});

	Uploaded = generation;
	return(true);
}


void UISurfaceElement::OnRender(void)
{
	std::string const source = GetAttribute<Rml::String>("src", "");
	if (source != Source) {
		Source = source;
		Uploaded = 0;
	}

	UISurfaceProviderClass * const provider = Find_Provider(Source);
	if (provider == nullptr) {
		return;
	}

	if (!Refresh_Texture(*provider)) {
		return;
	}

	if (GeometryIsStale) {
		Generate_Geometry();
	}

	Quad.Render(GetAbsoluteOffset(Rml::BoxArea::Border), Pixels);
}


static Rml::ElementInstancerGeneric<UISurfaceElement> _Instancer;

} // namespace


void UI_Surface_Element_Init(void)
{
	Rml::Factory::RegisterElementInstancer("surface", &_Instancer);
}


void UI_Surface_Element_Shutdown(void)
{
	_Providers.clear();
}
