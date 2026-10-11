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
#ifndef FLAAKLYPA_TEXTINVADER_H
#define FLAAKLYPA_TEXTINVADER_H

#include "common/array.h"
#include "common/rect.h"
#include "common/str.h"

#include "flaaklypa/font.h"
#include "flaaklypa/scene.h"

namespace Flaaklypa {

/**
 * "Ordspillet" (textinvader, add-on set 2): a typing game. Words fall
 * from the night sky over Flåklypa; the player types them before they
 * reach the ground. Correct letters score, finished words score more,
 * mistakes and lost words cost energy (the chain of lights at the
 * bottom). Yellow words clear the sky, orange words slow everything
 * down for a while and red words give energy back.
 *
 * Handler 0x44c640 of the original; see NOTES.md.
 */
class TextinvaderScene : public Scene {
public:
	TextinvaderScene(FlaaklypaEngine *vm, const SceneDef *def);
	~TextinvaderScene() override;

	bool load() override;
	void onInit(int arg) override;
	void onClose() override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onMouseMove(int hotspot, int x, int y) override;
	void onKey(const Common::KeyState &key) override;
	bool handlesKey(const Common::KeyState &key) override;
	void onAnimFinished(Anim *anim) override;
	void onTimer(int id, int data) override;
	void onUpdate() override;

private:
	enum {
		kMaxSlots = 10,
		kMaxWordLen = 32,
		kHotspotExit = 2,
		kHotspotHelp = 3,
		kHotspotSign = 4,
		kBonusNone = 0,
		kBonusYellow = 1,    ///< clears the sky
		kBonusOrange = 2,    ///< slows the words down for a while
		kBonusRed = 3,       ///< energy
		kTimerSlowEnd = -2,  ///< timer data: the slow down period is over
		kTimerRemove = 1000, ///< timer data: 1000 + slot, remove the finished word
		kGroundY = 491       ///< words below this line are lost
	};

	enum {
		kSoundWrong = 0,
		kSoundRight,
		kSoundStart,
		kSoundDrop,
		kSoundCount
	};

	/** One falling word (the 600 byte slot structures at 0x682c20 of the original). */
	struct Slot {
		bool active;
		bool done;             ///< typed completely, waiting for its removal timer
		int bonus;
		Common::String word;
		int x, y;              ///< top left of the text
		int w, h;              ///< text extent
		int starX, starY;      ///< offset of the 128x128 shooting star clip
		float fy;              ///< exact vertical position
		Anim *text;            ///< the word in its colour (z 3)
		Anim *typed;           ///< the typed prefix in green on top of it (z 4)
		Anim *star;            ///< the "li" clip played when the word appears (z 5)
		Slot() : active(false), done(false), bonus(0), x(0), y(0), w(0), h(0), starX(0), starY(0), fy(0), text(nullptr), typed(nullptr), star(nullptr) {}
		Common::Rect rect() const { return Common::Rect(x, y, x + w, y + h); }
	};

	int rnd(int n);
	void loadWordList();
	const Common::String &pickWord(int maxLen);

	void showStartSign();
	void showScoreSign();
	void turnSign();
	void highlightButton(int hotspot);
	void startGame();
	void gameOver();

	void spawnWord(int slot);
	void drawWord(int slot);
	void redrawAll();
	void removeWord(int slot);
	void updateMeter();
	void applyBonus(int bonus);
	void checkLevel();
	void findTarget();
	int checkTyped() const;

	void addTimer(uint32 delayMs, int data);

	Slot _slots[kMaxSlots];
	Common::Array<Common::String> _words[kMaxWordLen]; ///< indexed by length
	Common::Array<Anim *> _ownAnims;                   ///< elements made from our own definitions

	BitmapFont _fontBlue, _fontGreen, _fontYellow, _fontOrange, _fontRed, _fontScore;
	Graphics::ManagedSurface *_lights;    ///< bitmap/julelys1.bmp, the lit chain
	Anim *_meter;                         ///< the lit part of the chain
	Anim *_signClip;                      ///< skilt.smk, the sign turning round
	Anim *_signBitmap;                    ///< skilt.bmp, the sign showing the score
	Anim *_scoreText;
	Anim *_sounds[kSoundCount];           ///< wrong, right, start, drop (4x4 audio clips)

	bool _running;
	bool _starting;                       ///< the sign is turning
	uint32 _startTime;
	uint32 _lastTick;
	int _timerSeq;
	int _highlighted;

	int _difficulty;                      ///< DAT_00687500: 0..2, the tournament player's level (FUN_004194a0), else 0
	int _slotCount;                       ///< words in the sky at once, 1..10
	int _maxLen;                          ///< longest word picked, 1..32
	int _score;
	int _energy;
	float _speed;                         ///< pixels per second
	float _slow;                          ///< 0.5 while an orange word works
	int _wordsTyped;
	int _wordsRemoved;
	int _level;
	int _nextBonus;                       ///< bonus type of the next bonus word minus one
	int _bonusMilestone;

	int _target;                          ///< slot being typed, -1 none
	Common::String _typed;
	Common::String _targetWord;
};

} // End of namespace Flaaklypa

#endif
