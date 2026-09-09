/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// What the shell's own translation units hand each other. Nothing outside code/ui includes
// this. It names RmlUi types by forward declaration so that uirender.cpp stays the only
// file carrying bgfx and uitexture.cpp the only one carrying the image decoders.

#pragma once

#include <cstdint>
#include <vector>

struct ImDrawData;

namespace Rml {
	class RenderInterface;
	class SystemInterface;
	class FileInterface;
	class FontEngineInterface;
}


// Decoded image pixels, RGBA8 with premultiplied alpha, top row first.
struct UIImageData
{
	std::vector<unsigned char> Pixels;
	int Width = 0;
	int Height = 0;
};


// uitexture.cpp
bool UI_Decode_Image(char const * source, UIImageData & image);

// uifont.cpp
Rml::FontEngineInterface * UI_Font_Interface(void);
void UI_Font_Shutdown(void);

// uirender.cpp
Rml::RenderInterface * UI_Render_Interface(void);
bool UI_Render_Init(void);
void UI_Render_Shutdown(void);

// Points the overlay views at where the frame landed in the window. Coordinates handed to
// the toolkits afterwards are physical pixels from the frame's top left corner.
void UI_Render_Begin(int destx, int desty, int width, int height);
void UI_Render_End(void);
void UI_Render_ImGui(ImDrawData * data);

// uimessagebox.cpp
void UI_Message_Box_Service(void);

// uisurface.cpp
void UI_Surface_Element_Init(void);
void UI_Surface_Element_Shutdown(void);

// uishell.cpp. Puts what the shell draws on screen, which is the synchronous repaint a
// modeless dialog got from SendMessage(WM_PAINT). Only a screen with no loop of its own
// needs it; a screen inside UI_Run_Modal is presented by every pass. An immediate paint
// ignores the present pacing, which a box that must be seen before a long operation begins
// cannot afford to be skipped by.
void UI_Paint_Now(bool immediate);

// Turns an identifier RmlUi reports back into the Win32 virtual key it came from. A screen
// that records a keypress needs it, because an RmlUi key event carries the identifier and
// the game's own key encoding is a virtual key with its modifier bits above it. Zero for an
// identifier no key produces.
int UI_Virtual_Key(int identifier);

// uisystem.cpp
Rml::SystemInterface * UI_System_Interface(void);

// uifile.cpp
Rml::FileInterface * UI_File_Interface(void);

// uidev.cpp
bool UI_Dev_Init(void);
void UI_Dev_Shutdown(void);
void UI_Dev_New_Frame(int width, int height, double deltaseconds);
void UI_Dev_Render(void);
bool UI_Dev_Wants_Mouse(void);
bool UI_Dev_Wants_Keyboard(void);
bool UI_Dev_Is_Open(void);
void UI_Dev_Toggle(void);
void UI_Dev_Mouse_Position(float x, float y);
void UI_Dev_Mouse_Button(int button, bool down);
void UI_Dev_Mouse_Wheel(float delta);
