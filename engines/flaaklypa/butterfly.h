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
#ifndef FLAAKLYPA_BUTTERFLY_H
#define FLAAKLYPA_BUTTERFLY_H

#include "common/rect.h"
#include "common/str.h"

#include "flaaklypa/font.h"
#include "flaaklypa/scene.h"

namespace Flaaklypa {

/**
 * "Sommerfugler i magen" (the butterfly sub game, started from the pee
 * page): butterflies of 15 species flutter across a meadow, wasps and bees
 * too. The mouse is a net; a click catches everything within 35 pixels of
 * the pointer. Butterflies score, insects cost points. Each level has a
 * time limit (the caterpillar timer on the right) and a score to reach.
 *
 * Mirrors the handler at 0x43a410 of FGP.exe.
 */
class ButterflyScene : public Scene {
public:
	ButterflyScene(FlaaklypaEngine *vm, const SceneDef *def);
	~ButterflyScene() override;

	bool load() override;
	void onInit(int arg) override;
	void onClose() override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onMouseMove(int hotspot, int x, int y) override;
	void onAnimFinished(Anim *anim) override;
	void onUpdate() override;

	/** Species caught in any game so far (the collection the album screen shows). */
	static bool isCollected(int species);

	enum {
		kSpeciesCount = 15,
		kEnemyCount = 2
	};

private:
	enum {
		kSlots = 30,              ///< butterflies and insects at a time, each
		kLevels = 11,
		kClipsPerSpecies = 8,     ///< clips every 45 degrees
		kClipsPerEnemy = 4,       ///< every 90 degrees
		kSpriteSize = 32,
		kCatchRadius = 35,
		kScareRadius = 150,
		kHotspotStart = 1,        ///< the looping start clip
		kHotspotHelp = 31,
		kHotspotExit = 32,
		kHotspotAlbum = 33,
		kZLevel = -200,
		kZButterfly = -100,
		kZEnemy = 0,
		kZBorder = 100,
		kZText = 110,
		kZButton = 150,
		kZCatcher = 250
	};

	/** One row of the species tables at 0x49e078 (butterflies) and 0x49e438 (insects). */
	struct SpeciesDef {
		int cls;                  ///< 0, 1 common (left window), 2 rare (right window), 3 insect
		const char *name;         ///< clip prefix and icon bitmap
		int points;
		int weight;               ///< spawn probability (the butterfly weights add up to 100)
		int params[2][6];         ///< per mode (0 normal, 1 scared): min/max ms between turns, min/max speed px/s, min/max turn degrees
	};

	/** The 0xd8 byte entity structs at 0x66a1a8 (butterflies) and 0x667e40 (insects). */
	struct Entity {
		bool active;
		bool enemy;
		int species;
		bool resting;             ///< wings frozen (+0xc)
		int mode;                 ///< +0x10
		uint32 nextChange;        ///< +0x14, time of the next turn
		float x, y;               ///< +0x18, +0x1c top left of the 32x32 sprite
		int heading;              ///< +0x20 degrees, 0 up, 90 right
		float dirX, dirY;         ///< +0x24, +0x28
		int speed;                ///< +0x2c pixels per second
		Anim *anim;               ///< +0x30 the clip for the current heading (owned)
	};

	static const SpeciesDef kSpecies[kSpeciesCount];
	static const SpeciesDef kEnemies[kEnemyCount];
	static const int kLevelTable[kLevels][2];     ///< duration ms, points needed
	static const float kLevelSpeed[kLevels];

	int randRange(int lo, int hi);
	static int octantClip(int heading);
	static int quadrantClip(int heading);
	const SpeciesDef &speciesOf(const Entity &e) const;
	const AnimDef *clipDef(const Entity &e, int clip) const;

	// entities
	void resetEntities();
	void setClip(Entity &e, int heading);
	void setPlaying(Entity &e, bool rest);
	bool shouldRest(const Entity &e);
	void turn(Entity &e);
	void setMode(Entity &e, int mode);
	void spawn(Entity &e, bool enemy, int species);
	void removeEntity(Entity &e);
	void removeAll(Entity *list);
	int freeSlot(const Entity *list) const;
	int pickSpecies();
	void spawnButterfly();
	void spawnEnemy();
	void moveEntity(Entity &e, uint32 now, uint32 last);
	bool within(const Entity &e, int x, int y, int radius) const;
	void addPoints(const Entity &e);
	void catchAt(int x, int y);

	// screen
	void initScreen();
	void startGame();
	void showCatcher(bool show);
	void setLevel(int level);
	void restartTimer();
	bool updateTimer(uint32 now);
	void timeUp();
	void gameOver();
	void updateIcons();
	void drawNumber(const char *area, int value);
	void drawLabel(const char *area, const Common::String &text);
	void highlightButton(int hotspot);

	BitmapFont _font10, _font18;
	Entity _butterflies[kSlots];
	Entity _enemies[kSlots];
	int _caught[kSpeciesCount];
	AnimDef _clipDefs[kSpeciesCount * kClipsPerSpecies + kEnemyCount * kClipsPerEnemy];
	Common::String _clipNames[kSpeciesCount * kClipsPerSpecies + kEnemyCount * kClipsPerEnemy];
	CursorEntry _blankCursors[257];

	bool _inGame;               ///< the timer runs (timermove is on screen)
	int _levelPoints;           ///< 0x667e28
	int _score;                 ///< 0x667e2c
	int _level;                 ///< 0x667e30
	uint32 _levelStart;         ///< 0x667e34
	int _tick;                  ///< 0x667e38
	uint32 _lastTick;           ///< 0x667d78
	int _highlighted;
	Common::Point _lastMouse;
};

/**
 * "Sommerfugl kolleksjon" (buttercol, handler 0x43a230): the album of the
 * butterfly game. The caught species are shown in colour over the black and
 * white drawings of the backdrop.
 */
class ButtercolScene : public Scene {
public:
	ButtercolScene(FlaaklypaEngine *vm, const SceneDef *def);

	void onInit(int arg) override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onMouseMove(int hotspot, int x, int y) override;

private:
	enum {
		kHotspotHelp = 31,
		kHotspotExit = 32,
		kHotspotFirst = 20        ///< species i has hotspot 20 + i
	};
	static const char *const kCollection[];
	void placeCollection();
	void highlightButton(int hotspot);
	/** The cheat code LOVELYFLIES (0x43a390): the album shows every species. */
	void cheatLovelyFlies();
	int _highlighted;
	bool _showAll;  ///< DAT_00666c60 all set (the profile keeps its own)
};

} // End of namespace Flaaklypa

#endif
