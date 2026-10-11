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
#ifndef FLAAKLYPA_BEEMAZE_H
#define FLAAKLYPA_BEEMAZE_H

#include "common/array.h"
#include "common/list.h"
#include "common/rect.h"
#include "common/str.h"

#include "flaaklypa/font.h"
#include "flaaklypa/scene.h"

namespace Flaaklypa {

/**
 * "Ludvigs Labyrint" (the beemaze sub game): a Pac-Man style maze game.
 * Ludvig walks through a 22x16 cell maze with the arrow keys and picks up
 * the sticks, nuts, berries and mushrooms. Bees roam the maze; a smoke
 * puffer protects him for a while and puts bees to sleep, a beehive carried
 * back to the start square gives bonus points but makes the bees chase him.
 * Five mazes, cycled with more bees and hives every round.
 *
 * Mirrors the BEEMAZE module of the original (handler 0x42f1b0).
 */
class BeemazeScene : public Scene {
public:
	BeemazeScene(FlaaklypaEngine *vm, const SceneDef *def);
	~BeemazeScene() override;

	bool load() override;
	void onInit(int arg) override;
	void onClose() override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onMouseMove(int hotspot, int x, int y) override;
	void onKey(const Common::KeyState &key) override;
	void onKeyUp(const Common::KeyState &key) override;
	void onAnimFinished(Anim *anim) override;
	void onTimer(int id, int data) override;
	void onUpdate() override;

private:
	enum {
		kCols = 22,
		kRows = 16,
		kCells = kCols * kRows,
		kCellSize = 32,
		kLevels = 5,
		kMazeX = 48,            ///< screen position of cell (0, 0)
		kMazeY = 74,
		kMazeWidth = kCols * kCellSize,
		kMazeHeight = kRows * kCellSize,
		kMaxBees = 7,
		kMaxLives = 3,
		kMaxTrail = 25,
		kSpriteWidth = 40,
		kSpriteHeight = 52,
		kWalkFrames = 15,
		kHotspotStart = 101,
		kHotspotHelp = 31,
		kHotspotExit = 32
	};

	enum Dir {
		kDirNone = 0,
		kDirRight = 1,
		kDirDown = 2,
		kDirLeft = 3,
		kDirUp = 4
	};

	/** Cell types of the maze tables. */
	enum Cell {
		kCellStart = 1,         ///< the 2x2 start square (drops the hive)
		kCellPath = 2,          ///< corridor; items are placed here
		kCellHive = 3,          ///< beehive site
		kCellWall = 4,
		kCellBeeZone = 5,       ///< passable for bees only
		kCellOpen = 6           ///< corridor without items; at the border a tunnel to the other side
	};

	enum Flags {
		kFlagSmoke = 1,         ///< Ludvig is protected by a smoke cloud
		kFlagMushroom = 2,      ///< mushroom speed boost
		kFlagHoney = 4,         ///< carrying a hive
		kFlagFlicker = 8        ///< the smoke cloud is about to vanish
	};

	enum Timers {
		kTimerStart = 1,
		kTimerMushroom,
		kTimerSmoke,
		kTimerFlicker,
		kTimerBee0              ///< + bee index
	};

	enum BeeMode {
		kBeeRandom,
		kBeePanic,
		kBeeTrail,
		kBeeChase
	};

	struct AutoKey {
		uint32 time;
		int dir;                ///< kDirNone releases
	};

	// owned animation objects (the original has its own structs for these)
	Anim *makeAnim(const char *name, bool smacker, bool visible, bool transparent, bool loop);
	void dropAnim(Anim *&a);
	Anim *soundClip(const char *name);
	void playSound(const char *name, int variants);

	// game flow
	void startGame();
	void startLevel();
	void restartLevel();
	void loseLife();
	void gameOver();
	void killGameTimers();
	void drawScore();
	void drawTitle();
	void highlightButton(int hotspot);

	// level building
	void placePuffers();
	void placeItems();
	void placeItem(int x, int y);
	void placeHives();
	void placeBees();
	void startLevelClips();
	void removeLevelClips();
	void playAmbientClip();
	void clearItems();
	void clearBees();
	Anim *setItem(int x, int y, const char *name);
	Anim *item(int x, int y) const { return _items[y * kCols + x]; }
	bool itemIs(int x, int y, const char *name) const;

	// maze
	int cellType(int x, int y) const { return kMazes[_level % kLevels][y][x]; }
	bool playerPassable(int x, int y) const;
	bool beePassable(int x, int y) const;
	bool tunnel(int x, int y, int dir) const;
	void pushTrail(int x, int y);
	bool onTrail(int x, int y) const;

	// Ludvig
	void movePlayer();
	void stepPlayer(int step);
	void pickup(int x, int y);
	void drawPlayerFrame();
	void updatePlayerAnim();
	// the cheat codes of the init (FUN_0040ad50)
	void cheatShrooms();
	void cheatVoja();
	void updateSmokePos();

	// bees
	void moveBees();
	void moveBee(int i, int step, BeeMode mode);
	int randomDir(int i);
	int panicDir(int i);
	int trailDir(int x, int y);
	int chaseDir(int i);
	void smokeBees();
	bool beeHit() const;

	// development aid
	void parseAutoKeys();
	void processAutoKeys();
	void setWanted(int dir);
	void releaseWanted(int dir);

	static const byte kMazes[kLevels][kRows][kCols];
	static const int kArrowPos[kLevels][2];
	static const AnimList kLevelClips[kLevels];

	// owned animations
	Common::List<Common::String> _ownNames;
	Common::List<AnimDef> _ownDefs;
	Common::Array<Anim *> _ownAnims;
	Anim *_beeAnims[kMaxBees];
	Anim *_beeSleepAnims[kMaxBees];
	Anim *_lifeAnims[kMaxLives];
	Anim *_levelAnim;
	Anim *_player;
	Anim *_items[kCells];
	Common::Array<Anim *> _sounds;

	// sprite sheets
	Graphics::ManagedSurface *_walkCycle;
	Graphics::ManagedSurface *_honeyCycle;
	Graphics::ManagedSurface *_smokeCycle;
	Graphics::ManagedSurface *_honeySmokeCycle;
	uint32 _keyColor;
	BitmapFont _font10, _font14;

	// game state
	bool _running;              ///< DAT_006584bc
	int _level;                 ///< DAT_006598a0, 0 based; maze = level % 5
	int _lives;                 ///< DAT_00659668
	int _score;                 ///< DAT_006584c0
	int _shownScore;            ///< DAT_006584e8
	uint32 _flags;              ///< DAT_00658504
	Common::Point _ludvigCentre; ///< DAT_00658570: the middle of Ludvig's element, for the VOJA cheat
	bool _advance;              ///< DAT_00648d64: next restart goes to the next level
	int _sticks, _berries, _nuts, _mushrooms, _hives;
	int _highlighted;

	// Ludvig
	int _wanted;                ///< DAT_00658588 direction asked for with the keys
	int _facing;                ///< DAT_00658500
	int _px, _py;               ///< DAT_006584f8/fc position in maze pixels
	int _tx, _ty;               ///< DAT_00658580/84 cell he walks to
	int _steps;                 ///< DAT_00648c98 movement counter driving the walk cycle
	int _lastSteps;             ///< DAT_006598b0
	int _walkFrame;             ///< DAT_006598b8
	uint32 _moveTime;           ///< DAT_006598ac
	uint32 _frameTime;          ///< DAT_006598b4
	int _smokeX, _smokeY;       ///< DAT_00658560/64
	int _flickerCount;          ///< DAT_006598bc

	// bees
	int _beeCount;              ///< DAT_006598a4
	int _beeX[kMaxBees], _beeY[kMaxBees];     ///< DAT_00658510
	int _beeCX[kMaxBees], _beeCY[kMaxBees];   ///< DAT_00648c60 cell the bee flies to
	int _beeDir[kMaxBees];      ///< DAT_004cd780 (never reset in the original)
	bool _beeSleeping[kMaxBees];///< DAT_006584c8
	uint32 _beeTime;            ///< DAT_006598a8
	bool _beeTimeReset;         ///< DAT_006584b8
	Common::Array<Common::Point> _trail;      ///< DAT_006584b0 ring buffer of Ludvig's last cells
	int _trailWrite;            ///< DAT_00648d60
	Common::Array<Common::Point> _hiveCells;  ///< DAT_00659878
	uint32 _ambientTime;        ///< DAT_00658558

	// development aid
	Common::Array<AutoKey> _autoKeys;
	uint32 _initTime;
};

} // End of namespace Flaaklypa

#endif
