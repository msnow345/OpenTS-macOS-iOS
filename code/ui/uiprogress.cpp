/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The progress and wait box. It is the first screen whose picture the engine draws: the bar
// is the game's own artwork clipped to the part of the job that is finished, so it reaches
// the document through the <surface> element rather than as a file the document names.
//
// What is preserved from IDD_PROGRESS_WAIT, and where each came from: the bar is the first
// frame of the shape the caller named, drawn from its left edge and cut off at the fraction
// finished, which is what ProgressScreenClass::Display_Progress does with
// Shape->Get_Rect(0); the bar is centered on the frame the template calls
// IDC_PROGRESS_BAR_FRAME; and the caption is the template's own literal, because that
// template holds the text rather than a string identifier.
//
// The box is not modal. Its callers -- the map generator and the file transfer -- keep
// running their own loops underneath it and close it when the job ends.
//
// docs/UI_DESIGN.md, "Screens" and "Assets and strings", own the contracts this keeps to.

#include "always.h"

#include "uiprogress.h"

#include "uiinternal.h"
#include "uirmlview.h"
#include "uisurface.h"

#include "_convert.h"
#include "dbgprint.h"
#include "_mixfile.h"
#include "convert.h"
#include "draw.h"
#include "mixfile.h"
#include "shapeset.h"

#include <RmlUi/Core/DataModelHandle.h>

#include <algorithm>
#include <memory>
#include <string>


// The caption IDD_PROGRESS_WAIT names. The template carries the text itself rather than a
// string identifier, so there is nothing to look up.
static char const * const DEFAULT_CAPTION = "Working - Please Wait";

// The name the document gives the bar's pixels.
static char const * const BAR_SURFACE = "progressbar";


/// <summary>
/// The toolkit-free half of the box. It has no actions: the job underneath moves the bar
/// and nothing the player does reaches the screen.
/// </summary>
class ProgressWaitPresenterClass : public UIPresenterClass
{
	public:
		std::string Caption = DEFAULT_CAPTION;

		// The part of the job that is finished, 0 to 1.
		double Fraction = 0.0;

		// The name of the bar artwork, as Set_Graphic_Data names it.
		std::string BarShape;

		virtual void Execute(UIIntent const &) override {}
		virtual void Refresh(void) override {}
};


/// <summary>
/// The bar's pixels. The shape is drawn into an engine surface exactly as the dialog drew
/// it, and the element converts and uploads that when it changes.
/// </summary>
class ProgressBarSurfaceClass : public UISurfaceBufferClass
{
	public:
		ProgressBarSurfaceClass(ShapeSet * shape, int width, int height) :
			UISurfaceBufferClass(width, height),
			Shape(shape)
		{
		}

		void Draw(double fraction);

	private:
		ShapeSet * Shape = nullptr;
};


void ProgressBarSurfaceClass::Draw(double fraction)
{
	Clear();

	if (Shape == nullptr) {
		return;
	}

	// The dialog cut the bar off at the fraction finished by shrinking the shape's own
	// rectangle, which is what keeps a part-drawn bar the same pixels as a full one.
	Rect rect = Shape->Get_Rect(0);
	rect.Width = (int)(rect.Width * std::clamp(fraction, 0.0, 1.0));
	rect.X = 0;
	rect.Y = 0;

	Draw_Shape(Get_Surface(), *NormalDrawer, Shape, 0, Point2D(0, 0), rect, SHAPE_WIN_REL);
	Mark_Dirty();
}


class ProgressWaitViewClass : public UIRmlViewClass
{
	public:
		ProgressWaitViewClass(ProgressWaitPresenterClass & presenter);
		virtual ~ProgressWaitViewClass(void) override;

		virtual void Bind(Rml::DataModelConstructor & model) override;
		virtual void Sync(void) override;

		// Attaches the artwork the presenter named and draws the bar at its current value.
		void Attach_Bar(void);

	private:
		ProgressWaitPresenterClass & Screen;
		std::unique_ptr<ProgressBarSurfaceClass> Bar;
};


ProgressWaitViewClass::ProgressWaitViewClass(ProgressWaitPresenterClass & presenter) :
	UIRmlViewClass(presenter, "progresswait.rml"),
	Screen(presenter)
{
}


ProgressWaitViewClass::~ProgressWaitViewClass(void)
{
	// The registration goes before the provider does, so nothing can be asked for pixels
	// that have been freed.
	UI_Unregister_Surface(BAR_SURFACE);
}


void ProgressWaitViewClass::Bind(Rml::DataModelConstructor & model)
{
	model.Bind("caption", &Screen.Caption);
}


void ProgressWaitViewClass::Sync(void)
{
	if (Bar != nullptr) {
		Bar->Draw(Screen.Fraction);
	}
}


void ProgressWaitViewClass::Attach_Bar(void)
{
	UI_Unregister_Surface(BAR_SURFACE);
	Bar.reset();

	if (Screen.BarShape.empty()) {
		return;
	}

	ShapeSet * const shape = (ShapeSet *)MFCD::Retrieve(Screen.BarShape.c_str());
	if (shape == nullptr) {
		DebugString("[UI] The progress bar artwork %s is not in the mix files.\n", Screen.BarShape.c_str());
		return;
	}

	Rect const rect = shape->Get_Rect(0);
	if (rect.Width <= 0 || rect.Height <= 0) {
		return;
	}

	Bar = std::make_unique<ProgressBarSurfaceClass>(shape, rect.Width, rect.Height);
	Bar->Draw(Screen.Fraction);

	// The element takes its size from the provider, and notices the registration on the next
	// context update.
	UI_Register_Surface(BAR_SURFACE, Bar.get());
}


// The one progress box. Every caller opens one, holds it for the length of a job and closes
// it, and no caller opens a second while one is up.
static std::unique_ptr<ProgressWaitPresenterClass> _Presenter;
static std::unique_ptr<ProgressWaitViewClass> _View;


bool UI_Progress_Wait_Open(void)
{
	if (_View != nullptr) {
		return(false);
	}

	// The presenter is created first so that it is destroyed last, because the data model
	// the view binds reads the presenter's view-model.
	_Presenter = std::make_unique<ProgressWaitPresenterClass>();
	_View = std::make_unique<ProgressWaitViewClass>(*_Presenter);

	if (!_View->Prepare(false)) {
		_View.reset();
		_Presenter.reset();
		return(false);
	}

	// The box must be on screen before the job underneath begins, or a caller that never
	// pumps again shows nothing at all.
	UI_Paint_Now(true);
	return(true);
}


void UI_Progress_Wait_Set_Bar(char const * shape)
{
	if (_View == nullptr) {
		return;
	}

	_Presenter->BarShape = (shape != nullptr) ? shape : "";
	_View->Attach_Bar();
	UI_Paint_Now(true);
}


void UI_Progress_Wait_Set_Progress(double fraction)
{
	if (_View == nullptr) {
		return;
	}

	_Presenter->Fraction = fraction;
	_View->Sync();

	// The dialog repainted itself where the gauge moved, through a synchronous WM_PAINT.
	UI_Paint_Now(false);
}


void UI_Progress_Wait_Close(void)
{
	if (_View == nullptr) {
		return;
	}

	_View->Close();
	_View.reset();
	_Presenter.reset();
}


bool UI_Progress_Wait_Is_Open(void)
{
	return(_View != nullptr);
}

