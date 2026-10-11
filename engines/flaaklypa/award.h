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
#ifndef FLAAKLYPA_AWARD_H
#define FLAAKLYPA_AWARD_H

#include "common/array.h"
#include "common/str.h"

#include "flaaklypa/profile.h"

namespace Flaaklypa {

class FlaaklypaEngine;

/*
 * The AWARD module of the original (FUN_0041b3d0..FUN_0041c460): the items
 * a player wins, kept in the active profile's award counters (AwardType in
 * profile.h), and the award dialog that announces them.
 *
 * Without an active profile (FlaaklypaEngine::profile() is nullptr) the
 * counters read as 0 and changes are lost, like in the original; the award
 * dialog still comes.
 */

/** Award categories besides the profile's counters (AwardType). */
enum {
	/**
	 * A scene of the 3D viewer (the hidden 3D glasses of the story pages):
	 * bit n of the "anaglyph" scene's profile value 100, not a counter.
	 */
	kAward3DScene = 8,
	/** The 15 3D images of the gallery, always available (no counter). */
	kAward3DImage = 9,
	kAwardCategories = 10
};

/** The flags of showAward() and awardGive(). */
enum {
	/** flags & 7: the medal won, 1 bronze, 2 silver, 3 gold, 4 platinum. */
	kAwardMedalMask = 7,
	/** The count is the number of the item to award, not a number of items. */
	kAwardSpecificItem = 0x10
};

// ---- Items and counters ----------------------------------------------------

/** The number of items of a category (FUN_0041b680, table 0x49ca88). */
int awardItemCount(int type);
/** A counter of the active profile's award chunk, 0 without a profile (FUN_0041b540). */
int32 awardCounter(FlaaklypaEngine *vm, int type);
/**
 * Whether item n (0 based) of a category is won with the given counter
 * value (FUN_0041b5d0): car parts and bonus points below the count, the
 * others by their bit; some items are always available (table 0x49ca60).
 */
bool awardIsWon(int type, int32 counter, int item);
/** awardIsWon() with the active profile's counter. */
bool awardHasItem(FlaaklypaEngine *vm, int type, int item);
/** The number of items of a category won with the given counter value (FUN_0041b690). */
int awardWonCount(int type, int32 counter);
/** The counter value with item n added (FUN_0041b6e0). */
int32 awardAddItem(int type, int32 counter, int item);
/**
 * Awards items of a category to the active profile (FUN_0041b740): the
 * next count items not won yet, or with kAwardSpecificItem the item count
 * itself (then always reports one item). A car also unlocks its gallery
 * image (image car + 18). Returns the number of items awarded and appends
 * their numbers to items. Calls profileChanged() when the profile changed.
 */
int awardGive(FlaaklypaEngine *vm, int type, int count, int flags = 0, Common::Array<int> *items = nullptr);
/** The language.ini name of item n of a category, awarddlg:NONE when out of range (FUN_0041b580). */
Common::String awardItemName(FlaaklypaEngine *vm, int type, int item);
/**
 * The language.ini and bitmap number (0 based) of butterfly n, the bit of
 * the butterfly counter (FUN_0041b3d0, table 0x49cab0); -1 when out of range.
 */
int awardButterflyIndex(int butterfly);
/** Whether the hidden 3D glasses of 3D viewer scene n have been found (FUN_0041b8c0). */
bool award3DSceneFound(FlaaklypaEngine *vm, int scene);

// ---- The image gallery's 79 items (activity, jigsaw, sliding) --------------

/** The number of gallery items (FUN_0041b4e0): 79. */
int galleryItemCount();
/**
 * The gallery items of a category (FUN_0041b4f0): images 0..30, cars
 * 31..40, tracks 41..48, butterflies 49..63, 3D images 64..78. Sets the
 * first item and the end (exclusive) and returns their number; 0 for the
 * other categories.
 */
int galleryRange(int type, int *start, int *end);
/** Whether gallery item n is available to the active profile (FUN_0041b3f0). */
bool galleryItemAvailable(FlaaklypaEngine *vm, int item);
/** The language.ini name of gallery item n (FUN_0041b460), "" when out of range. */
Common::String galleryItemName(FlaaklypaEngine *vm, int item);
/** The bitmap name of gallery item n, e.g. "car_03" (FUN_0041b4a0), "" when out of range. */
Common::String galleryItemBitmap(int item);

// ---- The award dialog ------------------------------------------------------

/**
 * Awards items and announces them in the award dialog (FUN_0041bb20):
 * awards count items of a category (see awardGive(); car parts beyond the
 * 62 become bonus points, which may award other items instead, and a 3D
 * scene is awarded once) and shows the dialog with the title, the text, the
 * medal of flags & 7 and the items won. Nothing is shown when nothing was
 * won and there is no medal. Title and text are language.ini keys or plain
 * text; empty ones get the defaults of the category and the medal. Returns
 * true when the dialog was shown.
 */
bool showAward(FlaaklypaEngine *vm, int type, const Common::String &title, const Common::String &text, int count, int flags = 0);
/** Car parts for a won game of a difficulty, 0 easy, 1 medium, 2 hard (FUN_0041c0b0: chess, draughts). */
void awardDifficulty(FlaaklypaEngine *vm, int level, int count);
/** A car part for finding all fact pages of a story page (FUN_0041c120). */
void awardFactsFound(FlaaklypaEngine *vm);
/** A car part for the hidden car part of a page or game (FUN_0041c150). */
void awardHiddenCarPart(FlaaklypaEngine *vm);
/** A car part for solved jigsaw / sliding puzzles (FUN_0041c180). */
void awardJigsaw(FlaaklypaEngine *vm);
/** The 3D viewer scene of hidden 3D glasses, once (FUN_0041c1b0). */
void award3DScene(FlaaklypaEngine *vm, int scene);
/**
 * A car part after three hours of play, once per profile (FUN_0040d4c0,
 * profile value "timeaward" 0): call when the active profile has been in
 * use for 10800 seconds and no sub game round is running.
 */
void awardPlayTime(FlaaklypaEngine *vm);

/**
 * Development aid behind the "test_award" config key: "<kind>[,<count>[,<flags>]]"
 * with kind a category number (showAward() with the default texts, count
 * items, flags = medal | kAwardSpecificItem) or fact, hidden, jigsaw, time,
 * 3dscene (count = scene) and easy, medium, hard (awardDifficulty()). Uses
 * a temporary profile when none is active, preset from "test_award_counters"
 * (eight numbers) and "test_award_anaglyph", and logs the counters.
 */
void testAward(FlaaklypaEngine *vm, const Common::String &spec);

} // End of namespace Flaaklypa

#endif
