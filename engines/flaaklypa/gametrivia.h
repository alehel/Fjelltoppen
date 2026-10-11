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
#ifndef FLAAKLYPA_GAMETRIVIA_H
#define FLAAKLYPA_GAMETRIVIA_H

#include "common/array.h"
#include "common/formats/ini-file.h"
#include "common/rect.h"
#include "common/str.h"
#include "graphics/managed_surface.h"

#include "flaaklypa/button.h"
#include "flaaklypa/font.h"
#include "flaaklypa/scene.h"

namespace Flaaklypa {

class SoundPlayer;

/**
 * "Kjentmannsprøven" (the gametrivia sub game of add-on set 3, handler
 * 0x47e240): Solan shows slides of the game on a screen (a picture, a line
 * of text or a sound clip) and the player drags each slide onto the
 * painting of the scene it comes from before the time runs out. Four rounds
 * of eight slides with three to six paintings; slides not shown for the
 * longest time come first (their last showing is kept in the profile).
 */
class GametriviaScene : public Scene {
public:
	GametriviaScene(FlaaklypaEngine *vm, const SceneDef *def);
	~GametriviaScene() override;

	void onInit(int arg) override;
	void onClose() override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onMouseUp(int hotspot, int x, int y) override;
	void onMouseMove(int hotspot, int x, int y) override;
	bool handlesKey(const Common::KeyState &key) override;
	void onAnimStarted(Anim *anim) override;
	void onAnimFinished(Anim *anim) override;
	void onTimer(int id, int data) override;
	void onUpdate() override;

private:
	/** A round of trivia.ini [round] (DAT_006da3b8, 0x18 bytes each). */
	struct Round {
		int questions;   ///< +0
		int difficulty;  ///< +4 the questions' difficulty (difficulty<n>_<level>)
		int time;        ///< +8 ms per question (time<n>_<level>)
		int basePoints;  ///< +0xc
		int secPoints;   ///< +0x10 per second left
		int scenes;      ///< +0x14 paintings on the wall
	};

	/** A question of bitmapq/soundq/textq.ini (DAT_006da3c4, 0x24 bytes each). */
	struct Question {
		int index;              ///< +0 number over the three files, in file order
		int difficulty;         ///< +4
		uint32 lastShown;       ///< +8 minutes since 1998-10-01 (time() / 60 - 15120000), from the profile
		Common::String image;   ///< +0xc bitmap/image/<image>.bmp on the screen
		Common::String mouse;   ///< +0x10 bitmap/mouse/<mouse>.bmp while dragged
		Common::String text;    ///< +0x14 text written on the image, if any
		Common::String sound;   ///< +0x18 sound/<sound>.wav played, if any
		int scene;              ///< +0x1c the answer
		float key;              ///< +0x20 sort key of the round's selection
	};

	/** An answer of trivia.ini [answers] (DAT_006d70f0, 0x18 bytes each). */
	struct Answer {
		int index;   ///< +0
		int scene;   ///< +4 the scene number of the questions, hotspot = scene + 100
		int name;    ///< +8 the painting: set<n>/<i>/<n>_pictures<name>.bmp
		int first;   ///< +0xc its questions in the sorted question array
		int count;   ///< +0x10
		float key;   ///< +0x14 sort key of the round's selection
	};

	/** A PROGRESS object of the original (FUN_004089a0), horizontal. */
	struct Bar {
		Anim *anim;
		Graphics::ManagedSurface *bitmap;
		Bar() : anim(nullptr), bitmap(nullptr) {}
	};

	enum State {
		kStateIdle = 0,      ///< DAT_006da518: no question (start button, game over)
		kStateQuestion = 1,  ///< a slide is up and the clock runs
		kStateNext = 2       ///< Solan clicks on to the next slide
	};

	// Setup and teardown
	void loadData();
	void loadQuestions(const Common::INIFile &ini, int base, int count, const Common::String defaults[4]);
	void saveData();
	void createControls();
	void freeControls();

	// Rounds and questions
	int totalQuestions() const;
	int roundOf(int question) const;
	Question *currentQuestion();
	void sortAnswers(int difficulty);
	int collectQuestions(int difficulty, int scenes);
	void showPaintings(const Round &round);
	void selectRound(int round);
	void startGame();
	void showQuestion(int n);
	void drawQuestionText(Anim *image, const Common::String &text);
	void playQuestionSound();
	void nextPicture(bool correct);
	void advance();
	void gameOver();

	// Input
	void startDrag(int x, int y);
	void endDrag();
	void drop(int hotspot);
	void onButton(int id);
	bool isAnswerScene(int scene) const;
	bool timedOut(uint32 now) const { return _deadline < (int32)now; }

	// Display
	void updateClock();
	bool updateTime(uint32 now);
	void updateScore();
	void updateProgress();
	void resetCorrect();
	void updateCorrect();
	void setBar(Bar &bar, float value);
	Anim *createText(const char *name, const Common::Rect &r);
	void drawText(Anim *a, const Common::String &text, int align);
	void playClone(const char *name);

	void testStep(uint32 now);

	Common::INIFile _trivia;                ///< DAT_006da51c
	Common::Array<Round> _rounds;
	Common::Array<Question> _questions;     ///< sorted by scene, difficulty, last showing, index
	Common::Array<Answer> _answers;
	Common::Array<int> _answerOrder;        ///< DAT_006da524: the answers, least recently asked first
	Common::Array<int> _roundQuestions;     ///< DAT_006da528: the round's questions in the order shown

	int _level;          ///< DAT_006da3ac: column of difficulty<n>_<l> / time<n>_<l>, 1 outside tournaments
	State _state;        ///< DAT_006da518
	int _current;        ///< DAT_006d92c0: the question shown, 0.. over all rounds
	int _roundBase;      ///< DAT_006da3b0: questions of the rounds before the current one
	int32 _score;        ///< DAT_006dba30
	int _correct;        ///< DAT_006da520
	int32 _deadline;     ///< DAT_006da3c0: game time the question times out

	Common::String _image;        ///< DAT_00511890: the slide's element
	Common::String _frame;        ///< DAT_005119e0: the paintings' frame
	Common::Array<Common::String> _paintings;  ///< DAT_006da530: up to 32 painting elements
	Anim *_drag;                  ///< DAT_00511938: the dragged slide (FUN_0040c1f0), nullptr when none
	Common::Point _dragOffset;
	int _dragQuestion;            ///< the drag's data: the question's index in _questions

	SoundPlayer *_sounds;
	Common::String _soundName;    ///< DAT_006da3bc: the slide's sound, "" when none
	int _soundChannel;

	SceneButtons _buttons;
	BitmapFont _font;             ///< DAT_006d81d8 "Amerigo BT_10_"
	BitmapFont _textFont;         ///< DAT_006d71a8 "Comic Sans MS_10_b"
	Anim *_scoreText;             ///< DAT_006da300
	Anim *_progressText;          ///< DAT_006da3c8
	Anim *_correctText;           ///< DAT_006d9218
	Bar _correctBar;              ///< DAT_006da2f8
	Bar _progressBar;             ///< DAT_006da3b4
	Bar _timeBar;                 ///< DAT_006d6f98

	// test_gametrivia
	Common::String _testMoves;
	uint32 _testDelay;
	uint32 _questionTime;
	int _testDone;
};

} // End of namespace Flaaklypa

#endif
