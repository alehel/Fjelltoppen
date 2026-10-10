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
#ifndef FLAAKLYPA_WHACKAMOLE_H
#define FLAAKLYPA_WHACKAMOLE_H

#include "common/str.h"

#include "flaaklypa/font.h"
#include "flaaklypa/scene.h"

namespace Flaaklypa {

/**
 * "Dra meg baklengs!" (the whackamole sub game of add-on set 2): birds peek
 * out of seven bird houses and five nests in an old tree and want to be fed
 * a larva (the mouse cursor) before they disappear again; a squirrel shows
 * up now and then and must not be fed. Each level lasts a fixed time; a
 * feeding average of 70 % or more takes the player to the next level.
 *
 * Mirrors the handler at 0x44d980 of the original.
 */
class WhackamoleScene : public Scene {
public:
	WhackamoleScene(FlaaklypaEngine *vm, const SceneDef *def);
	~WhackamoleScene() override;

	bool load() override;
	void onInit(int arg) override;
	void onClose() override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onMouseMove(int hotspot, int x, int y) override;
	void onAnimFinished(Anim *anim) override;
	void onTimer(int id, int data) override;
	void onUpdate() override;

private:
	enum {
		kHoles = 12,            ///< bird houses 1..7, nests 1..5 (hotspots 1..12)
		kLevelCount = 18,
		kHotspotExit = 32,
		kHotspotHelp = 33,
		kHotspotStart = 34,
		kZClips = 10,
		kZText = 11,
		kZButtons = 200,
		kZCursor = 250,
		kTimerCursor = 0,
		kPointsPerBird = 13,
		kPointsPerPercent = 29,
		kCursorHideTime = 250,  ///< ms the larva is gone after a feeding
		kCursorHideLimit = 15000 ///< from this many points on the larva stays
	};

	/** The clip sets of each hole, in the order of the original's arrays. */
	enum Kind {
		kBirdA, kBirdB1, kBirdB2, kBirdB3, kBirdC,          // bird comes out, waits (random b), goes back in
		kBirdAR, kBirdB1R, kBirdB2R, kBirdB3R, kBirdCR,     // the same, mirrored bird
		kSquirrelA, kSquirrelB1, kSquirrelC,
		kBirdSound,                                         // looping chirp while a bird waits ("...b1.s")
		kKindCount
	};

	struct LevelDef {
		int birds;              ///< birds shown during the level
		int squirrels;          ///< squirrels shown during the level
		int waitRange;          ///< a bird waits rand(waitRange) + 1 clips
		int unused;             ///< not read by the handler
		int duration;           ///< ms
	};

	Anim *clip(Kind kind, int hole) const { return _clips[kind][hole]; }
	int holeOf(Anim *a, Kind kind) const;
	Anim *squirrelAt(int hole) const;
	Anim *birdAt(int hole) const;
	Anim *mirroredBirdAt(int hole) const;
	bool holeFree(int hole) const;
	int pickFreeHole() const;
	void startClip(Kind kind, int hole);
	void removeAllClips();
	void playSound(const char *name);
	int rnd(int n) const;

	void startGame();
	void startLevel();
	void endLevel();
	bool spawnBird();
	bool spawnSquirrel();
	void hideLarva();
	float percent() const;

	void drawText(const char *area, const Common::String &text);
	void drawPoints();
	void drawPercent();
	void drawTime();
	void highlightButton(int hotspot);

	static const LevelDef kLevels[kLevelCount];
	static const char *const kHoleNames[kHoles];
	static const bool kHoleHasLst[kHoles];
	static const CursorEntry kNoCursor[];

	BitmapFont _font;
	Anim *_clips[kKindCount][kHoles];
	Common::String _names[kKindCount][kHoles];
	Common::String _timeText;
	int _highlighted;

	// game state, see the globals listed in NOTES.md
	bool _running;
	bool _messageBox;          ///< the next level box is open (see endLevel())
	bool _busy[kHoles];         ///< a fed bird / squirrel is still going back in
	int _waits[kHoles];         ///< remaining waiting clips of the animal in the hole
	int _balance;               ///< fed birds minus fed squirrels (not displayed)
	int _appeared;              ///< birds shown plus squirrels fed
	int _fed;
	int _points;
	int _level;
	int _duration;
	uint32 _levelStart;
	const LevelDef *_levelDef;
	int _birdInterval, _squirrelInterval;
	int _birds, _squirrels;
	uint32 _nextBird, _nextSquirrel;
	bool _cursorHidden;
};

} // End of namespace Flaaklypa

#endif
