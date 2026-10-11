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
#ifndef FLAAKLYPA_HISCORE_H
#define FLAAKLYPA_HISCORE_H

#include "common/array.h"
#include "common/str.h"

namespace Flaaklypa {

class FlaaklypaEngine;
class Profile;

/*
 * The HISCORE module of the original (FUN_0041e8e0..FUN_0041f9d0): the high
 * score tables of the sub games, kept in the player profiles (record 0x401
 * of a scene, keyed by a level or variant, 0 for the sub games), the medal
 * a table holds, and the "highscore" dialog every sub game opens at the end
 * of a round.
 *
 * Each table holds the ten best entries {player, date, score}, best first.
 * The dialog shows three of them: the player's own table, the "global" one
 * (the ten best entries of all players) and the "union" one (the best entry
 * of each player).
 */

/** The flags of a table: how scores are ranked and shown. */
enum {
	kScoreFormatMask = 0xff,
	kScorePoints = 0,          ///< shown as "%d"
	kScoreHMS = 1,             ///< milliseconds shown as hh:mm:ss
	kScoreMSC = 2,             ///< milliseconds shown as mm:ss.cc
	kScoreOrderMask = 0xff00,
	kScoreLowerBetter = 0x100, ///< ranked lowest first (times)
	/** The time tables of the original (jigsaw, sliding, race, balloonhunt). */
	kScoreTime = kScoreLowerBetter | kScoreMSC
};

enum {
	kScoreTableSize = 10       ///< entries per table
};

/** An entry of a high score table (0x24 bytes in the original). */
struct ScoreEntry {
	Common::String name;   ///< the player (the profile's name)
	uint32 date;           ///< when it was made, a profile time stamp (FlaaklypaEngine::profileTime())
	int32 score;

	ScoreEntry() : date(0), score(0) {}
};

/** A high score table (0x174 bytes in the original). */
struct ScoreTable {
	int32 medal;                        ///< the best medal won, 0 none, 1 bronze .. 4 platinum
	Common::Array<ScoreEntry> entries;  ///< at most kScoreTableSize, best first
	/**
	 * The row shown in the bold font (+0x170): the entry just added, -1 when
	 * it did not make the table. Tables read from the profiles start at 0
	 * like in the original, so the first row is bold unless it is set.
	 */
	int highlight;

	ScoreTable() : medal(0), highlight(0) {}
	void clear() { medal = 0; entries.clear(); highlight = 0; }

	/**
	 * Inserts an entry before the first one it beats (FUN_0041e9c0); a full
	 * table drops its last entry. Returns the row or -1 when the entry did
	 * not make the table.
	 */
	int insert(const ScoreEntry &entry, int flags);
	/**
	 * Inserts an entry keeping one entry per player (FUN_0041ea50): the
	 * player's old entry is replaced when the new one beats it, else
	 * nothing happens (-1).
	 */
	int insertBest(const ScoreEntry &entry, int flags);
};

// ---- helpers ---------------------------------------------------------------

/** > 0 when score a beats b (FUN_0041e930): a - b, or b - a for kScoreLowerBetter. */
int32 compareScores(int32 a, int32 b, int flags);
/**
 * The medal of a score (FUN_0041efe0): medals holds the four thresholds
 * {bronze, silver, gold, platinum}; the best one reached (or not beaten)
 * counts. 0 without a table.
 */
int scoreMedal(int32 score, const int *medals, int flags);
/** The date of an entry, "dd.mm.yy" (FUN_0041ed50). */
Common::String formatScoreDate(uint32 time);
/** A score as the tables show it (FUN_0041ed90), see kScorePoints / kScoreHMS / kScoreMSC. */
Common::String formatScore(int32 value, int flags);

// ---- the tables in the profiles --------------------------------------------

/**
 * Reads a table from a profile (FUN_004115c0 / FUN_00411620): the 0x58
 * byte record 0x401 of the scene with the key, every entry named after the
 * profile. False (and an empty table) when there is none.
 */
bool readScoreTable(const Profile &profile, const Common::String &scene, uint32 key, ScoreTable &table);
/** The active profile's medal for a scene's table (first int of the record), 0 without one. */
int scoreTableMedal(FlaaklypaEngine *vm, const Common::String &scene, uint32 key = 0);
/**
 * The lowest medal of all sub games (FUN_0040d9b0): the key 0 tables of the
 * scenes of type 3 in the active profile, 0 without a profile.
 */
int lowestSubGameMedal(FlaaklypaEngine *vm);
/**
 * Reads the three tables of a scene (FUN_0041ebd0); each may be nullptr.
 * profile: the active profile's table (empty without a profile). global:
 * the ten best entries of the tables of all profiles (FUN_00411d40, at
 * most 64 tables, players in name order). best: the best entry of each
 * player ("union"). All highlights are 0.
 */
void loadScoreTables(FlaaklypaEngine *vm, const Common::String &scene, uint32 key,
                     ScoreTable *profile, ScoreTable *global, ScoreTable *best, int flags);

// ---- registration and the dialog -------------------------------------------

/**
 * Enters a score into the current scene's tables without the dialog
 * (FUN_0041ee50; jigsaw, sliding, mahjong and the race use it directly):
 * the entry gets the player's name (profile:NONAME without a profile) and
 * the time, goes into the player's table, the global and the union table,
 * and the player's table is stored (FUN_00411c70) when the entry made it.
 * A better medal (from the medal thresholds, see scoreMedal(); medals may
 * be nullptr) shows the award dialog with the medal and one car part per
 * medal step (none with noCarParts), and when that raised the lowest medal
 * over all sub games, a second award dialog gives one car part per step.
 * Without a profile nothing is read or written (the tables hold just the
 * new entry) but the medal award still shows. entry and tables (three:
 * player, global, union, with their highlights set) receive the result
 * when given. Returns the entry's row in the player's table, -1 when it
 * did not make it.
 */
int addScore(FlaaklypaEngine *vm, uint32 key, int32 score, const int *medals, int flags,
             ScoreEntry *entry = nullptr, ScoreTable *tables = nullptr, bool noCarParts = false);

/**
 * The high score registration the sub games call at the end of a round
 * (FUN_0041f9a0 with key 0 and flags 0, FUN_0041f8c0 in general): enters
 * the score (addScore(): medal awards included) and shows the "highscore"
 * dialog with the player's, the global and the union table and the new
 * entry, with the scene frozen; returns when it is closed.
 *
 * title: language.ini key or text over the tables; empty = the current
 * scene's title (FUN_0040ce70). score: the round's score (milliseconds
 * for the time formats). medals: the four medal thresholds of the game
 * {bronze, silver, gold, platinum}, nullptr for none. key: the table of the
 * scene (a level or track; 0 for every caller of FUN_0041f9a0). flags:
 * kScorePoints for the sub games, kScoreTime for times. The original's
 * last argument, a callback run after the dialog, is the caller's code
 * after this call. Returns the entry's row in the player's table or -1.
 *
 * TODO: tournament mode (FUN_00419490) is not ported: there the score goes
 * to the tournament's table (FUN_004196c0), the next game of the
 * tournament starts (FUN_00419740) and neither the dialog nor the callback
 * come.
 */
int registerScore(FlaaklypaEngine *vm, const Common::String &title, int32 score, const int *medals,
                  uint32 key = 0, int flags = kScorePoints);
/**
 * FUN_0041f9d0 (gametrivia, mathlab, synonym): registerScore() with key 0
 * whose medal award brings no car parts (DAT_005a6fe0); the rise of the
 * lowest medal still gives them.
 */
int registerScoreNoCarParts(FlaaklypaEngine *vm, const Common::String &title, int32 score, const int *medals);

/**
 * Shows the current scene's tables without entering a score (FUN_0041f640
 * / FUN_0041f030; jigsaw, sliding, mahjong, anaglyph and racemenu after
 * addScore()): entry (may be nullptr) is shown on the line above the
 * table, highlights (three rows, may be nullptr) mark the bold rows.
 * scene empty = the current one (racemenu passes "racing").
 */
void showScoreTables(FlaaklypaEngine *vm, const Common::String &title, uint32 key, const ScoreEntry *entry,
                     const int *highlights, int flags, const Common::String &scene = "");

/**
 * Development aid behind the "test_highscore" config key:
 * "<scene>,<score>[,<flags>[,<key>]]" registers a score in the scene's
 * table like the game does (its medal table, its title) and shows the
 * dialog. "test_highscore_medal=<n>" first raises the medal of every other
 * sub game's table to n (to see the lowest medal rise).
 */
void testHighscore(FlaaklypaEngine *vm, const Common::String &spec);

} // End of namespace Flaaklypa

#endif
