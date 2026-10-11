/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#ifndef FLAAKLYPA_GLOBALS_H
#define FLAAKLYPA_GLOBALS_H

#include "common/scummsys.h"

#include "flaaklypa/racing.h"

namespace Flaaklypa {

/** A note of the music fence's tune (8 bytes at DAT_006b6230). */
struct FenceNote {
	int32 pitch;         ///< 0..17 (A1 .. D3)
	int32 length;        ///< 0..7, see FenceScene's kLengthNames
};

/** The music fence's tune, kept across visits. */
struct FenceTune {
	enum {
		kMaxNotes = 0x400
	};
	FenceNote notes[kMaxNotes];  ///< DAT_006b6230
	int count = 0;               ///< DAT_006b8230
};

/**
 * The scene state the original keeps in global variables, so that it
 * outlives the scene: the random picks without repeats of the story pages,
 * the fence's tune, the race menu's settings and the like. The original
 * resets them only when the program starts; the engine owns them, so a
 * game started again from the launcher starts afresh.
 */
struct Globals {
	struct Anaglyph {
		int idleCount = 0, idleMask = 0;   ///< DAT_006d4884 / DAT_006d6f88: random idle clips played
		int hitCount = 0, hitMask = 0;     ///< DAT_006d47d4 / DAT_006d6f78: random hit clips played
	} anaglyph;

	struct Desk {
		int pick101Mask = 0, pick102Mask = 0, pick110Mask = 0; ///< variants played
	} desk;

	FenceTune fence;

	struct Gametrivia {
		uint32 nextPictureMask = 0;        ///< DAT_006d71a0: next picture clips played
	} gametrivia;

	struct Garage {
		int mask008 = 0, mask009 = 0;      ///< variants played since the last full round
	} garage;

	struct Goodbye {
		int fireworkCount = 0;             ///< DAT_0055d000: fireworks launched
	} goodbye;

	struct House {
		int maskWindow = 0, maskBench = 0, maskDoor = 0; ///< variants used since the last reset
	} house;

	struct Intent {
		int chessMask = 0;                 ///< variants played
	} intent;

	struct Mathlab {
		bool thirdDisabled = true;         ///< DAT_00512860 (.data, 1): the third container is covered
		bool hatchOpen = true;             ///< DAT_005120a8 (.data, 1): the hatch at the end of the rail is open
	} mathlab;

	struct Outtent {
		int cushionsMask = 0, grassMask = 0, tableMask = 0; ///< random pick masks
	} outtent;

	struct Pee {
		int bushMask = 0;                  ///< variants played
	} pee;

	struct Race {
		RaceSettings settings;             ///< DAT_006cfed0 (zeroed by the menu's init, FUN_0047b150)
		RaceScoreState score;              ///< DAT_006d05c8..DAT_006d0634
	} race;

	struct Sceneindex {
		int tab = 0;                       ///< DAT_0057e0ec: the tab shown
	} sceneindex;

	struct Tvroom {
		int mask005 = 0, maskBee = 0, maskMoose = 0, maskMooseClick = 0, maskSolan = 0;
	} tvroom;

	struct Yard {
		int introCount = 0, introMask = 0; ///< random pick without repeats
	} yard;

	// Value-initialized, so the plain arrays and structs start zeroed.
	Globals() : fence(), race() {}
};

} // End of namespace Flaaklypa

#endif
