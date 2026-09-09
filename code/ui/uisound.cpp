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

#include "audio/audioengine.h"
#include "globals.h"
#include "goptions.h"
#include "incdec.h"
#include "init.h"
#include "theme.h"

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
