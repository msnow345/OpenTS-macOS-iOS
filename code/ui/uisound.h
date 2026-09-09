/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The sound and music screen's behavior, with no toolkit in it. Both views drive this one
// presenter: the OwnerDraw dialog procedure in sounddlg.cpp and the RmlUi document.
//
// docs/UI_DESIGN.md, "Screens", owns the contract.

#pragma once

#include "uiscreen.h"

#include <string>
#include <vector>


// What a view raises. Value carries the slider step, the check state or the list row.
inline constexpr char const * UI_SOUND_MUSIC = "music";      // Value: slider step
inline constexpr char const * UI_SOUND_SOUND = "sound";      // Value: slider step
inline constexpr char const * UI_SOUND_VOICE = "voice";      // Value: slider step
inline constexpr char const * UI_SOUND_SHUFFLE = "shuffle";  // Value: nonzero to shuffle
inline constexpr char const * UI_SOUND_REPEAT = "repeat";    // Value: nonzero to repeat
inline constexpr char const * UI_SOUND_SELECT = "select";    // Value: track list row
inline constexpr char const * UI_SOUND_PLAY = "play";
inline constexpr char const * UI_SOUND_STOP = "stop";
inline constexpr char const * UI_SOUND_ACCEPT = "accept";


class UISoundPresenterClass : public UIPresenterClass
{
	public:
		// The steps a volume is expressed in, which is the range the dialog's track bars
		// were given. A view shows steps; only this class knows what they mean.
		static int const VOLUME_LEVELS = 10;

		struct TrackType
		{
			std::string Label;
			int Theme = 0;
		};

		virtual void Execute(UIIntent const & intent) override;
		virtual void Refresh(void) override;
		virtual void Service(void) override;

		/*
		**	The view-model. Plain values, and the only thing a view reads.
		*/
		int MusicVolume = 0;
		int SoundVolume = 0;
		int VoiceVolume = 0;
		bool Shuffle = false;
		bool Repeat = false;

		// Can anything be heard? With no audio device every control is shown but disabled,
		// as the dialog disabled them.
		bool Available = false;

		// Does this screen carry the music controls? Only the template the game shows during
		// play does; the one it shows with no game running carries the three volumes alone.
		bool HasMusic = false;

		std::vector<TrackType> Tracks;
		int Selected = 0;

		// Which of the two dialog templates the state above corresponds to, for a view that
		// has to choose a document.
		bool Is_Lite(void) const { return(!HasMusic); }
};


// Shows the screen for the state the presenter was refreshed into and does not return until
// the player accepts it. A OUTCOME_FAILED_TO_OPEN result means the document could not be
// prepared and nothing was shown, which is the caller's cue to open the legacy dialog.
UIResult UI_Sound_Screen(UISoundPresenterClass & presenter);
