/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// Turns an image a document names into the premultiplied RGBA8 pixels the render interface
// uploads. The bytes come through the game's file system, so an image resolves from a mix
// exactly as a document does.
//
// Images resolve by extension. PNG and TGA decode through bimg, which bgfx already carries.
// PCX goes through Read_PCX_File and SHP through ShapeSet, which is how the game's own
// artwork reaches a document.
//
// The source string is a file name followed by up to two '#' arguments:
//
//     dbak6440.pcx                 the picture, through the palette it carries
//     dbak6440.pcx#mousepal.pal    the picture, through a named palette instead
//     mouse.shp#0                  one frame, through the game palette
//     mouse.shp#0#mousepal.pal     one frame, through a named palette
//
// A .pal file holds the six-bit guns the video hardware wanted, so its values are scaled
// the way init.cpp scales the palettes it loads. Shape index zero is transparent.
//
// docs/UI_DESIGN.md, "Assets and strings", owns the routing.

#include "always.h"

#include "uiinternal.h"

#include "_palette.h"
#include "ccfile.h"
#include "dbgprint.h"
#include "dsurface.h"
#include "pcx.h"
#include "shapeset.h"

#include <bx/allocator.h>
#include <bimg/decode.h>

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>


static bx::DefaultAllocator _Allocator;


static std::string Extension_Of(char const * source)
{
	std::string const name = source != NULL ? source : "";
	std::size_t const dot = name.find_last_of('.');
	if (dot == std::string::npos) {
		return("");
	}

	std::string extension = name.substr(dot + 1);
	for (char & letter : extension) {
		letter = (char)std::tolower((unsigned char)letter);
	}
	return(extension);
}


/// <summary>
/// Strips every directory a reference carries, matching how uifile.cpp resolves names.
/// </summary>
static std::string Base_Name(char const * source)
{
	std::string const path = source != NULL ? source : "";
	std::size_t const mark = path.find_last_of("\\/:");
	return(mark == std::string::npos ? path : path.substr(mark + 1));
}


static bool Read_Whole_File(char const * name, std::vector<unsigned char> & bytes)
{
	CCFileClass file(name);

	if (!file.Is_Available() || !file.Open(FileClass::READ)) {
		return(false);
	}

	int const size = file.Size();
	if (size <= 0) {
		file.Close();
		return(false);
	}

	bytes.resize((std::size_t)size);
	int const read = file.Read(bytes.data(), size);
	file.Close();

	return(read == size);
}


/// <summary>
/// Reads one of the game's palette files.
/// </summary>
/// <returns>bool; Was a full 256 colour palette read?</returns>
static bool Read_Palette_File(char const * name, PaletteClass & palette)
{
	std::vector<unsigned char> bytes;

	if (!Read_Whole_File(name, bytes) || bytes.size() < 256 * 3) {
		return(false);
	}

	for (int index = 0; index < 256; index++) {
		palette[index] = RGBClass(
			(unsigned char)(bytes[index * 3 + 0] << 2),
			(unsigned char)(bytes[index * 3 + 1] << 2),
			(unsigned char)(bytes[index * 3 + 2] << 2));
	}

	return(true);
}


static void Store_Opaque_Pixel(UIImageData & image, std::size_t offset, RGBClass const & color)
{
	image.Pixels[offset + 0] = (unsigned char)color.Get_Red();
	image.Pixels[offset + 1] = (unsigned char)color.Get_Green();
	image.Pixels[offset + 2] = (unsigned char)color.Get_Blue();
	image.Pixels[offset + 3] = 255;
}


/// <summary>
/// Decodes one of the game's PCX pictures.
/// </summary>
/// <param name="name">The file name, resolved through the game's file system.</param>
/// <param name="palette_name">A palette to use instead of the one the file carries, or an
/// empty string for the file's own.</param>
/// <returns>bool; Was the picture decoded?</returns>
static bool Decode_PCX(std::string const & name, std::string const & palette_name, UIImageData & image)
{
	CCFileClass file(name.c_str());
	PaletteClass palette;

	Surface * picture = Read_PCX_File(file, &palette);
	if (picture == NULL) {
		return(false);
	}

	if (!palette_name.empty() && !Read_Palette_File(palette_name.c_str(), palette)) {
		DebugString("[UI] Palette %s could not be read for %s.\n", palette_name.c_str(), name.c_str());
	}

	image.Width = picture->Get_Width();
	image.Height = picture->Get_Height();

	if (image.Width <= 0 || image.Height <= 0) {
		delete picture;
		return(false);
	}

	image.Pixels.assign((std::size_t)image.Width * image.Height * 4, 0);

	int const stride = picture->Stride();
	unsigned char const * bits = (unsigned char const *)picture->Lock();

	if (bits == NULL) {
		delete picture;
		return(false);
	}

	if (picture->Bytes_Per_Pixel() == 1) {
		for (int y = 0; y < image.Height; y++) {
			unsigned char const * row = bits + (std::size_t)y * stride;
			for (int x = 0; x < image.Width; x++) {
				Store_Opaque_Pixel(image, ((std::size_t)y * image.Width + x) * 4, palette[row[x]]);
			}
		}
	} else {

		// A three plane PCX is decoded straight into the primary's own packing, so the
		// component masks the video mode established are what take it apart again.
		for (int y = 0; y < image.Height; y++) {
			unsigned short const * row = (unsigned short const *)(bits + (std::size_t)y * stride);
			for (int x = 0; x < image.Width; x++) {
				unsigned short const pixel = row[x];
				RGBClass const color(
					(unsigned char)((pixel >> DSurface::RedRight) << DSurface::RedLeft),
					(unsigned char)((pixel >> DSurface::GreenRight) << DSurface::GreenLeft),
					(unsigned char)((pixel >> DSurface::BlueRight) << DSurface::BlueLeft));
				Store_Opaque_Pixel(image, ((std::size_t)y * image.Width + x) * 4, color);
			}
		}
	}

	picture->Unlock();
	delete picture;

	return(true);
}


/// <summary>
/// Decodes one frame of a shape file into the shape's logical rectangle.
/// Index zero is transparent, so a frame keeps the position its sub-rectangle gives it and
/// the pixels around it stay clear.
/// </summary>
/// <returns>bool; Was the frame decoded?</returns>
static bool Decode_SHP(std::string const & name, int frame, std::string const & palette_name, UIImageData & image)
{
	std::vector<unsigned char> bytes;

	if (!Read_Whole_File(name.c_str(), bytes) || bytes.size() < sizeof(ShapeSet)) {
		return(false);
	}

	PaletteClass palette = GamePalette;
	if (!palette_name.empty() && !Read_Palette_File(palette_name.c_str(), palette)) {
		DebugString("[UI] Palette %s could not be read for %s.\n", palette_name.c_str(), name.c_str());
	}

	ShapeSet const * shape = (ShapeSet const *)bytes.data();

	image.Width = shape->Get_Width();
	image.Height = shape->Get_Height();

	if (image.Width <= 0 || image.Height <= 0 || frame < 0 || frame >= shape->Get_Count()) {
		return(false);
	}

	image.Pixels.assign((std::size_t)image.Width * image.Height * 4, 0);

	Rect const rect = shape->Get_Rect(frame);
	unsigned char const * data = (unsigned char const *)shape->Get_Data(frame);

	if (data == NULL || rect.Width <= 0 || rect.Height <= 0) {
		return(true);
	}

	if (rect.X < 0 || rect.Y < 0
		|| rect.X + rect.Width > image.Width || rect.Y + rect.Height > image.Height) {
		DebugString("[UI] Shape %s frame %d claims a rectangle outside its own bounds.\n", name.c_str(), frame);
		return(false);
	}

	bool const compressed = shape->Is_RLE_Compressed(frame);
	unsigned char const * const end = bytes.data() + bytes.size();
	unsigned char const * line = data;

	for (int y = 0; y < rect.Height; y++) {

		// A compressed line starts with its own byte length and then runs of pixels, where a
		// zero introduces a count of transparent ones.
		unsigned char const * source = compressed ? line + sizeof(unsigned short) : data + (std::size_t)y * rect.Width;
		int x = 0;

		while (x < rect.Width) {

			if (source >= end) {
				DebugString("[UI] Shape %s frame %d runs past the end of the file.\n", name.c_str(), frame);
				return(false);
			}

			unsigned char const index = *source++;

			if (index == 0) {
				if (compressed) {
					if (source >= end) {
						return(false);
					}
					unsigned char const run = *source++;

					// A zero length run would leave the line where it is, so it counts as one
					// pixel rather than as a reason to stop moving.
					x += run != 0 ? run : 1;
				} else {
					x++;
				}
				continue;
			}

			std::size_t const offset = ((std::size_t)(rect.Y + y) * image.Width + (rect.X + x)) * 4;
			Store_Opaque_Pixel(image, offset, palette[index]);
			x++;
		}

		if (compressed) {
			if (line + sizeof(unsigned short) > end) {
				return(false);
			}
			unsigned short length;
			std::memcpy(&length, line, sizeof(length));
			if (length == 0) {
				return(false);
			}
			line += length;
		}
	}

	return(true);
}


static bool Decode_Through_Bimg(std::string const & name, UIImageData & image)
{
	std::vector<unsigned char> bytes;
	if (!Read_Whole_File(name.c_str(), bytes)) {
		return(false);
	}

	bimg::ImageContainer * container = bimg::imageParse(&_Allocator, bytes.data(),
		(uint32_t)bytes.size(), bimg::TextureFormat::RGBA8);

	if (container == NULL) {
		return(false);
	}

	image.Width = (int)container->m_width;
	image.Height = (int)container->m_height;
	image.Pixels.assign((unsigned char const *)container->m_data,
		(unsigned char const *)container->m_data + (std::size_t)image.Width * image.Height * 4);

	bimg::imageFree(container);

	// The render interface uploads premultiplied alpha, which is what RmlUi's texture
	// contract specifies, and a decoded file carries straight alpha.
	for (std::size_t pixel = 0; pixel + 3 < image.Pixels.size(); pixel += 4) {
		unsigned int const alpha = image.Pixels[pixel + 3];
		image.Pixels[pixel + 0] = (unsigned char)((image.Pixels[pixel + 0] * alpha + 127) / 255);
		image.Pixels[pixel + 1] = (unsigned char)((image.Pixels[pixel + 1] * alpha + 127) / 255);
		image.Pixels[pixel + 2] = (unsigned char)((image.Pixels[pixel + 2] * alpha + 127) / 255);
	}

	return(true);
}


/// <summary>
/// Reads an image a document referenced and hands back its pixels.
/// </summary>
/// <param name="source">The image source string as the document wrote it.</param>
/// <param name="image">Receives premultiplied RGBA8 pixels, top row first.</param>
/// <returns>bool; Was the image decoded?</returns>
bool UI_Decode_Image(char const * source, UIImageData & image)
{
	std::string const request = Base_Name(source);

	std::string name = request;
	std::string first;
	std::string second;

	std::size_t mark = name.find('#');
	if (mark != std::string::npos) {
		first = name.substr(mark + 1);
		name = name.substr(0, mark);

		mark = first.find('#');
		if (mark != std::string::npos) {
			second = first.substr(mark + 1);
			first = first.substr(0, mark);
		}
	}

	std::string const extension = Extension_Of(name.c_str());
	bool decoded = false;

	if (extension == "pcx") {
		decoded = Decode_PCX(name, first, image);
	} else if (extension == "shp") {
		decoded = Decode_SHP(name, std::atoi(first.c_str()), second, image);
	} else if (extension == "png" || extension == "tga") {
		decoded = Decode_Through_Bimg(name, image);
	} else {
		DebugString("[UI] Image %s has no reader.\n", request.c_str());
		return(false);
	}

	if (!decoded) {
		return(false);
	}

	return(image.Width > 0 && image.Height > 0);
}
