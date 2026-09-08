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
// The engine's own PCX and SHP artwork, and the surfaces the engine draws at run time, are
// not read here yet; the first screen that shows game art adds them.
//
// docs/UI_DESIGN.md, "Assets and strings", owns the routing.

#include "always.h"

#include "uiinternal.h"

#include "ccfile.h"
#include "dbgprint.h"

#include <bx/allocator.h>
#include <bimg/decode.h>

#include <cctype>
#include <cstring>
#include <string>


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
/// Reads an image a document referenced and hands back its pixels.
/// </summary>
/// <param name="source">The image source string as the document wrote it.</param>
/// <param name="image">Receives premultiplied RGBA8 pixels, top row first.</param>
/// <returns>bool; Was the image decoded?</returns>
bool UI_Decode_Image(char const * source, UIImageData & image)
{
	std::string const name = Base_Name(source);
	std::string const extension = Extension_Of(name.c_str());

	if (extension != "png" && extension != "tga") {
		DebugString("[UI] Image %s has no reader; only PNG and TGA are read so far.\n", name.c_str());
		return(false);
	}

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

	return(image.Width > 0 && image.Height > 0);
}
