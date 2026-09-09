/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The ImGui context and the developer overlays it draws. Tool visibility and frame rate
// never touch deterministic state, and the overlays are armed by a developer key rather
// than by anything a player can reach.
//
// docs/UI_DESIGN.md, "Dear ImGui", owns what belongs here.

#include "always.h"

#include "uiinternal.h"

#include "dbgprint.h"

#include <imgui.h>


static ImGuiContext * _Context = nullptr;
static bool _Open = false;
static bool _FrameStarted = false;


bool UI_Dev_Init(void)
{
	if (_Context != nullptr) {
		return(true);
	}

	IMGUI_CHECKVERSION();
	_Context = ImGui::CreateContext();
	if (_Context == nullptr) {
		return(false);
	}

	ImGuiIO & io = ImGui::GetIO();

	// The renderer creates and destroys textures on request through the draw data rather
	// than from a font atlas it owns, which is the pinned version's backend contract.
	io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
	io.BackendPlatformName = "OpenTS UI shell";
	io.BackendRendererName = "OpenTS bgfx overlay";

	// Nothing is written beside the executable: the engine keeps its own settings.
	io.IniFilename = nullptr;
	io.LogFilename = nullptr;

	return(true);
}


void UI_Dev_Shutdown(void)
{
	if (_Context == nullptr) {
		return;
	}

	ImGui::DestroyContext(_Context);
	_Context = nullptr;
	_Open = false;
	_FrameStarted = false;
}


bool UI_Dev_Is_Open(void)
{
	return(_Context != nullptr && _Open);
}


void UI_Dev_Toggle(void)
{
	if (_Context == nullptr) {
		return;
	}

	_Open = !_Open;
	DebugString("[UI] Developer overlay %s.\n", _Open ? "shown" : "hidden");
}


/// <summary>
/// Builds the overlay's frame. Called from the shell's tick, on wall-clock time.
/// </summary>
void UI_Dev_New_Frame(int width, int height, double deltaseconds)
{
	if (_Context == nullptr || !_Open || width <= 0 || height <= 0) {
		return;
	}

	ImGuiIO & io = ImGui::GetIO();
	io.DisplaySize = ImVec2((float)width, (float)height);
	io.DeltaTime = deltaseconds > 0.0 ? (float)deltaseconds : (1.0f / 60.0f);

	ImGui::NewFrame();
	_FrameStarted = true;

	ImGui::SetNextWindowPos(ImVec2(16.0f, 16.0f), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("OpenTS")) {
		ImGui::Text("Overlay %d x %d", width, height);
		ImGui::Text("%.1f frames per second", io.Framerate);
	}
	ImGui::End();

	ImGui::Render();
}


void UI_Dev_Render(void)
{
	if (_Context == nullptr || !_FrameStarted) {
		return;
	}

	UI_Render_ImGui(ImGui::GetDrawData());
	_FrameStarted = false;
}


bool UI_Dev_Wants_Mouse(void)
{
	return(_Context != nullptr && _Open && ImGui::GetIO().WantCaptureMouse);
}


bool UI_Dev_Wants_Keyboard(void)
{
	return(_Context != nullptr && _Open && ImGui::GetIO().WantCaptureKeyboard);
}


void UI_Dev_Mouse_Position(float x, float y)
{
	if (_Context == nullptr || !_Open) {
		return;
	}

	ImGui::GetIO().AddMousePosEvent(x, y);
}


void UI_Dev_Mouse_Button(int button, bool down)
{
	if (_Context == nullptr || !_Open || button < 0 || button > 4) {
		return;
	}

	ImGui::GetIO().AddMouseButtonEvent(button, down);
}


void UI_Dev_Mouse_Wheel(float delta)
{
	if (_Context == nullptr || !_Open) {
		return;
	}

	ImGui::GetIO().AddMouseWheelEvent(0.0f, delta);
}
