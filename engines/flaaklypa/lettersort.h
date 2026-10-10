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
#ifndef FLAAKLYPA_LETTERSORT_H
#define FLAAKLYPA_LETTERSORT_H

#include "common/array.h"
#include "common/rect.h"
#include "common/str.h"

#include "flaaklypa/font.h"
#include "flaaklypa/scene.h"

namespace Flaaklypa {

/**
 * "Postsorteringsmaskinen" (the lettersort sub game of the yard): Solan
 * delivers letters through the post office window, each with a country's
 * flag as its stamp. The player drags the letters from the floor to the
 * mail bags on the conveyor belt above; a bag with the same flag takes
 * four letters, then Reodor's claw lifts it off the belt. A steam pusher
 * shoves every new bag onto the belt from the left; when the chain of bags
 * gets so long that the first one falls off the right end, the alarm bell
 * rings and the game is over.
 *
 * Scene handler at 0x43db30 in the executable; the module's functions are
 * at 0x43dc30..0x43fd90.
 */
class LettersortScene : public Scene {
public:
	LettersortScene(FlaaklypaEngine *vm, const SceneDef *def);
	~LettersortScene() override;

	bool load() override;
	void onInit(int arg) override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onMouseUp(int hotspot, int x, int y) override;
	void onMouseMove(int hotspot, int x, int y) override;
	void onAnimFinished(Anim *anim) override;
	void onUpdate() override;

private:
	enum {
		kMaxLetters = 30,
		kMaxBags = 10,
		kMaxClaws = 9,
		kMaxCountries = 12,
		kLevels = 10,
		kHotspotStart = 1,
		kHotspotExit = 2,
		kHotspotHelp = 3,
		kHotspotLetter = 100          ///< + letter index
	};

	/**
	 * An animation structure the original fills in by sprintf: the name is
	 * rewritten while the element is off screen, as the game swaps bitmaps
	 * and clips of its letters, bags and claws.
	 */
	struct Sprite {
		AnimDef def;
		char name[64];
		Anim *anim;
		Sprite();
	};

	struct Letter {                   ///< 0xc0 byte structs at 0x6745b0
		bool active;
		int x, y;
		int country;
		int variant;                  ///< 0..11 picture of the letter
		bool gold;
		Sprite sprite;
	};

	struct Bag {                      ///< 0x168 byte structs at 0x672578
		bool active;
		int x, y;
		int country;
		int fill;
		bool gold;
		Sprite bag;
		Sprite flag;
	};

	struct Claw {                     ///< 0xc0 byte structs at 0x671eb0
		bool active;
		int x, y;
		int bag;
		int state;                    ///< 0 grabbing, 1 retrying, 2 lifting
		bool gold;
		Sprite sprite;
	};

	struct LevelDef {
		int bags;                     ///< bags to lift to finish the level
		int bagInterval;              ///< seconds between new bags
		int letterInterval;           ///< seconds between new letters
		int countries;                ///< countries in play
	};

	static const LevelDef kLevels_[kLevels];

	// sprites
	void defineSprite(Sprite &s, bool smacker, bool visible, bool transparent, bool loop, int hotspot, int x, int y, int z);
	void nameSprite(Sprite &s, const Common::String &name);
	void removeSprite(Sprite &s);
	void playSound(const char *name, int variants);
	const char *countryName(int country) const;

	// game flow
	void loadCountries();
	void resetGame();
	void startGame();
	void setLevel(int level);
	void playSolanPost();
	void playSolanIdle();
	void showStamp(int level);
	void addScore(int n);
	void drawScore();
	void secondTick(int sec);
	void bellTick();
	void gameOverDone();
	bool levelComplete() const;

	// letters
	int freeLetter() const;
	void spawnLetter();
	void dropLetters();
	void randomLetterPos(int &x, int &y);
	void clampLetterPos(int &x, int &y);
	int letterHistogram(int *hist) const;
	int pickLetterCountry();
	int pickBagCountry();
	int pickBelow(int limit, const int *hist, int n);
	int pickAbove(int limit, const int *hist, int n);
	void letterBitmapName(Letter &l);
	void showLetter(Letter &l);
	void removeLetter(int i);
	void pickupLetter(int i, int x, int y);
	void dropLetter(int i, int x, int y);
	void placeLetter(int i, int x, int y);
	void returnLetter(int i);

	// bags
	int freeBag() const;
	void spawnBag();
	void removeBag(int b);
	int bagAt(int x) const;
	void sortedBags(Common::Array<int> &bags) const;
	Common::Rect bagRect(int b) const;
	void setBagPos(int b, int x, int y);
	void bagBitmapName(Bag &bag);
	int fillBag(int b, int n);
	bool letterMatches(int i, int b) const;
	void queueClaw(int b);
	void queueAllBags();
	bool clawHolds(int b) const;

	// pusher and falling bags
	void startPusher();
	bool updatePusher();
	void pushBags();
	int fallingBag() const;
	void dropBag(int b);

	// claws
	int freeClaw() const;
	void startClaw(int b);
	void clawClipName(Claw &c);
	void clawFinished(int i);
	void removeClaw(int i);

	void highlightButton(int hotspot);

	Letter _letters[kMaxLetters];
	Bag _bags[kMaxBags];
	Claw _claws[kMaxClaws];
	Sprite _solanPost;                ///< 0x4db8e0
	Sprite _solanIdle;                ///< 0x4db838
	Sprite _bagout;                   ///< 0x4db640
	Sprite _stamp;                    ///< 0x4db988
	Common::Array<Sprite *> _sounds;  ///< clones of the sound effect struct 0x4dbc28

	Common::String _countries[kMaxCountries];
	int _countryCount;
	Common::Array<int> _clawQueue;    ///< full bags waiting for the claw, 0x67338c

	BitmapFont _font18, _font10;

	bool _running;                    ///< 0x676c98
	int _score;                       ///< 0x676c9c
	int _level;                       ///< 0x676ca0
	int _lettersSpawned;              ///< 0x676ca4
	int _bagsSpawned;                 ///< 0x676ca8
	int _lettersSorted;               ///< 0x676cac
	int _bagsCleared;                 ///< 0x676cb0
	int _capacity;                    ///< 0x673388, letters per bag
	int _bagZ;                        ///< 0x672574
	int _letterZ;                     ///< 0x676d60, z of the next letter put down
	int _bellCount;                   ///< 0x675c30, seconds of alarm left
	int _postX, _postY;               ///< where the current Solan delivery clip plays

	bool _pusherActive;               ///< 0x673410
	int _pusherX, _pusherY;           ///< 0x673414, 0x673418
	int _pusherPhase;                 ///< 0x67341c
	uint32 _pusherStart;              ///< 0x673420

	uint32 _startTime;
	int _lastSecond;                  ///< 0x672570
	int _dragLetter;                  ///< letter being dragged, -1 none
	int _dragOffX, _dragOffY;
	int _highlighted;
};

} // End of namespace Flaaklypa

#endif
