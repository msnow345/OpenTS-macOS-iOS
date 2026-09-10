/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The mode icon: the cursor for the mode the player has armed, drawn in the corner of the
// frame while it is armed.
//
// The pointer's shape is where this game says a superweapon is aimed, or that the next tap
// sells or repairs. A display that draws no pointer says none of it, and there is nothing
// else on screen that does, so the corner says it instead.
//
// The mode is read from the flags that hold it, not from the shape the engine last chose.
// DisplayClass picks that shape from what lies under the pointer, so with sell armed and
// nothing sellable beneath it the sell cursor is never asked for at all -- and an armed mode
// with nothing under the finger is exactly the case this exists for. Everything the pointer
// shape can say beyond the mode describes the spot it is on, which a player with no hover
// only learns after the tap that has already committed the action.
//
// docs/UI_DESIGN.md, "Screens" and "Assets and strings", own the contracts this keeps to.

#include "always.h"

#include "uiinternal.h"

#include "_map.h"
#include "action.hh"
#include "dbgprint.h"
#include "globals.h"
#include "mouse.h"
#include "object.h"
#include "stimer.h"
#include "suprtype.h"
#include "techno.h"
#include "timer.h"
#include "vector.h"
#include "video.h"
#include "win.h"
#include "winstub.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>


static char const * const DOCUMENT = "modeicon.rml";
static char const * const ICON_ID = "icon";

static Rml::ElementDocument * _Document = nullptr;
static Rml::Element * _Icon = nullptr;

// The shape frame the icon is showing, and the insets it was placed at. Both are compared
// before anything is written, so a pass that changes nothing touches neither the document nor
// the overlay's dirty flag.
static int _Frame = -1;
static int _Left = -1;
static int _Bottom = -1;

// The icon's own animation stage. The engine animates the cursor it is drawing, which is the
// shape the pointer is over rather than this one, and it stops stepping entirely while that
// shape is a still one -- so the icon steps its own, at the rate the shape's own entry in
// MouseControl asks for. The timer is the wall clock one the cursor animation runs on.
static MouseType _Shape = MOUSE_COUNT;
static CDTimerClass<SystemTimerClass> _Timer;
static int _Stage = 0;

// The document could not be loaded. One failure is enough; asking again every pass would
// report it every pass.
static bool _Failed = false;


/// <summary>
/// Answers whether the icon belongs on this display.
/// A display that draws a pointer already carries what the icon carries, so it does not get
/// one. OPENTS_MODE_ICON overrides that either way, because the corner is far easier to
/// work on where a pointer is visible than where it is not: 1 shows the icon, 0 hides it.
/// </summary>
static bool Wanted(void)
{
	// -2 until the environment has been read, -1 where it said nothing.
	static int forced = -2;

	if (forced == -2) {
		char const * const setting = std::getenv("OPENTS_MODE_ICON");
		forced = (setting != NULL && setting[0] != '\0') ? std::atoi(setting) : -1;
	}

	if (forced >= 0) {
		return(forced != 0);
	}

	return(!Win_Pointer_Is_Drawn());
}


/// <summary>
/// Is there a scenario whose mouse shapes mean anything, and is the tactical view the thing
/// on screen?
/// The shape the engine leaves behind outlives the scenario it was chosen in, so the icon
/// follows the scenario rather than the shape.
/// </summary>
static bool Belongs_On_Screen(void)
{
	return(GameActive && ScenarioActive && !UI_Modal_Is_Shown()
		&& MouseClass::MouseShapes != NULL && Wanted());
}


/// <summary>
/// The shape for the super weapon the sidebar armed.
/// A super weapon's own type carries the action it is aimed with, and that action is what the
/// tactical view picks its cursor from, so this is the cursor half of that choice and nothing
/// here decides which weapon uses which action. A weapon aimed with an action no cursor is
/// drawn for reports nothing rather than something wrong.
/// </summary>
static MouseType Armed_Super_Shape(SuperWeaponType super)
{
	if ((unsigned)super >= (unsigned)SuperWeaponTypes.Count()) {
		return(MOUSE_COUNT);
	}

	SuperWeaponTypeClass const * const type = SuperWeaponTypes[super];
	if (type == NULL) {
		return(MOUSE_COUNT);
	}

	switch (type->Action) {
		case ACTION_NUKE_BOMB:
			return(MOUSE_NUCLEAR_BOMB);

		case ACTION_AIR_STRIKE:
		case ACTION_ION_CANNON:
		case ACTION_DROP_POD:
			return(MOUSE_AIR_STRIKE);

		case ACTION_EMPULSE:
			return(MOUSE_EM_PULSE);

		case ACTION_EMPULSE_RANGE:
			return(MOUSE_EM_PULSE_RANGE);

		case ACTION_CHEM_BOMB:
			return(MOUSE_CHEMBOMB);

		case ACTION_SABOTAGE:
			return(MOUSE_DEMOLITIONS);

		default:
			return(MOUSE_COUNT);
	}
}


/// <summary>
/// Which shape reports what the player can do next, or MOUSE_COUNT while there is nothing to
/// report. Most of these are modes the player armed from the sidebar, which stay set wherever
/// the pointer goes until they act or cancel. A unit that can deploy is not a mode but belongs
/// here for the same reason: without a hovering pointer there is nothing else on screen to say
/// that tapping it turns it into a building. A building being placed is deliberately absent —
/// its own picture is already under the finger.
/// </summary>
static MouseType Armed_Shape(void)
{
	if (Map.IsSellMode) {
		return(MOUSE_SELL_BACK);
	}

	if (Map.IsRepairMode) {
		return(MOUSE_REPAIR);
	}

	if (Map.IsPowerMode) {
		return(MOUSE_TOGGLE_POWER);
	}

	if (Map.IsWaypointMode) {
		return(MOUSE_WAYPOINT);
	}

	if (Map.IsTargettingMode != SUPER_NONE) {
		return(Armed_Super_Shape(Map.IsTargettingMode));
	}

	// Can_Deploy_Now is the same question the engine asks before it offers ACTION_SELF, so the
	// icon appears exactly when a tap would deploy and not merely when a deployable unit is
	// selected somewhere it cannot unfold.
	for (int index = 0; index < CurrentObject.Count(); index++) {
		ObjectClass const * object = CurrentObject[index];

		if (object == NULL) {
			continue;
		}

		TechnoClass const * techno = object->As_TechnoClass();

		if (techno != NULL && techno->Can_Deploy_Now()) {
			return(MOUSE_DEPLOY);
		}
	}

	return(MOUSE_COUNT);
}


static void Hide(void)
{
	if (_Document != nullptr && _Document->IsVisible()) {
		_Document->Hide();
		UI_Mark_Overlay_Dirty();
	}
}


/// <summary>
/// Loads and shows the document the first time the icon is needed.
/// The document is not a screen: nothing on it can be pressed and it holds no input scope,
/// so it is shown on the context directly rather than through UIRmlViewClass, which would
/// hand the pointer to the host for as long as it was up.
/// </summary>
/// <returns>bool; Is there a document with an icon element in it?</returns>
static bool Ensure_Document(void)
{
	if (_Document != nullptr) {
		if (!_Document->IsVisible()) {
			_Document->Show(Rml::ModalFlag::None);
			UI_Mark_Overlay_Dirty();
		}
		return(true);
	}

	if (_Failed) {
		return(false);
	}

	Rml::Context * const context = UI_Overlay_Context();
	if (context == nullptr) {
		return(false);
	}

	_Document = context->LoadDocument(DOCUMENT);
	if (_Document == nullptr) {
		_Failed = true;
		DebugString("[UI] The document %s could not be loaded.\n", DOCUMENT);
		return(false);
	}

	_Icon = _Document->GetElementById(ICON_ID);
	if (_Icon == nullptr) {
		_Failed = true;
		DebugString("[UI] The document %s has no element called %s.\n", DOCUMENT, ICON_ID);
		_Document->Close();
		_Document = nullptr;
		return(false);
	}

	_Frame = -1;
	_Left = -1;
	_Bottom = -1;

	_Document->Show(Rml::ModalFlag::None);
	UI_Mark_Overlay_Dirty();
	return(true);
}


/// <summary>
/// Puts the icon as far into the corner as the host leaves free.
/// A modern tablet keeps the bottom of its display for a gesture of its own, and an icon
/// flush to that corner sits in it, so the icon is held off by exactly what the host says it
/// covers and no more. The document is laid out in frame pixels and the host answers in the
/// window's own, so the overlap is converted by the ratio the shell scales the context with.
/// </summary>
static void Place(void)
{
	VideoScaleInfo const & scale = Video_Get_Scale_Info();
	float const ratio = std::min(scale.ScaleX, scale.ScaleY);

	int left = 0;
	int bottom = 0;

	RECT safe;
	if (ratio > 0.0f && Win_Window_Safe_Area(MainWindow, safe)) {
		int const coveredleft = (int)safe.left - scale.DestX;
		int const coveredbottom = (scale.DestY + scale.DestHeight) - (int)safe.bottom;

		if (coveredleft > 0) left += (int)(coveredleft / ratio + 0.5f);
		if (coveredbottom > 0) bottom += (int)(coveredbottom / ratio + 0.5f);
	}

	if (left == _Left && bottom == _Bottom) {
		return;
	}

	_Left = left;
	_Bottom = bottom;

	char value[32];
	std::snprintf(value, sizeof(value), "%ddp", left);
	_Icon->SetProperty("left", value);
	std::snprintf(value, sizeof(value), "%ddp", bottom);
	_Icon->SetProperty("bottom", value);

	UI_Mark_Overlay_Dirty();
}


/// <summary>
/// Puts the armed mode's cursor in the corner, or takes the icon away.
/// Called from the shell's tick, so the icon follows the same wall clock the rest of the
/// overlay does and nothing here reads or advances a game timer.
/// </summary>
void UI_Mode_Icon_Service(void)
{
	MouseType const shape = Belongs_On_Screen() ? Armed_Shape() : MOUSE_COUNT;

	if (shape == MOUSE_COUNT) {
		_Shape = MOUSE_COUNT;
		Hide();
		return;
	}

	if (!Ensure_Document()) {
		return;
	}

	Place();

	int const count = Map.Get_Mouse_Frame_Count(shape);
	int const rate = Map.Get_Mouse_Frame_Rate(shape);

	if (shape != _Shape) {
		_Shape = shape;
		_Stage = 0;
		_Timer = rate;
	} else if (rate > 0 && count > 1 && _Timer == 0) {
		_Stage = (_Stage + 1) % count;
		_Timer = rate;
	}

	// The large form always. The small cursors exist for a pointer over a crowded map, which
	// is not what a corner is.
	int const frame = Map.Get_Mouse_Start_Frame(shape) + (count > 0 ? _Stage % count : 0);

	if (frame == _Frame) {
		return;
	}

	_Frame = frame;

	char source[64];
	std::snprintf(source, sizeof(source), "mouse.shp#%d#mousepal.pal", frame);
	_Icon->SetAttribute("src", source);

	UI_Mark_Overlay_Dirty();
}


void UI_Mode_Icon_Shutdown(void)
{
	if (_Document != nullptr) {
		_Document->Close();
		_Document = nullptr;
	}

	_Icon = nullptr;
	_Frame = -1;
	_Left = -1;
	_Bottom = -1;
	_Shape = MOUSE_COUNT;
	_Stage = 0;
	_Failed = false;
}
