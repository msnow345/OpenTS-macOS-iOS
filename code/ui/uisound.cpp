/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The sound and music screen's behavior, taken out of Sound_Option_Dialog_Func so that the
// dialog procedure and an RmlUi document can drive the same one.
//
// What the dialog did that is not obvious from the controls, and is kept here:
// the "Music Volume" slider sets Options.ScoreVolume; a volume dragged previews itself and
// the same volume is applied again without a preview when the screen is accepted; shuffle
// and repeat exclude one another, the newly checked one clearing the other; the track list
// holds the themes Theme.Is_Allowed admits, numbered from one in that order, and is built
// once when the screen opens, so a song starting later does not move the selection; and the
// screen has no cancel, because the template names no cancel button and the dialog
// procedure ignored the IDCANCEL that Escape produces.
//
// docs/UI_DESIGN.md, "Screens", owns the contract.

#include "always.h"

#include "uisound.h"

#include "uirmlview.h"

#include "audio/audioengine.h"
#include "globals.h"
#include "goptions.h"
#include "incdec.h"
#include "init.h"
#include "theme.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/Input.h>

#include <algorithm>
#include <cstdio>


/// <summary>Turns a volume into the slider step that stands for it.</summary>
static int Sound_Volume_To_Step(float volume)
{
	return((int)(volume * (double)UISoundPresenterClass::VOLUME_LEVELS + 0.5));
}


/// <summary>Turns a slider step back into the volume it stands for.</summary>
static float Sound_Step_To_Volume(int step)
{
	int const clamped = std::clamp(step, 0, UISoundPresenterClass::VOLUME_LEVELS);
	return((float)(clamped / (double)UISoundPresenterClass::VOLUME_LEVELS));
}


/// <summary>
/// Copies the engine's audio state into the view-model.
/// </summary>
void UISoundPresenterClass::Refresh(void)
{
	Available = AudioEngine.Is_Available();

	// The dialog picked its template by this, not by which menu opened it.
	HasMusic = (GameActive != false);

	MusicVolume = Sound_Volume_To_Step(Options.ScoreVolume);
	SoundVolume = Sound_Volume_To_Step(Options.SoundVolume);
	VoiceVolume = Sound_Volume_To_Step(Options.VoiceVolume);

	Shuffle = Options.IsScoreShuffle;
	Repeat = Options.IsScoreRepeat;

	Tracks.clear();
	Selected = 0;

	if (!HasMusic) {
		return;
	}

	int visible = 1;
	for (ThemeType index = THEME_FIRST; index < Theme.Max_Themes(); index++) {
		if (!Theme.Is_Allowed(index)) continue;

		char buffer[100];
		int const length = Theme.Track_Length(index);
		char const * const fullname = Theme.Full_Name(index);

		std::snprintf(buffer, sizeof(buffer), "%02d - %s [%d:%02d]", visible,
			(fullname != nullptr) ? fullname : "", length / 60, length % 60);
		visible++;

		if (Theme.What_Is_Playing() == index) {
			Selected = (int)Tracks.size();
		}

		TrackType track;
		track.Label = buffer;
		track.Theme = index;
		Tracks.push_back(track);
	}
}


/// <summary>
/// Answers an intent a view raised.
/// </summary>
void UISoundPresenterClass::Execute(UIIntent const & intent)
{
	if (intent.Action == UI_SOUND_MUSIC) {
		MusicVolume = std::clamp(intent.Value, 0, VOLUME_LEVELS);
		Options.Set_Score_Volume(Sound_Step_To_Volume(MusicVolume), true);

	} else if (intent.Action == UI_SOUND_SOUND) {
		SoundVolume = std::clamp(intent.Value, 0, VOLUME_LEVELS);
		Options.Set_Sound_Volume(Sound_Step_To_Volume(SoundVolume), true);

	} else if (intent.Action == UI_SOUND_VOICE) {
		VoiceVolume = std::clamp(intent.Value, 0, VOLUME_LEVELS);
		Options.Set_Voice_Volume(Sound_Step_To_Volume(VoiceVolume), true);

	} else if (intent.Action == UI_SOUND_SHUFFLE) {
		Shuffle = (intent.Value != 0);
		Options.Set_Shuffle(Shuffle);
		if (Shuffle) {
			Repeat = false;
			Options.Set_Repeat(false);
		}

	} else if (intent.Action == UI_SOUND_REPEAT) {
		Repeat = (intent.Value != 0);
		Options.Set_Repeat(Repeat);
		if (Repeat) {
			Shuffle = false;
			Options.Set_Shuffle(false);
		}

	} else if (intent.Action == UI_SOUND_SELECT) {
		if (intent.Value >= 0 && intent.Value < (int)Tracks.size()) {
			Selected = intent.Value;
		}

	} else if (intent.Action == UI_SOUND_PLAY) {
		if (Selected >= 0 && Selected < (int)Tracks.size()) {
			// Stopping first is what the dialog did, so the queued song starts rather than
			// waiting behind the one already playing.
			Theme.Stop();
			Theme.Queue_Song((ThemeType)Tracks[Selected].Theme);
		}

	} else if (intent.Action == UI_SOUND_STOP) {
		Theme.Queue_Song(THEME_QUIET);

	} else if (intent.Action == UI_SOUND_ACCEPT) {
		// The volumes are applied again without a preview, which is what the dialog's OK
		// handler did with the positions it read back off the sliders.
		Options.Set_Score_Volume(Sound_Step_To_Volume(MusicVolume), false);
		Options.Set_Sound_Volume(Sound_Step_To_Volume(SoundVolume), false);
		Options.Set_Voice_Volume(Sound_Step_To_Volume(VoiceVolume), false);

		UIResult result;
		result.Outcome = UIResult::OUTCOME_ACCEPTED;
		Result = result;
	}
}


/// <summary>
/// The maintenance the dialog driver ran on every pass of its own loop.
/// </summary>
void UISoundPresenterClass::Service(void)
{
	if (!GameActive) {
		Title_Screen_Restore();
	}
}


//---------------------------------------------------------------------------------------
// The RmlUi view. One document per dialog template, because the two templates differ by
// which controls exist rather than by how one is arranged.
//---------------------------------------------------------------------------------------

/// <summary>
/// The RmlUi half of the sound screen.
/// </summary>
class SoundViewClass : public UIRmlViewClass
{
	public:
		SoundViewClass(UISoundPresenterClass & presenter, char const * document);

		virtual void Bind(Rml::DataModelConstructor & model) override;
		virtual void Sync(void) override;

		// A slider takes its position from the model as the document loads, and that raises
		// a change event of its own. Nothing is queued until this is set, which is what
		// DialogInitialized did for the dialog's own WM_HSCROLL.
		void Settle(void) { Settled = true; }

	private:
		void Volume(char const * which, int step);

		UISoundPresenterClass & Screen;
		bool Settled = false;
};


SoundViewClass::SoundViewClass(UISoundPresenterClass & presenter, char const * document) :
	UIRmlViewClass(presenter, document),
	Screen(presenter)
{
}


void SoundViewClass::Volume(char const * which, int step)
{
	if (!Settled) return;

	// A position the screen already holds raises no intent, so setting a slider from the
	// model cannot preview a volume the player did not move.
	if (which == UI_SOUND_MUSIC && step == Screen.MusicVolume) return;
	if (which == UI_SOUND_SOUND && step == Screen.SoundVolume) return;
	if (which == UI_SOUND_VOICE && step == Screen.VoiceVolume) return;

	Screen.Queue(UIIntent{which, "", step});
}


void SoundViewClass::Bind(Rml::DataModelConstructor & model)
{
	if (auto track = model.RegisterStruct<UISoundPresenterClass::TrackType>()) {
		track.RegisterMember("label", &UISoundPresenterClass::TrackType::Label);
	}
	model.RegisterArray<std::vector<UISoundPresenterClass::TrackType>>();

	model.Bind("music", &Screen.MusicVolume);
	model.Bind("sound", &Screen.SoundVolume);
	model.Bind("voice", &Screen.VoiceVolume);
	model.Bind("shuffle", &Screen.Shuffle);
	model.Bind("repeat", &Screen.Repeat);
	model.Bind("available", &Screen.Available);
	model.Bind("tracks", &Screen.Tracks);
	model.Bind("selected", &Screen.Selected);

	// An event handler never acts: it queues, and the runner executes the queue after
	// Context::Update has returned.
	model.BindEventCallback("volume",
		[this](Rml::DataModelHandle, Rml::Event & event, Rml::VariantList const & arguments) {
			if (arguments.empty()) return;

			Rml::String const which = arguments[0].Get<Rml::String>();
			int const step = (int)(event.GetParameter<float>("value", 0.0f) + 0.5f);

			if (which == UI_SOUND_MUSIC) Volume(UI_SOUND_MUSIC, step);
			else if (which == UI_SOUND_SOUND) Volume(UI_SOUND_SOUND, step);
			else if (which == UI_SOUND_VOICE) Volume(UI_SOUND_VOICE, step);
		});

	model.BindEventCallback("toggle",
		[this](Rml::DataModelHandle, Rml::Event &, Rml::VariantList const & arguments) {
			if (arguments.empty()) return;

			Rml::String const which = arguments[0].Get<Rml::String>();
			if (which == UI_SOUND_SHUFFLE) {
				Screen.Queue(UIIntent{UI_SOUND_SHUFFLE, "", Screen.Shuffle ? 0 : 1});
			} else if (which == UI_SOUND_REPEAT) {
				Screen.Queue(UIIntent{UI_SOUND_REPEAT, "", Screen.Repeat ? 0 : 1});
			}
		});

	model.BindEventCallback("pick",
		[this](Rml::DataModelHandle, Rml::Event &, Rml::VariantList const & arguments) {
			if (arguments.empty()) return;
			Screen.Queue(UIIntent{UI_SOUND_SELECT, "", (int)arguments[0].Get<float>()});
		});

	model.BindEventCallback("press",
		[this](Rml::DataModelHandle, Rml::Event &, Rml::VariantList const & arguments) {
			if (arguments.empty()) return;

			Rml::String const what = arguments[0].Get<Rml::String>();
			if (what == UI_SOUND_PLAY) Screen.Queue(UIIntent{UI_SOUND_PLAY, "", 0});
			else if (what == UI_SOUND_STOP) Screen.Queue(UIIntent{UI_SOUND_STOP, "", 0});
			else if (what == UI_SOUND_ACCEPT) Screen.Queue(UIIntent{UI_SOUND_ACCEPT, "", 0});
		});

	// Enter accepts, because the template names no default push button and Windows then
	// sends the dialog IDOK. Escape does nothing, because the dialog procedure ignored the
	// IDCANCEL it produces, so this screen has no cancel either.
	model.BindEventCallback("key",
		[this](Rml::DataModelHandle, Rml::Event & event, Rml::VariantList const &) {
			int const key = event.GetParameter<int>("key_identifier", Rml::Input::KI_UNKNOWN);
			if (key == Rml::Input::KI_RETURN || key == Rml::Input::KI_NUMPADENTER) {
				Screen.Queue(UIIntent{UI_SOUND_ACCEPT, "", 0});
			}
		});
}


void SoundViewClass::Sync(void)
{
	if (!Model) return;

	// Only what an executed intent can change is dirtied. The volumes are not, because a
	// slider already carries the position its own change event reported.
	Model.DirtyVariable("shuffle");
	Model.DirtyVariable("repeat");
	Model.DirtyVariable("selected");
}


/// <summary>
/// Shows the sound controls and waits for the player to accept them.
/// </summary>
UIResult UI_Sound_Screen(UISoundPresenterClass & presenter)
{
	SoundViewClass view(presenter, presenter.Is_Lite() ? "soundlite.rml" : "sound.rml");

	if (!view.Prepare(true)) {
		UIResult result;
		result.Outcome = UIResult::OUTCOME_FAILED_TO_OPEN;
		return(result);
	}

	view.Settle();

	UIResult const result = UI_Run_Modal(presenter, view);
	view.Close();
	return(result);
}
