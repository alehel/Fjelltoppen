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
#include "common/algorithm.h"
#include "common/config-manager.h"
#include "common/debug.h"
#include "common/endian.h"
#include "common/system.h"

#include "flaaklypa/dialog.h"
#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/gametrivia.h"
#include "flaaklypa/hiscore.h"
#include "flaaklypa/profile.h"
#include "flaaklypa/resources.h"
#include "flaaklypa/sound.h"

namespace Flaaklypa {

// Sub game "Kjentmannsprøven" (handler FUN_0047e240, code 0x47e240..0x47fe50).

enum {
	// BUTTON module ids (FUN_0047f590)
	kButtonHelp = 1,
	kButtonExit = 2,
	kButtonSound = 6,

	// hotspots: the start bitmap, the mask (screen and radio), Solan, the paintings
	kHotspotStart = 3,
	kHotspotScreen = 5,
	kHotspotRadio = 7,
	kHotspotSolan = 10,
	kHotspotAnswer = 100,     ///< + the answer's scene number

	kCharSolan = 1,

	kTimerClock = 0,
	kClockPeriod = 1000,

	kZClockLong = -1,
	kZClockShort = 0,
	kZRadio = 0,
	kZFrame = 99,
	kZPainting = 100,
	kZImage = 100,
	kZButtons = 100,
	kZControls = 200,
	kZStart = 400,
	kZDrag = 500,

	// the screen the slides are centred on (0x4a03b8: 636,154 - 783,295)
	kScreenCentreX = 636 + (783 - 636) / 2,
	kScreenCentreY = 154 + (295 - 154) / 2,
	// the slide's default position (the element structure at 0x511890)
	kImageDefaultX = 12,
	kImageDefaultY = 439,
	// the text of a text slide (FUN_0047e990): (5,5)-(105,95) of the image
	kTextLeft = 5,
	kTextTop = 5,
	kTextWidth = 100,
	kTextHeight = 90,

	// time() / 60 - 15120000: minutes since 1998-10-01 00:00 UTC
	kMinuteBase = 15120000,

	// FUN_0047ec00: jitter = rand(-100..99) * 0.01 * (range * 0.25 + 10)
	kJitterRange = 100,

	// text alignment flags of the original (FUN_004124e0: 0xHHVV)
	kAlignLeftMiddle = 0x001,
	kAlignCentre = 0x101,
	kAlignRightMiddle = 0x201
};

// The medal thresholds of FUN_0047fab0 (0x4a03d8); FUN_0041f9d0 gives no car parts for them
static const int kMedals[4] = { 5000, 10000, 15000, 20000 };

// The question files, in the order their questions are numbered (0x511e78)
static const char *const kQuestionFiles[] = { "bitmapq.ini", "soundq.ini", "textq.ini" };

// list_511668: Solan's idle loop
static const char *const kSolanIdle[] = {
	"move_head_breath", "blink_breath", "move_head_breath", "move_feet_breath", "move_head_breath", nullptr
};

// list_511680: Solan clicks to the next slide; the fourth (the long one) only after a right answer
static const char *const kNextPicture[] = {
	"solan_next_picture1", "solan_next_picture2", "solan_next_picture3", "solan_next_picture4", nullptr
};
enum { kNextPictureCount = 4 };

// DAT_006d71a0: the clips of kNextPicture played so far (FUN_0040a150), kept for the whole session
static uint32 s_nextPictureMask = 0;

// The texts of FUN_0047f590 (left, top, right, bottom)
static const int kScoreRect[4] = { 578, 522, 784, 538 };     // 0x4a0368: SCORE label and the score
static const int kCorrectRect[4] = { 11, 551, 131, 566 };    // 0x4a0378: CORRECT label
static const int kProgressRect[4] = { 11, 572, 131, 588 };   // 0x4a0388: PROGRESS label
static const int kCorrectValueRect[4] = { 511, 551, 562, 567 };  // 0x4a0398
static const int kProgressValueRect[4] = { 511, 572, 562, 588 }; // 0x4a03a8

static Common::Rect makeRect(const int r[4]) {
	return Common::Rect(r[0], r[1], r[2], r[3]);
}

// ---- INI helpers (FUN_0040e2d0 string, FUN_0040e3a0 "%d") ------------------------

static bool iniString(const Common::INIFile &ini, const char *section, const Common::String &key, Common::String &value) {
	Common::String raw;
	if (!ini.getKey(key, section, raw))
		return false;
	value = Resources::iniValue(raw);
	return true;
}

static int iniInt(const Common::INIFile &ini, const char *section, const Common::String &key) {
	Common::String v;
	if (!iniString(ini, section, key, v))
		return 0;
	return atoi(v.c_str());
}

// FUN_0047f360: the number of consecutive keys <format>0, <format>1, ...
static int iniCount(const Common::INIFile &ini, const char *section, const char *format) {
	int n = 0;
	Common::String v;
	while (ini.getKey(Common::String::format(format, n), section, v))
		n++;
	return n;
}

// ---- sorting -----------------------------------------------------------------------

// LAB_0047eb50 / LAB_0047ed40: ascending float keys (equal for unordered values)
struct KeyLess {
	const Common::Array<float> *keys;
	bool operator()(int a, int b) const { return (*keys)[a] < (*keys)[b]; }
};

GametriviaScene::GametriviaScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def),
	_level(1), _state(kStateIdle), _current(0), _roundBase(0), _score(0), _correct(0), _deadline(0),
	_drag(nullptr), _dragQuestion(-1), _sounds(nullptr), _soundChannel(-1), _buttons(this),
	_scoreText(nullptr), _progressText(nullptr), _correctText(nullptr),
	_testDelay(1500), _questionTime(0), _testDone(-1) {
}

GametriviaScene::~GametriviaScene() {
	delete _sounds;
	delete _correctBar.bitmap;
	delete _progressBar.bitmap;
	delete _timeBar.bitmap;
}

// ---- init / close (FUN_0047e370 / FUN_0047f7a0) ------------------------------------

void GametriviaScene::onInit(int arg) {
	playMusic("track5");
	_state = kStateIdle;
	_level = 1;
	// TODO: tournament mode (FUN_00419490): the level is the tournament
	// player's difficulty (FUN_004194a0(0, &DAT_006da3ac)).

	// The slides' WAV files play at the engine's sound volume (FUN_00413a40,
	// sound.ini included).
	_sounds = new SoundPlayer(_name);

	loadData();
	createControls();
	updateClock();
	// FUN_0040c480(now + 1000, 1000, 0): a periodic one second timer for the clock
	setTimer(kTimerClock, kClockPeriod);

	addCharacter(kCharSolan, kHotspotSolan);
	setCharacterZ(kCharSolan, 0);
	setCharacterAnims(kCharSolan, kSolanIdle, nullptr, nullptr);
	resetCharacters();
	playAnim("fan");

	// TODO: tournament mode: FUN_00419660 shows "tournament:PLAYERREADY" and
	// the game starts at once (FUN_0047e530) without the start button.
	addAnim("start", Anim::kDefaultPos, Anim::kDefaultPos, kZStart);

	if (ConfMan.hasKey("test_gametrivia")) {
		// Development aid "test_gametrivia=[<delay>:]<moves>": answers each
		// question <delay> ms (1500) after it shows; move n is for question
		// n (the last one repeats): c right painting, w a wrong one, m a
		// drop beside the paintings (a miss), t nothing (time out).
		Common::String s = ConfMan.get("test_gametrivia");
		const char *colon = strchr(s.c_str(), ':');
		if (colon) {
			_testDelay = atoi(s.c_str());
			s = Common::String(colon + 1);
		}
		_testMoves = s;
	}
}

void GametriviaScene::onClose() {
	freeControls();
	saveData();
	if (_sounds)
		_sounds->stopAll();
	_soundName.clear();
}

// FUN_0047ef50: the rounds of trivia.ini, the questions of the three files
// (with the time each was last shown, from the profile) and the answers.
void GametriviaScene::loadData() {
	_trivia.clear();
	if (!resources()->loadIni(_name, "trivia.ini", _trivia))
		warning("Gametrivia: trivia.ini missing");

	_rounds.clear();
	const int rounds = iniCount(_trivia, "round", "questions%d");
	for (int i = 0; i < rounds; i++) {
		Round r;
		r.questions = iniInt(_trivia, "round", Common::String::format("questions%d", i));
		r.difficulty = iniInt(_trivia, "round", Common::String::format("difficulty%d_%d", i, _level));
		r.time = iniInt(_trivia, "round", Common::String::format("time%d_%d", i, _level));
		r.scenes = iniInt(_trivia, "round", Common::String::format("scenes%d", i));
		r.basePoints = iniInt(_trivia, "round", Common::String::format("basepoints%d", i));
		r.secPoints = iniInt(_trivia, "round", Common::String::format("secpoints%d", i));
		_rounds.push_back(r);
	}

	_questions.clear();
	for (int f = 0; f < ARRAYSIZE(kQuestionFiles); f++) {
		Common::INIFile ini;
		if (!resources()->loadIni(_name, kQuestionFiles[f], ini)) {
			warning("Gametrivia: %s missing", kQuestionFiles[f]);
			continue;
		}
		// [default] image, mouse, text and sound; an empty string stands for
		// a missing value (the original's null pointer)
		Common::String defaults[4];
		iniString(ini, "default", "image", defaults[0]);
		iniString(ini, "default", "mouse", defaults[1]);
		iniString(ini, "default", "text", defaults[2]);
		iniString(ini, "default", "sound", defaults[3]);
		loadQuestions(ini, _questions.size(), iniCount(ini, "questions", "scene%d"), defaults);
	}

	// FUN_00411e70(0, ...): the minute each question was last shown, one
	// int per question in file order; all 0 without the record (or when
	// its size does not match, e.g. another language's textq.ini)
	const uint32 size = _questions.size() * 4;
	Common::Array<byte> buf(size, 0);
	if (size && _vm->profile() && !_vm->profile()->getRecord(_name, Profile::kRecordValue, 0, buf.data(), size))
		memset(buf.data(), 0, size);
	int shown = 0;
	for (uint i = 0; i < _questions.size(); i++) {
		_questions[i].lastShown = READ_LE_UINT32(&buf[i * 4]);
		if (_questions[i].lastShown)
			shown++;
	}

	// LAB_0047f330: by scene, difficulty, last showing (signed), number
	Common::sort(_questions.begin(), _questions.end(), [](const Question &a, const Question &b) {
		if (a.scene != b.scene)
			return a.scene < b.scene;
		if (a.difficulty != b.difficulty)
			return a.difficulty < b.difficulty;
		if (a.lastShown != b.lastShown)
			return (int32)a.lastShown < (int32)b.lastShown;
		return a.index < b.index;
	});

	_answers.clear();
	const int answers = iniCount(_trivia, "answers", "scene%d");
	for (int i = 0; i < answers; i++) {
		Answer a;
		a.index = i;
		a.scene = iniInt(_trivia, "answers", Common::String::format("scene%d", i));
		a.name = iniInt(_trivia, "answers", Common::String::format("name%d", i));
		// FUN_0047f3d0: the answer's questions (one block, the array is sorted by scene)
		int q = 0;
		while (q < (int)_questions.size() && _questions[q].scene != a.scene)
			q++;
		a.first = q;
		while (q < (int)_questions.size() && _questions[q].scene == a.scene)
			q++;
		a.count = q - a.first;
		a.key = 0.0f;
		_answers.push_back(a);
	}
	debug(1, "Gametrivia: %d rounds (%d questions), %d questions (%d shown before), %d answers, level %d",
	      _rounds.size(), totalQuestions(), _questions.size(), shown, _answers.size(), _level);
}

// FUN_0047f430
void GametriviaScene::loadQuestions(const Common::INIFile &ini, int base, int count, const Common::String defaults[4]) {
	for (int i = 0; i < count; i++) {
		Question q;
		q.index = base + i;
		q.difficulty = iniInt(ini, "questions", Common::String::format("difficulty%d", i));
		q.lastShown = 0;
		if (!iniString(ini, "questions", Common::String::format("image%d", i), q.image))
			q.image = defaults[0];
		if (!iniString(ini, "questions", Common::String::format("mouse%d", i), q.mouse))
			q.mouse = defaults[1];
		if (q.mouse.empty())
			q.mouse = q.image;
		if (!iniString(ini, "questions", Common::String::format("text%d", i), q.text))
			q.text = defaults[2];
		if (!iniString(ini, "questions", Common::String::format("sound%d", i), q.sound))
			q.sound = defaults[3];
		q.scene = iniInt(ini, "questions", Common::String::format("scene%d", i));
		q.key = 0.0f;
		_questions.push_back(q);
	}
}

// FUN_0047f7d0: the questions' last showings go back into the profile
void GametriviaScene::saveData() {
	if (!_vm->profile() || _questions.empty())
		return;
	Common::Array<byte> buf(_questions.size() * 4, 0);
	for (const auto &q : _questions)
		if (q.index >= 0 && q.index < (int)_questions.size())
			WRITE_LE_UINT32(&buf[q.index * 4], q.lastShown);
	_vm->profile()->setRecord(_name, Profile::kRecordValue, 0, buf.data(), buf.size());
	_vm->profileChanged();
}

// FUN_0047f590: fonts, labels, texts, buttons and the three bars
void GametriviaScene::createControls() {
	_font.load("Amerigo BT_10_");
	_textFont.load("Comic Sans MS_10_b");
	// "Amerigo BT_14_" (DAT_006d92c8) is loaded but never used, and so is an
	// empty text area at (144,522)-(561,538) (DAT_006da470).

	// The labels (FUN_00407a30): an element of the text's size aligned in the rectangle
	drawText(createText("label_score", makeRect(kScoreRect)), _vm->getString("gametrivia:SCORE"), kAlignLeftMiddle);
	drawText(createText("label_progress", makeRect(kProgressRect)), _vm->getString("gametrivia:PROGRESS"), kAlignCentre);
	drawText(createText("label_correct", makeRect(kCorrectRect)), _vm->getString("gametrivia:CORRECT"), kAlignCentre);
	// The text areas (FUN_00407930), empty until the game starts
	_scoreText = createText("text_score", makeRect(kScoreRect));
	_progressText = createText("text_progress", makeRect(kProgressValueRect));
	_correctText = createText("text_correct", makeRect(kCorrectValueRect));

	_buttons.setDefaultZ(kZButtons);
	_buttons.setFont(&_font);
	_buttons.add(kButtonHelp, 0, 0, "help_0", "help_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonExit, 0x2d8, 0, "exit_0", "exit_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonSound, 8, 0x205, "button_0", "button_1", "button_2", nullptr, _vm->getString("gametrivia:SOUND"), false);

	// PROGRESS_Create (FUN_004089a0(x, y, z, bitmap, 0, 0x100)): the element
	// is one pixel wider than the bitmap and sits one pixel to the left
	const uint32 green = _vm->_screen->format.RGBToColor(0, 0xff, 0);
	static const struct {
		const char *bitmap;
		int x, y;
	} kBars[3] = {
		{ "correct", 0x90, 0x226 },
		{ "progress", 0x90, 0x23b },
		{ "time", 0x244, 0x22e }
	};
	Bar *bars[3] = { &_correctBar, &_progressBar, &_timeBar };
	for (int i = 0; i < 3; i++) {
		Bar &b = *bars[i];
		delete b.bitmap;
		b.bitmap = _vm->_resources->loadBitmap(_name, Common::String::format("bitmap/%s.bmp", kBars[i].bitmap), _vm->_screen->format);
		const int w = b.bitmap ? b.bitmap->w : 1, h = b.bitmap ? b.bitmap->h : 1;
		b.anim = defineSurfaceAnim(Common::String::format("bar_%s", kBars[i].bitmap).c_str(), w + 1, h, green);
		b.anim->add(kBars[i].x - 1, kBars[i].y, kZControls);
		setBar(b, 0.0f);
	}
}

// FUN_0047f8d0
void GametriviaScene::freeControls() {
	Bar *bars[3] = { &_correctBar, &_progressBar, &_timeBar };
	for (int i = 0; i < 3; i++) {
		if (bars[i]->anim && bars[i]->anim->isAdded())
			bars[i]->anim->remove();
		delete bars[i]->bitmap;
		bars[i]->bitmap = nullptr;
	}
	_buttons.removeAll();
}

// ---- rounds ------------------------------------------------------------------------

// FUN_0047e5e0
int GametriviaScene::totalQuestions() const {
	int n = 0;
	for (const auto &r : _rounds)
		n += r.questions;
	return n;
}

// FUN_0047e5a0: the round a question belongs to, -1 past the last
int GametriviaScene::roundOf(int question) const {
	int sum = 0;
	for (uint i = 0; i < _rounds.size(); i++) {
		sum += _rounds[i].questions;
		if (question < sum)
			return i;
	}
	return -1;
}

// FUN_0047e880: the round's list is indexed from the round's first question
GametriviaScene::Question *GametriviaScene::currentQuestion() {
	const int i = _current - _roundBase;
	if (i < 0 || i >= (int)_roundQuestions.size())
		return nullptr;
	return &_questions[_roundQuestions[i]];
}

// FUN_0047ec00: a random offset of the sort keys, up to a quarter of their
// range plus 10 minutes either way
static float jitter(FlaaklypaEngine *vm, float first, float last) {
	const int r = (int)vm->getRandomNumber(2 * kJitterRange - 1) - kJitterRange;
	return (float)((double)r * (double)0.01f * (((double)last - (double)first) * 0.25 + 10.0));
}

// FUN_0047ea80: the answers ordered by the average last showing of their
// questions of the difficulty, least recent first, with some randomness
void GametriviaScene::sortAnswers(int difficulty) {
	Common::Array<float> keys(_answers.size(), 0.0f);
	_answerOrder.clear();
	for (uint i = 0; i < _answers.size(); i++) {
		Answer &a = _answers[i];
		// FUN_0047eb90: unsigned sum / count; the original divides 0 by 0
		// (NaN) for a scene without such questions, which the data never has
		uint32 sum = 0;
		int n = 0;
		for (int q = a.first; q < a.first + a.count; q++) {
			if (_questions[q].difficulty == difficulty) {
				sum += _questions[q].lastShown;
				n++;
			}
		}
		a.key = n ? (float)((double)sum / n) : 0.0f;
		keys[i] = a.key;
		_answerOrder.push_back(i);
	}
	if (_answerOrder.empty())
		return;
	KeyLess less;
	less.keys = &keys;
	Common::sort(_answerOrder.begin(), _answerOrder.end(), less);
	const float first = keys[_answerOrder[0]], last = keys[_answerOrder.back()];
	for (uint i = 0; i < _answerOrder.size(); i++) {
		Answer &a = _answers[_answerOrder[i]];
		a.key = (float)(jitter(_vm, first, last) + (double)a.key);
		keys[_answerOrder[i]] = a.key;
	}
	Common::sort(_answerOrder.begin(), _answerOrder.end(), less);
}

// FUN_0047ec40: the questions of the difficulty of the first <scenes>
// answers, ordered by their last showing, least recent first, with some
// randomness. Returns their number.
int GametriviaScene::collectQuestions(int difficulty, int scenes) {
	_roundQuestions.clear();
	for (int i = 0; i < scenes && i < (int)_answerOrder.size(); i++) {
		const Answer &a = _answers[_answerOrder[i]];
		for (int q = a.first; q < a.first + a.count; q++) {
			if (_questions[q].difficulty == difficulty) {
				_questions[q].key = (float)_questions[q].lastShown;
				_roundQuestions.push_back(q);
			}
		}
	}
	if (_roundQuestions.empty())
		return 0;
	Common::Array<float> keys(_questions.size(), 0.0f);
	for (uint i = 0; i < _questions.size(); i++)
		keys[i] = _questions[i].key;
	KeyLess less;
	less.keys = &keys;
	Common::sort(_roundQuestions.begin(), _roundQuestions.end(), less);
	const float first = keys[_roundQuestions[0]], last = keys[_roundQuestions.back()];
	for (uint i = 0; i < _roundQuestions.size(); i++) {
		Question &q = _questions[_roundQuestions[i]];
		q.key = (float)(jitter(_vm, first, last) + (double)q.key);
		keys[_roundQuestions[i]] = q.key;
	}
	Common::sort(_roundQuestions.begin(), _roundQuestions.end(), less);
	return _roundQuestions.size();
}

// FUN_0047ed80: the frame of the round's paintings and the first <scenes>
// answers' paintings in it (positions from trivia.ini [scenes])
void GametriviaScene::showPaintings(const Round &round) {
	if (!_frame.empty() && isAnimAdded(_frame.c_str()))
		removeAnim(_frame.c_str());
	for (const auto &p : _paintings)
		if (isAnimAdded(p.c_str()))
			removeAnim(p.c_str());
	_paintings.clear();

	const int n = round.scenes;
	_frame = Common::String::format("set%d/frame", n);
	const int fx = iniInt(_trivia, "scenes", Common::String::format("set%d_frame_x", n));
	const int fy = iniInt(_trivia, "scenes", Common::String::format("set%d_frame_y", n));
	defineAnim(_frame.c_str(), false, true, 0, fx, fy, kZFrame)->add(fx, fy, kZFrame);
	for (int i = 1; i <= n && i <= (int)_answerOrder.size(); i++) {
		const Answer &a = _answers[_answerOrder[i - 1]];
		const int x = iniInt(_trivia, "scenes", Common::String::format("set%d_%d_x", n, i));
		const int y = iniInt(_trivia, "scenes", Common::String::format("set%d_%d_y", n, i));
		Common::String name = Common::String::format("set%d/%d/%d_pictures%04d", n, i, n, a.name);
		defineAnim(name.c_str(), false, true, a.scene + kHotspotAnswer, x, y, kZPainting)->add(x, y, kZPainting);
		_paintings.push_back(name);
		debug(1, "Gametrivia: painting %d: scene %d (%s) at %d,%d", i, a.scene, name.c_str(), x, y);
	}
}

// FUN_0047ea50
void GametriviaScene::selectRound(int round) {
	if (round < 0 || round >= (int)_rounds.size())
		return;
	const Round &r = _rounds[round];
	sortAnswers(r.difficulty);
	int n = collectQuestions(r.difficulty, r.scenes);
	if (n < r.questions)
		warning("Gametrivia: round %d has %d questions of difficulty %d, needs %d", round, n, r.difficulty, r.questions);
	showPaintings(r);
	debug(1, "Gametrivia: round %d: difficulty %d, %d ms, %d paintings, %d questions to pick from",
	      round, r.difficulty, r.time, r.scenes, n);
}

// FUN_0047e530: the start button
void GametriviaScene::startGame() {
	if (_rounds.empty() || totalQuestions() <= 0)
		return;
	_vm->setGameRunning(true);
	if (isAnimAdded("start"))
		removeAnim("start");
	_score = 0;
	_correct = 0;
	_roundBase = 0;
	updateScore();
	resetCorrect();
	updateProgress();
	selectRound(roundOf(0));
	showQuestion(0);
}

// FUN_0047e710: slide n goes up and its clock starts
void GametriviaScene::showQuestion(int n) {
	_current = n;
	Question *q = currentQuestion();
	if (_drag)
		endDrag();
	if (!_soundName.empty()) {
		// FUN_00414000
		_sounds->stop(_soundChannel);
		_soundChannel = -1;
		_soundName.clear();
	}
	if (!_image.empty() && isAnimAdded(_image.c_str()))
		removeAnim(_image.c_str());
	if (!q) {
		// The original reads past the round's list (a null pointer)
		warning("Gametrivia: no question %d", n);
		gameOver();
		return;
	}
	q->lastShown = FlaaklypaEngine::profileTime() / 60 - kMinuteBase;

	_image = "image/" + q->image;
	if (!q->sound.empty()) {
		_soundName = q->sound;
		playQuestionSound();
	}
	Anim *a = defineAnim(_image.c_str(), false, true, 0, kImageDefaultX, kImageDefaultY, kZImage);
	a->add(Anim::kDefaultPos, Anim::kDefaultPos, kZImage);
	// FUN_00412cf0: centred on the screen
	const Common::Rect r = a->rect();
	a->setPos(kScreenCentreX - r.width() / 2, kScreenCentreY - r.height() / 2);
	if (!q->text.empty())
		drawQuestionText(a, q->text);
	updateProgress();

	const uint32 now = _vm->getGameMillis();
	_deadline = (int32)now + _rounds[roundOf(n)].time;
	updateTime(now);
	_state = kStateQuestion;
	_questionTime = now;

	// For scripted tests: where the right painting is
	Common::String where = "not shown";
	for (const auto &p : _paintings) {
		Anim *pa = anim(p.c_str());
		if (pa->hotspot() == q->scene + kHotspotAnswer) {
			const Common::Rect pr = pa->rect();
			where = Common::String::format("%d,%d", (pr.left + pr.right) / 2, (pr.top + pr.bottom) / 2);
		}
	}
	debug(1, "Gametrivia: question %d: #%d difficulty %d scene %d (painting at %s) image %s%s%s%s%s", n, q->index,
	      q->difficulty, q->scene, where.c_str(), q->image.c_str(), q->text.empty() ? "" : " text: ", q->text.c_str(),
	      q->sound.empty() ? "" : " sound: ", q->sound.c_str());
}

// FUN_0047e990: the text in Comic Sans MS_10_b, word wrapped to 100 pixels,
// each line centred, the block centred vertically in (5,5)-(105,95)
void GametriviaScene::drawQuestionText(Anim *image, const Common::String &text) {
	Graphics::ManagedSurface *s = image->surface();
	if (!s)
		return;
	const Common::Array<Common::String> lines = wordWrap(_textFont, text, kTextWidth);
	const int lineH = _textFont.height();
	const int h = lines.size() * lineH;
	int y = kTextTop + (kTextHeight - h) / 2;
	for (const auto &line : lines) {
		_textFont.drawString(*s, kTextLeft + (kTextWidth - _textFont.stringWidth(line)) / 2, y, line);
		y += lineH;
	}
}

// FUN_0047e8b0: the slide's sound starts (it carries on if it plays) and
// the radio lights up while it plays
void GametriviaScene::playQuestionSound() {
	if (_soundName.empty())
		return;
	if (!_sounds->isPlaying(_soundChannel))
		_soundChannel = _sounds->play(_soundName);
	if (!isAnimAdded("radio"))
		addAnim("radio", Anim::kDefaultPos, Anim::kDefaultPos, kZRadio);
}

// FUN_0047fc60: Solan clicks to the next slide with a clip not played
// lately; the fourth only after a right answer (else the first plays, the
// fourth counting as played)
void GametriviaScene::nextPicture(bool correct) {
	const uint32 full = (1u << kNextPictureCount) - 1;
	if (s_nextPictureMask == full)
		s_nextPictureMask = 0;
	int free = 0;
	for (int i = 0; i < kNextPictureCount; i++)
		if (!(s_nextPictureMask & (1u << i)))
			free++;
	int k = free ? (int)_vm->getRandomNumber(free - 1) : 0;
	int idx = 0;
	for (int i = 0; i < kNextPictureCount; i++) {
		if (s_nextPictureMask & (1u << i))
			continue;
		if (k < 1) {
			idx = i;
			break;
		}
		k--;
	}
	s_nextPictureMask |= 1u << idx;
	if (!correct && idx == kNextPictureCount - 1)
		idx = 0;
	_state = kStateNext;
	playSingle(kNextPicture[idx]);
}

// FUN_0047fa50: Solan's clip has ended
void GametriviaScene::advance() {
	const int next = _current + 1;
	if (next >= totalQuestions()) {
		gameOver();
		return;
	}
	const int r0 = roundOf(_current), r1 = roundOf(next);
	if (r0 != r1) {
		_roundBase += _rounds[r0].questions;
		selectRound(r1);
	}
	showQuestion(next);
}

// FUN_0047fab0
void GametriviaScene::gameOver() {
	_vm->setGameRunning(false);
	_state = kStateIdle;
	_current = totalQuestions();
	updateProgress();
	if (!_image.empty() && isAnimAdded(_image.c_str()))
		removeAnim(_image.c_str());
	debug(1, "Gametrivia: game over, %d of %d right, score %d", _correct, totalQuestions(), _score);
	// TODO: tournament mode: no message box, the score goes to the
	// tournament (inside FUN_0041f8c0) and its next game starts, no start button.
	Common::String text = Common::String::format(_vm->getString("gametrivia:GAMEOVER").c_str(), _correct, totalQuestions());
	_vm->messageBox("interfaceh:GAMEOVER", text, MessageBox::kButtonOk);
	// FUN_0041f9d0(0, score, 0x4a03d8): medals without car parts
	registerScoreNoCarParts(_vm, "", _score, kMedals);
	addAnim("start", Anim::kDefaultPos, Anim::kDefaultPos, kZStart);
}

// ---- input ------------------------------------------------------------------------

// FUN_0047f960 (event 0x105)
void GametriviaScene::onMouseDown(int hotspot, int x, int y) {
	if (hotspot == kHotspotStart)
		startGame();
	else if (hotspot == kHotspotScreen && _state == kStateQuestion)
		startDrag(x, y);
	else if (hotspot == kHotspotRadio)
		playQuestionSound();
	_buttons.mouseDown(hotspot, x, y);
}

void GametriviaScene::onMouseUp(int hotspot, int x, int y) {
	// The EVENT module ends a drag on release with event 0x10b (FUN_0040c260)
	if (_drag)
		drop(hotspot);
	int id = _buttons.mouseUp();
	if (id)
		onButton(id);
}

void GametriviaScene::onMouseMove(int hotspot, int x, int y) {
	_buttons.setHover(hotspot);
}

// FUN_0047fd10: the space bar is eaten (no sequence abort)
bool GametriviaScene::handlesKey(const Common::KeyState &key) {
	return key.keycode == Common::KEYCODE_SPACE;
}

// FUN_0047fcf0 (event 0x116)
void GametriviaScene::onButton(int id) {
	switch (id) {
	case kButtonHelp:
		_vm->showHelp();
		break;
	case kButtonExit:
		_vm->endGame();
		break;
	case kButtonSound:
		playQuestionSound();
		break;
	default:
		break;
	}
}

// The mouse picks up the slide: bitmap/mouse/<mouse>.bmp centred on the
// mouse follows it (FUN_0040c1f0 with the question as data)
void GametriviaScene::startDrag(int x, int y) {
	Question *q = currentQuestion();
	if (!q)
		return;
	Common::String name = "mouse/" + q->mouse;
	_drag = defineAnim(name.c_str(), false, true, 0, 0, 0, kZDrag);
	_drag->add(x, y, kZDrag);
	const Common::Rect r = _drag->rect();
	_drag->setPos(x - r.width() / 2, y - r.height() / 2);
	_dragOffset = Common::Point(_drag->x() - x, _drag->y() - y);
	_dragQuestion = q - &_questions[0];
	playClone("click");
}

void GametriviaScene::endDrag() {
	if (_drag && _drag->isAdded())
		_drag->remove();
	_drag = nullptr;
}

// FUN_0047fd20 (event 0x10b): the slide is dropped on a hotspot
void GametriviaScene::drop(int hotspot) {
	const int question = _dragQuestion;
	endDrag();
	const uint32 now = _vm->getGameMillis();
	if (timedOut(now) || question < 0 || question >= (int)_questions.size())
		return;
	const Question &q = _questions[question];
	const int scene = hotspot - kHotspotAnswer;
	if (!isAnswerScene(scene)) {
		// Beside the paintings: the slide stays up
		playClone("miss");
		debug(1, "Gametrivia: missed (hotspot %d)", hotspot);
		return;
	}
	if (scene == q.scene) {
		playClone("correct");
		_correct++;
		const Round &r = _rounds[roundOf(_current)];
		int seconds = (_deadline - (int32)now) / 1000;
		if (seconds < 0)
			seconds = 0;
		const int points = r.basePoints + r.secPoints * seconds;
		_score += points;
		debug(1, "Gametrivia: right, %d s left: %d + %d * %d = %d points, score %d", seconds, r.basePoints,
		      r.secPoints, seconds, points, _score);
		updateScore();
		updateCorrect();
		nextPicture(true);
		return;
	}
	debug(1, "Gametrivia: wrong (scene %d, right %d)", scene, q.scene);
	playClone("wrong");
	updateCorrect();
	nextPicture(false);
}

// FUN_0047fe10: any answer of trivia.ini, shown or not
bool GametriviaScene::isAnswerScene(int scene) const {
	for (const auto &a : _answers)
		if (a.scene == scene)
			return true;
	return false;
}

// ---- events -----------------------------------------------------------------------

// FUN_0047f9f0 (event 0x10f): the projector's shutter clicks with Solan
void GametriviaScene::onAnimStarted(Anim *a) {
	for (int i = 0; i < kNextPictureCount; i++)
		if (!strcmp(a->name(), kNextPicture[i]))
			playAnim("shutter");
}

// FUN_0047fa20 (event 0x110)
void GametriviaScene::onAnimFinished(Anim *a) {
	for (int i = 0; i < kNextPictureCount; i++)
		if (!strcmp(a->name(), kNextPicture[i]))
			advance();
	if (!strcmp(a->name(), "shutter"))
		removeAnim("shutter");
}

// FUN_0047e450 (event 0x111)
void GametriviaScene::onTimer(int id, int data) {
	if (id != kTimerClock)
		return;
	setTimer(kTimerClock, kClockPeriod);
	updateClock();
}

// FUN_0047fb80 (event 0x112)
void GametriviaScene::onUpdate() {
	if (_drag) {
		const Common::Point m = _vm->mousePos();
		_drag->setPos(m.x + _dragOffset.x, m.y + _dragOffset.y);
	}
	if (isAnimAdded("radio") && (_soundName.empty() || !_sounds->isPlaying(_soundChannel)))
		removeAnim("radio");
	const uint32 now = _vm->getGameMillis();
	if (_state == kStateQuestion && !updateTime(now)) {
		debug(1, "Gametrivia: time out");
		// FUN_0040c260 drops a dragged slide; the drop comes too late to count
		if (_drag)
			endDrag();
		playClone("timeout");
		updateCorrect();
		nextPicture(false);
	}
	testStep(now);
}

// ---- display ----------------------------------------------------------------------

// FUN_0047e450: the clock on the wall shows the time of day; the long hand
// is frame <minute>, the short one frame minute / 12 + (hour % 12) * 5
void GametriviaScene::updateClock() {
	TimeDate t;
	g_system->getTimeAndDate(t);
	Anim *l = anim("clocklong");
	if (!l->isAdded())
		l->add(Anim::kDefaultPos, Anim::kDefaultPos, kZClockLong);
	l->showFrame(t.tm_min);
	Anim *s = anim("clockshort");
	if (!s->isAdded())
		s->add(Anim::kDefaultPos, Anim::kDefaultPos, kZClockShort);
	s->showFrame(t.tm_min / 12 + (t.tm_hour % 12) * 5);
}

// FUN_0047e8e0: the time bar shows the time left; false once it is out.
// (It also formats the time left as "%02d:%02d" s:cs for nothing.)
bool GametriviaScene::updateTime(uint32 now) {
	if (timedOut(now))
		return false;
	const int32 left = _deadline - (int32)now;
	const int round = roundOf(_current);
	const int time = round >= 0 ? _rounds[round].time : 0;
	setBar(_timeBar, time ? (float)left / (float)time : 0.0f);
	return true;
}

// FUN_0047e600
void GametriviaScene::updateScore() {
	drawText(_scoreText, Common::String::format("%d", _score), kAlignRightMiddle);
}

// FUN_0047e640: "n / total" slides shown
void GametriviaScene::updateProgress() {
	const int total = totalQuestions();
	int shown = _current + 1;
	if (shown < 0)
		shown = 0;
	else if (shown > total)
		shown = total;
	setBar(_progressBar, total ? (float)shown / (float)total : 0.0f);
	drawText(_progressText, Common::String::format("%d / %d", shown, total), kAlignCentre);
}

// FUN_0047e6c0
void GametriviaScene::resetCorrect() {
	_correct = 0;
	setBar(_correctBar, 0.0f);
	drawText(_correctText, "0 / 0", kAlignCentre);
}

// FUN_0047fc00: the bar is right answers of all slides, the text right
// answers of the slides shown
void GametriviaScene::updateCorrect() {
	const int total = totalQuestions();
	setBar(_correctBar, total ? (float)_correct / (float)total : 0.0f);
	drawText(_correctText, Common::String::format("%d / %d", _correct, _current + 1), kAlignCentre);
}

// PROGRESS_Set (FUN_00408b00), horizontal: the left round(width * value)
// columns of the bitmap, one pixel in
void GametriviaScene::setBar(Bar &bar, float value) {
	if (!bar.anim || !bar.anim->surface())
		return;
	Graphics::ManagedSurface *s = bar.anim->surface();
	s->fillRect(Common::Rect(s->w, s->h), s->format.RGBToColor(0, 0xff, 0));
	if (!bar.bitmap)
		return;
	value = CLIP(value, 0.0f, 1.0f);
	const float f = (float)bar.bitmap->w * value;
	const int n = MIN<int>((int)(f > 0.0f ? f + 0.5f : f - 0.5f), bar.bitmap->w);
	if (n > 0)
		s->blitFrom(*bar.bitmap, Common::Rect(0, 0, n, bar.bitmap->h), Common::Point(1, 0));
}

// A text element (FUN_00407930): a key coloured surface over the rectangle
Anim *GametriviaScene::createText(const char *name, const Common::Rect &r) {
	Anim *a = defineSurfaceAnim(name, r.width(), r.height(), _vm->_screen->format.RGBToColor(0, 0xff, 0));
	a->add(r.left, r.top, kZControls);
	return a;
}

// FUN_004079c0 / FUN_004124e0: the text aligned by 0xHHVV (H 0 left, 1
// centre, 2 right; V 0 top, 1 middle, 2 bottom)
void GametriviaScene::drawText(Anim *a, const Common::String &text, int align) {
	if (!a || !a->surface())
		return;
	Graphics::ManagedSurface *s = a->surface();
	s->fillRect(Common::Rect(s->w, s->h), s->format.RGBToColor(0, 0xff, 0));
	const int w = _font.stringWidth(text), h = _font.height();
	int x = 0, y = 0;
	switch (align & 0xff00) {
	case 0x100: x = (s->w - w) / 2; break;
	case 0x200: x = s->w - w; break;
	default: break;
	}
	switch (align & 0xff) {
	case 1: y = (s->h - h) / 2; break;
	case 2: y = s->h - h; break;
	default: break;
	}
	_font.drawString(*s, x, y, text);
}

// FUN_00412c20 (SCENE_PlayAnimClone): the sound effects; here the element
// starts again instead of a copy
void GametriviaScene::playClone(const char *name) {
	Anim *a = anim(name);
	a->remove();
	a->play();
	a->setRemoveWhenDone(true);
}

// ---- development aid --------------------------------------------------------------

void GametriviaScene::testStep(uint32 now) {
	if (_testMoves.empty() || _state != kStateQuestion || _testDone == _current || now - _questionTime < _testDelay)
		return;
	_testDone = _current;
	const char move = _current < (int)_testMoves.size() ? _testMoves[_current] : _testMoves.lastChar();
	Question *q = currentQuestion();
	if (!q || move == 't')
		return;
	int hotspot = 0;
	if (move == 'c') {
		hotspot = q->scene + kHotspotAnswer;
	} else if (move == 'w') {
		for (const auto &p : _paintings) {
			const int h = anim(p.c_str())->hotspot();
			if (h != q->scene + kHotspotAnswer) {
				hotspot = h;
				break;
			}
		}
	}
	debug(1, "Gametrivia: test move '%c' on question %d: drop on hotspot %d", move, _current, hotspot);
	startDrag(kScreenCentreX, kScreenCentreY);
	drop(hotspot);
}

} // End of namespace Flaaklypa
