/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The UI shell's interface to the rest of the engine. No RmlUi, ImGui or bgfx type
// appears here, the way no bgfx type appears in bgfxbackend.h, so the engine reaches the
// shell without carrying any of those libraries' headers or build settings.
//
// docs/UI_DESIGN.md owns the architecture this belongs to.

#pragma once

#include <windows.h>


// Starts and stops the shell. The renderer must already be running, and the shell is torn
// down before it stops.
bool UI_Init(void);
void UI_Shutdown(void);

// Called after the frame's size or the drawable area's size changed, so the context, the
// coordinate mapping and the clipping move together with the frame.
void UI_On_Resize(void);

// Advances animation and layout for documents that are not being run by a modal loop.
void UI_Tick(void);

// Draws the overlays between the frame and the flip. Only video.cpp calls this.
void UI_Render_Overlay(void);

// Offers a window message to the toolkits before the game sees it. A true return means the
// message was consumed and the window procedure returns without handling it.
bool UI_Handle_Window_Message(HWND window, UINT message, WPARAM wparam, LPARAM lparam);

// Does anything the shell draws need putting on screen again? True keeps a present
// happening when the game's own frame has not changed.
bool UI_Overlay_Is_Dirty(void);

// Is an overlay document on screen? The coexistence rule in docs/UI_DESIGN.md forbids
// showing a legacy dialog while one is.
bool UI_Document_Is_Visible(void);

// Should a migrated screen use its RmlUi view rather than its legacy one? No screen has
// migrated yet, so this answers false until one has.
bool UI_Use_Rml(void);
