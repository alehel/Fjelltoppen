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
#ifndef FLAAKLYPA_RACING_H
#define FLAAKLYPA_RACING_H

#include "common/scummsys.h"

#include "flaaklypa/hiscore.h"

namespace Flaaklypa {

/*
 * What the race menu hands the race (the 0x184 byte struct DAT_006cfed0 the
 * menu passes to FUN_0040cd70("racing", &settings); the story's race has
 * its own copy at 0x50aaf0).
 */

/** The controls of a player in one device (0x20 bytes, FUN_0047a980). */
struct RaceControls {
	enum {
		kAccelerate = 0,
		kReverse = 1,
		kLeft = 2,
		kRight = 3,
		kTrick = 4,
		kHorn = 5,
		kKeys = 6
	};
	enum {
		kTypeKeyboard = 1,
		kTypeGamepad1 = 2,
		kTypeGamepad2 = 3
	};
	int32 keys[kKeys];   ///< Windows virtual key codes (keyboard) or gamepad inputs
	int32 type;          ///< kType*
	int32 device;        ///< the input device (a pointer in the original), 0 when absent
};

/** A player's controls: one set per device and the device used (0x64 bytes). */
struct RacePlayerControls {
	RaceControls configs[3];  ///< keyboard, gamepad 1, gamepad 2
	int32 selected;           ///< the config in use (+0x60)
};

/** The race (0x184 bytes). */
struct RaceSettings {
	enum {
		kModeOnePlayer = 0,
		kModeDemo = 1,
		kModeTwoPlayers = 2
	};
	char track[32];           ///< +0x00 the track's directory (racing/tracks/<track>)
	char cars[4][32];         ///< +0x20 the cars' directories (racing/cars/<car>), the players' first
	int32 laps;               ///< +0xa0
	int32 carCount;           ///< +0xa4 1..4
	int32 mode;               ///< +0xa8 kMode*
	uint32 bestLap;           ///< +0xac the best lap (ms) of the track, set by the race
	int32 tricks;             ///< +0xb0 dirty tricks (pickups) on
	RacePlayerControls players[2]; ///< +0xb4 / +0x118
	uint32 raceKey;           ///< +0x17c the race time table: laps flag (0x4a0140) | track
	uint32 lapKey;            ///< +0x180 the lap time table: track | 0x80000000
};

/**
 * The default controls of a player for a device type (FUN_0047a980): the
 * arrow keys, Ctrl and Shift for player 0, T G F H A Z for player 1, the
 * gamepads' axes and buttons for the gamepad types.
 */
void raceDefaultControls(RaceControls &controls, int player, int type);

/**
 * The race menu's settings (DAT_006cfed0), kept across the scene changes
 * like the original's global.
 */
RaceSettings &raceMenuSettings();

/**
 * The high score entries of the last race of the menu's settings
 * (DAT_006d05c8..DAT_006d0634): the race menu shows them in bold when its
 * tables of that track (and laps) are opened.
 */
struct RaceScoreState {
	bool valid;               ///< DAT_006d05c8, set by the race (FUN_0047aa00)
	int track;                ///< DAT_006d05cc the track's resource number
	int laps;                 ///< DAT_006d05d0
	ScoreEntry raceEntry;     ///< DAT_006d05d4
	ScoreEntry lapEntry;      ///< DAT_006d05f8
	int raceRows[3];          ///< DAT_006d061c
	int lapRows[3];           ///< DAT_006d0628
};

RaceScoreState &raceScoreState();

} // End of namespace Flaaklypa

#endif
