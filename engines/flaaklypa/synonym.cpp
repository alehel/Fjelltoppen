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
#include "common/formats/ini-file.h"
#include "common/hash-str.h"
#include "common/hashmap.h"
#include "common/textconsole.h"

#include "flaaklypa/anim.h"
#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/hiscore.h"
#include "flaaklypa/profile.h"
#include "flaaklypa/resources.h"
#include "flaaklypa/synonym.h"
#include "flaaklypa/tournament.h"

namespace Flaaklypa {

// Sub game "Reodors Ordmaskin" (handler 0x482230, code 0x482230..0x4844a7).

enum {
	kHotspotStart = 1,          ///< the "start" ball of the scene table
	kButtonPass = 2,
	kButtonHelp = 3,
	kButtonExit = 4,
	kButtonShuffle = 5,
	kButtonHint = 6,
	kHotspotFirstLetter = 100,  ///< + the letter's place in the word (FUN_00483290: 100..163)

	kStateIdle = 0,             ///< no game: the start ball shows
	kStatePlaying = 1,          ///< the player builds the word
	kStateRolling = 2,          ///< the letters roll onto the upper shelf
	kStateFalling = 3,          ///< the letters fall into the funnel
	kStateWaiting = 4,          ///< ballstart1, blank1 or clockring plays

	kZText = 10,
	kZOverlay = 0x19,
	kZButtons = 0x1a,
	kZStart = 0x32,
	kZDropped = 0x14,
	kZDragged = 0x1e,

	kLargeSize = 0x28,          ///< "large_<c>": the balls on the lower shelf, 40 x 40
	kSmallSize = 0x20,          ///< "small_<c>": the balls on the upper shelf, 32 x 32
	kRollInterval = 133,        ///< ms between two letters starting to roll (FUN_00484100)
	kMinuteBase = 0xe6b680      ///< FUN_004827c0: minutes from 1970 to 1998-10-01
};

// The rectangles (left, top, right, bottom) at 0x4a0510..0x4a057c
static const int kSynonymRect[4] = { 187, 166, 508, 203 };  // 0x4a0510: the word shown
static const int kUpperShelf[4] = { 188, 228, 509, 260 };   // 0x4a0520
static const int kLowerShelf[4] = { 110, 276, 543, 366 };   // 0x4a0530: the drop area
static const int kPointsRect[4] = { 295, 372, 388, 384 };   // 0x4a0540: "Poeng"
static const int kScoreRect[4] = { 295, 384, 388, 417 };    // 0x4a0550
static const int kLevelRect[4] = { 295, 417, 388, 436 };    // 0x4a0560 "Nivå", 0x4a0570 the number

// FUN_0041f9d0(0, score, 0x4a0580): the medals (no car parts in this game)
static const int kMedals[4] = { 2500, 5000, 10000, 20000 };

// Float constants of the original
static const float kRollStartX = 518.0f;   // 0x44018000: where a letter appears (under overlayright)
static const float kRollSpeed = 300.0f;    // 0x49d9b8: pixels per second along the upper shelf
static const float kFallSpeed = 400.0f;    // 0x49f25c: pixels per second along the lower shelf and down
static const float kUpperEnd = 156.0f;     // 0x4a0590: upper shelf letters vanish (overlayleft)
static const float kFunnelEnd = 361.0f;    // 0x4a0594: the letters are gone into the funnel

static const char *const kBlank1 = "../../common/animation/blank1";

static Common::Rect makeRect(const int *r) {
	return Common::Rect(r[0], r[1], r[2], r[3]);
}

// FUN_004124e0: a w x h box in r with the alignment 0xHHVV (H 0 left,
// 1 centred, 2 right; V 0 top, 1 centred, 2 bottom).
static Common::Point alignIn(int w, int h, const Common::Rect &r, int align) {
	Common::Point p(r.left, r.top);
	if ((align & 0xff00) == 0x100)
		p.x = (r.width() - w) / 2 + r.left;
	else if ((align & 0xff00) == 0x200)
		p.x = r.right - w;
	if ((align & 0xff) == 1)
		p.y = (r.height() - h) / 2 + r.top;
	else if ((align & 0xff) == 2)
		p.y = r.bottom - h;
	return p;
}

// The original rounds with fadd/fsub 0.5 by the sign, then __ftol.
static int roundAway(double v) {
	return (int)(v > 0.0 ? v + 0.5 : v - 0.5);
}

// FUN_004827c0: the minute stamps of the word list
static uint32 minutesNow() {
	return FlaaklypaEngine::profileTime() / 60 - kMinuteBase;
}

// FUN_00483e30: the first of the ':' separated synonyms (FUN_00483e80
// counts them, the count is not used)
static Common::String firstSynonym(const Common::String &s) {
	const char *colon = strchr(s.c_str(), ':');
	return colon ? Common::String(s.c_str(), colon) : s;
}

SynonymScene::SynonymScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def),
	_buttons(this), _keyColor(0), _state(kStateIdle), _stateTime(0), _lastTick(0), _levelStart(0), _level(0),
	_wordIndex(0), _correct(0), _levelHints(0), _hints(0), _score(0), _placedCount(0),
	_frameCorrect(0), _frameTime(0), _frameClock(0), _dragLetter(-1), _testLevel(0), _testTime(0), _testSolve(0) {
	for (int i = 0; i < kMaxLetters; i++)
		_order[i] = _placed[i] = 0;
}

// ---- data (FUN_00482b70) ---------------------------------------------------------

// The order of the word list (FUN_00482f00): difficulty, length, then the
// word (strcmp on unsigned bytes)
bool SynonymScene::wordLess(const Word &a, const Word &b) {
	if (a.level != b.level)
		return a.level < b.level;
	if (a.length != b.length)
		return a.length < b.length;
	const byte *p = (const byte *)a.word.c_str(), *q = (const byte *)b.word.c_str();
	while (*p && *p == *q) {
		p++;
		q++;
	}
	return *p < *q;
}

// LAB_004826a0: the pool by its key
bool SynonymScene::keyLess(const Word *a, const Word *b) {
	return a->key < b->key;
}

// synonym.ini [dictionary]: word<n> (the word to build), level<n> (its
// difficulty 0..2), synonym<n> (':' separated, the first one is shown).
void SynonymScene::loadWords() {
	_words.clear();
	Common::INIFile ini;
	ini.suppressValuelessLineWarning();
	if (!resources()->loadIni(_name, "synonym.ini", ini)) {
		warning("Synonym: synonym.ini missing");
		return;
	}
	// The original looks each key up by name (FUN_0040e2d0); a map spares
	// walking the 15000 keys of the section for every one of them.
	Common::HashMap<Common::String, Common::String, Common::IgnoreCase_Hash, Common::IgnoreCase_EqualTo> keys;
	const Common::INIFile::SectionKeyList list = ini.getKeys("dictionary");
	for (const auto &kv : list)
		if (!keys.contains(kv.key))
			keys[kv.key] = Resources::iniValue(kv.value);

	// FUN_0047f360: word0, word1, ... up to the first one missing
	int n = 0;
	while (keys.contains(Common::String::format("word%d", n)))
		n++;

	// FUN_00411e70(0, times, n * 4): the minute each word was last given,
	// by its number (a 0x402 record of the profile; zero without one)
	Common::Array<byte> times;
	times.resize(n * 4, 0);
	if (n && _vm->profile())
		_vm->profile()->getRecord(_name, Profile::kRecordValue, 0, times.data(), n * 4);

	_words.resize(n);
	for (int i = 0; i < n; i++) {
		Word &w = _words[i];
		w.index = i;
		w.lastUsed = READ_LE_UINT32(&times[i * 4]);
		w.word = keys.getValOrDefault(Common::String::format("word%d", i));
		w.length = w.word.size();
		if (w.length > kMaxLetters) {
			// The original has 64 letter elements
			warning("Synonym: word %d is too long", i);
			w.length = kMaxLetters;
		}
		w.synonyms = keys.getValOrDefault(Common::String::format("synonym%d", i));
		w.level = atoi(keys.getValOrDefault(Common::String::format("level%d", i), "0").c_str());
		w.key = 0.0f;
	}
	Common::sort(_words.begin(), _words.end(), wordLess);
	debug(1, "Synonym: %d words", n);
}

// level.ini [level]: per level <l>_minlength_<d> / <l>_maxlength_<d> (the
// word lengths of each difficulty d), numcorrect, wordtime, hintcount,
// totaltime, basepoints, secpoints.
void SynonymScene::loadLevels() {
	_levels.clear();
	Common::INIFile ini;
	if (!resources()->loadIni(_name, "level.ini", ini)) {
		warning("Synonym: level.ini missing");
		return;
	}
	// FUN_0040e3a0: an integer with a default
	auto get = [&ini](const Common::String &key, int def) {
		Common::String value;
		return ini.getKey(key, "level", value) ? atoi(Resources::iniValue(value).c_str()) : def;
	};

	// FUN_00482f60: where the words of each difficulty are in the sorted
	// list. The original stores the end of difficulty 2 in the first count
	// (+0xc), so difficulty 0 spans the whole list; the length limits of
	// level.ini keep its words within difficulty 0 all the same.
	const int nw = _words.size();
	int start[kDifficulties], count[kDifficulties];
	int i = 0;
	start[0] = 0;
	while (i < nw && _words[i].level <= 0)
		i++;
	start[1] = i;
	count[0] = i - start[0];
	while (i < nw && _words[i].level <= 1)
		i++;
	start[2] = i;
	count[1] = i - start[1];
	while (i < nw && _words[i].level <= 2)
		i++;
	count[0] = i;
	count[2] = i - start[2];

	// FUN_0047f360: the levels up to the first without "<l>_totaltime"
	for (int n = 0; ini.hasKey(Common::String::format("%d_totaltime", n), "level"); n++) {
		Level l;
		l.totalTime = get(Common::String::format("%d_totaltime", n), 40000);
		for (int d = 0; d < kDifficulties; d++) {
			l.minLength[d] = get(Common::String::format("%d_minlength_%d", n, d), 0);
			l.maxLength[d] = get(Common::String::format("%d_maxlength_%d", n, d), 0);
		}
		l.numCorrect = get(Common::String::format("%d_numcorrect", n), 0xf);
		l.wordTime = get(Common::String::format("%d_wordtime", n), 10000);
		l.hintCount = get(Common::String::format("%d_hintcount", n), 0);
		l.basePoints = get(Common::String::format("%d_basepoints", n), 0);
		l.secPoints = get(Common::String::format("%d_secpoints", n), 0);
		// FUN_00483010: the words of each difficulty within the lengths
		// (sorted by length there)
		for (int d = 0; d < kDifficulties; d++) {
			int w = start[d];
			const int end = start[d] + count[d];
			while (w < end && _words[w].length < l.minLength[d])
				w++;
			l.start[d] = w;
			while (w < end && _words[w].length <= l.maxLength[d])
				w++;
			l.count[d] = w - l.start[d];
		}
		debug(1, "Synonym: level %d: %d+%d+%d words, %d to solve in %d ms", n, l.count[0], l.count[1], l.count[2],
		      l.numCorrect, l.totalTime);
		_levels.push_back(l);
	}
}

// FUN_00483120 (close): the minute stamps back into the profile
void SynonymScene::saveWordTimes() {
	if (_words.empty() || !_vm->profile())
		return;
	const uint32 size = _words.size() * 4;
	Common::Array<byte> times, old;
	times.resize(size, 0);
	old.resize(size, 0);
	for (const Word &w : _words)
		WRITE_LE_UINT32(&times[w.index * 4], w.lastUsed);
	// FUN_00411e40(0, times, size); the profile is only marked when it changed
	if (_vm->profile()->getRecord(_name, Profile::kRecordValue, 0, old.data(), size) && !memcmp(old.data(), times.data(), size))
		return;
	_vm->profile()->setRecord(_name, Profile::kRecordValue, 0, times.data(), size);
	_vm->profileChanged();
}

// FUN_00482570: the levels past the last one repeat it
const SynonymScene::Level &SynonymScene::level() const {
	int i = _level < 0 ? 0 : MIN<int>(_level, _levels.size() - 1);
	return _levels[i];
}

// FUN_004827a0
SynonymScene::Word &SynonymScene::currentWord() {
	return *_pool[_wordIndex];
}

// FUN_0047ec00: a random offset of up to a quarter of the spread of the
// keys plus ten minutes, either way
double SynonymScene::poolNoise(float lo, float hi) {
	const int r = (int)_vm->getRandomNumber(199) - 100;   // FUN_0040c550(-100, 100)
	return (double)r * (double)0.01f * (((double)hi - (double)lo) * (double)0.25f + (double)10.0f);
}

// FUN_004825b0: the words of a level in the order they are given: by the
// minute they were last given plus the noise, so words not given yet come
// first in random order, then the ones given longest ago.
int SynonymScene::buildPool(const Level &l) {
	_pool.clear();
	for (int d = 0; d < kDifficulties; d++)
		for (int i = l.start[d]; i < l.start[d] + l.count[d]; i++) {
			_words[i].key = (float)_words[i].lastUsed;
			_pool.push_back(&_words[i]);
		}
	if (_pool.empty())
		return 0;
	// The original uses qsort (not stable), so the order of equal keys
	// differs; so do the random numbers.
	Common::sort(_pool.begin(), _pool.end(), keyLess);
	const float lo = _pool[0]->key, hi = _pool.back()->key;
	for (Word *w : _pool)
		w->key = (float)(poolNoise(lo, hi) + (double)w->key);
	Common::sort(_pool.begin(), _pool.end(), keyLess);
	return _pool.size();
}

// ---- screen (FUN_00482320 / FUN_00482960) -----------------------------------------

void SynonymScene::onInit(int arg) {
	_keyColor = _vm->_screen->format.RGBToColor(0, 0xff, 0);
	playMusic("subgame21");
	initScreen();
	loadWords();
	loadLevels();
	_state = kStateIdle;
	_lastTick = _vm->getGameMillis();

	_testLevel = ConfMan.hasKey("synonym_level") ? ConfMan.getInt("synonym_level") : 0;
	_testTime = ConfMan.hasKey("synonym_time") ? ConfMan.getInt("synonym_time") : 0;
	_testSolve = ConfMan.hasKey("synonym_solve") ? ConfMan.getInt("synonym_solve") : 0;

	if (_words.empty() || _levels.empty())
		return;
	// Tournament mode (0x48234d): the "tournament:PLAYERREADY" box
	// (FUN_00419660), then the game starts at once (FUN_00482380) instead
	// of showing the start ball
	if (_vm->tournament()->isActive()) {
		_vm->tournament()->showPlayerReady();
		startGame();
		return;
	}
	addAnim("start", Anim::kDefaultPos, Anim::kDefaultPos, kZStart);
}

void SynonymScene::initScreen() {
	_fontSynonym.load("Arial_23_b");
	_fontScore.load("Amerigo BT_14_");
	_font.load("Amerigo BT_10_");

	// Labels (FUN_00407a30, z 10): "Poeng" centred, "Nivå" left aligned
	addLabel("pointslabel", kPointsRect, _vm->getString("synonym:POINTS"), 0x101);
	addLabel("levellabel", kLevelRect, _vm->getString("synonym:LEVEL"), 0x001);
	// Text areas (FUN_00407930, z 10): the level number (0x4a0570, the
	// rectangle of the "Nivå" label), the score and the word shown
	const Common::Rect level = makeRect(kLevelRect), score = makeRect(kScoreRect), word = makeRect(kSynonymRect);
	defineSurfaceAnim("leveltext", level.width(), level.height(), _keyColor)->add(level.left, level.top, kZText);
	defineSurfaceAnim("scoretext", score.width(), score.height(), _keyColor)->add(score.left, score.top, kZText);
	defineSurfaceAnim("synonymtext", word.width(), word.height(), _keyColor)->add(word.left, word.top, kZText);

	// Buttons (FUN_00406ed0, z 0x1a, transparent): pass and hint have a
	// disabled bitmap, only pass is ever disabled.
	_buttons.setDefaultZ(kZButtons);
	_buttons.add(kButtonHelp, 0, 0, "help_0", "help_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonExit, 0x2d8, 0, "exit_0", "exit_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonPass, 0x222, 0x18d, "pass_0", "pass_1", "pass_2", nullptr, "", true);
	_buttons.add(kButtonHint, 0x26, 0x18b, "hint_0", "hint_1", "hint_2", nullptr, "", true);
	_buttons.add(kButtonShuffle, 0x5e, 0xde, "shuffle_0", "shuffle_1", nullptr, nullptr, "", true);

	// The button captions, the gauges and the covers of the shelf ends
	addAnim("hint", Anim::kDefaultPos, Anim::kDefaultPos, kZText);
	addAnim("pass", Anim::kDefaultPos, Anim::kDefaultPos, kZText);
	addAnim("correct", Anim::kDefaultPos, Anim::kDefaultPos, kZText);
	addAnim("time", Anim::kDefaultPos, Anim::kDefaultPos, kZText);
	addAnim("clock", Anim::kDefaultPos, Anim::kDefaultPos, kZText);
	_frameCorrect = _frameTime = _frameClock = 0;
	addAnim("overlayleft", Anim::kDefaultPos, Anim::kDefaultPos, kZOverlay);
	addAnim("overlayright", Anim::kDefaultPos, Anim::kDefaultPos, kZOverlay);
	addAnim("overlayfunnel", Anim::kDefaultPos, Anim::kDefaultPos, kZOverlay);

	// The letter balls: elements whose bitmap (the original's element name)
	// changes, hotspot 100 + the letter's place in the word
	for (int i = 0; i < kMaxLetters; i++)
		defineAnim(letterName(i).c_str(), false, true, kHotspotFirstLetter + i, 0, 0, 0);
	// The sound effects (FUN_00438250 plays clones of audio only clips)
	static const char *const kSounds[] = {
		"sndballfall1", "sndballfall2", "sndbutton1", "sndbutton2", "sndclick", "sndclocktick",
		"sndcorrect", "sndmiss", "sndnohint", "sndstartball", "sndwrong"
	};
	for (int i = 0; i < ARRAYSIZE(kSounds); i++)
		defineAnim(kSounds[i], true, false, 0, 0, 0, 0, false);
}

// FUN_004830b0
void SynonymScene::onClose() {
	saveWordTimes();
}

// FUN_00407a30: a label as large as its text, placed in the rectangle
void SynonymScene::addLabel(const char *name, const int *rect, const Common::String &text, int align) {
	const int w = _font.stringWidth(text), h = _font.height();
	const Common::Point p = alignIn(w, h, makeRect(rect), align);
	Anim *a = defineSurfaceAnim(name, MAX(w, 1), MAX(h, 1), _keyColor);
	_font.drawString(*a->surface(), 0, 0, text);
	a->add(p.x, p.y, kZText);
}

// FUN_004079c0: the area cleared and the text placed in it
void SynonymScene::drawText(const char *area, const BitmapFont &font, const Common::String &text, int align) {
	Graphics::ManagedSurface *s = anim(area)->surface();
	s->fillRect(Common::Rect(0, 0, s->w, s->h), _keyColor);
	const Common::Point p = alignIn(font.stringWidth(text), font.height(), Common::Rect(0, 0, s->w, s->h), align);
	font.drawString(*s, p.x, p.y, text);
}

// FUN_00407880
void SynonymScene::clearText(const char *area) {
	Graphics::ManagedSurface *s = anim(area)->surface();
	s->fillRect(Common::Rect(0, 0, s->w, s->h), _keyColor);
}

// FUN_004826e0
void SynonymScene::showScore() {
	drawText("scoretext", _fontScore, Common::String::format("%d", _score), 0x101);
}

// FUN_004824a0: a gauge clip shows the frame of v (0..1): round(v * (frames
// - 1)), at least 1 (frame 0 is empty). Returns true when the frame changed.
bool SynonymScene::setClipFrame(const char *name, int &current, float v) {
	if (v < 0.0f)
		v = 0.0f;
	else if (1.0f < v)
		v = 1.0f;
	Anim *a = anim(name);
	const int frames = a->frameCount();
	int frame = roundAway((double)(frames - 1) * (double)v);
	if (frame < 1)
		frame = 1;
	else if (frames < frame)
		frame = frames;
	if (frame == current)
		return false;
	current = frame;
	// SmackGoto (FUN_00403770) with the frame as a 0 based number, like
	// the tvroom clock
	a->showFrame(frame);
	return true;
}

// ---- game ----------------------------------------------------------------------

// FUN_00482380: the start ball was clicked
void SynonymScene::startGame() {
	if (_words.empty() || _levels.empty())
		return;
	_vm->setGameRunning(true);
	if (isAnimAdded("start"))
		removeAnim("start");
	_hints = 0;
	_levelHints = 0;
	_score = 0;
	showScore();
	// The first level is 0 (the synonym_level aid's value), in a tournament
	// the player's level x 2 (0x4823c4: FUN_004194a0, lea edx, [ecx + ecx])
	int level = _testLevel;
	if (_vm->tournament()->isActive())
		level = _vm->tournament()->playerLevel() * 2;
	debug(1, "Synonym: the game starts at level %d", level);
	startLevel(level);
	nextWord();
}

// FUN_00482400
void SynonymScene::startLevel(int lvl) {
	_level = lvl;
	buildPool(level());
	drawText("leveltext", _font, Common::String::format("%d", lvl + 1), 0x201);
	_wordIndex = 0;
	_correct = 0;
	_levelHints = 0;
	_levelStart = _vm->getGameMillis();
	setClipFrame("correct", _frameCorrect, 0.0f);
	setClipFrame("time", _frameTime, 1.0f);
	setClipFrame("clock", _frameClock, 1.0f);
	debug(1, "Synonym: level %d (table %d), %d words, %d to solve, %d ms, %d hints, %d points per letter, %d per 2 s",
	      lvl, MIN<int>(MAX(lvl, 0), _levels.size() - 1), (int)_pool.size(), level().numCorrect, level().totalTime,
	      level().hintCount, level().basePoints, level().secPoints);
}

// FUN_00482720: the next word of the pool
void SynonymScene::nextWord() {
	if (_pool.empty())
		return;
	Word &w = currentWord();
	w.lastUsed = minutesNow();
	_hints = 0;
	clearText("synonymtext");
	_buttons.setState(kButtonPass, SceneButtons::kStateDisabled);
	setupLetters();
	setState(kStateWaiting);
	playAnim("ballstart1");
}

// FUN_00482780
void SynonymScene::setState(int state) {
	_stateTime = _vm->getGameMillis();
	_state = state;
}

// FUN_00484000: the letters of a word have gone
void SynonymScene::wordDone() {
	_levelHints += _hints;
	// FUN_00484090
	if (level().numCorrect <= _correct) {
		addBonus();
		startLevel(_level + 1);
	}
	_wordIndex++;
	// FUN_00484070: the pool's size; a new level starts at its second word
	const Level &l = level();
	if (l.count[0] + l.count[1] + l.count[2] <= _wordIndex)
		_wordIndex = 0;
	nextWord();
}

// FUN_004840b0: the level bonus. The help promises points for the time
// left; the original counts the seconds spent on the level.
void SynonymScene::addBonus() {
	int32 elapsed = (int32)(_vm->getGameMillis() - _levelStart);
	if (elapsed < 0)
		elapsed = 0;
	const int32 bonus = ((elapsed / 1000) * 2 / (_levelHints + 1)) * level().secPoints;
	_score += bonus;
	showScore();
	debug(1, "Synonym: level %d done after %d ms with %d hints: bonus %d, score %d", _level, elapsed, _levelHints, bonus, _score);
}

// FUN_00483eb0: the clock has rung
void SynonymScene::gameOver() {
	_vm->setGameRunning(false);
	setState(kStateIdle);
	debug(1, "Synonym: game over at level %d with %d points", _level, _score);
	// FUN_0040c350 / FUN_0040c260: a letter being dragged is dropped
	if (_dragLetter >= 0) {
		const Common::Point m = _vm->mousePos();
		endDrag(m.x, m.y);
	}
	const bool tournament = _vm->tournament()->isActive();
	// In a tournament the score goes to it, which moves on to its next player
	registerScoreNoCarParts(_vm, "", _score, kMedals);
	// ... and the start ball does not come back (0x483ee4)
	if (!tournament)
		addAnim("start", Anim::kDefaultPos, Anim::kDefaultPos, kZStart);
}

// FUN_004843f0: the time gauge and the clock run down over the level's
// time; false when it is over.
bool SynonymScene::updateClock(uint32 now) {
	const int total = _testTime > 0 ? _testTime : level().totalTime;
	if (total <= 0)
		return false;
	const double f = (double)(int32)(now - _levelStart) / (double)total;
	if (1.0 < f)
		return false;
	const float v = (float)(1.0 - f);
	setClipFrame("time", _frameTime, v);
	if (setClipFrame("clock", _frameClock, v))
		playSound("sndclocktick");
	return true;
}

// FUN_00484470: pass is enabled the level's word time after the letters
// have arrived
void SynonymScene::updatePass(uint32 now) {
	if (_buttons.state(kButtonPass) == SceneButtons::kStateDisabled && level().wordTime < (int32)(now - _stateTime))
		_buttons.setState(kButtonPass, SceneButtons::kStateNormal);
}

// FUN_00484100: every 133 ms another letter (in the shuffled order) appears
// at x 518 and rolls left to its place on the upper shelf. False once all
// are there.
bool SynonymScene::rollIn(uint32 now, float dt) {
	const int len = currentWord().length;
	int moving = 0;
	int n = (int32)(now - _stateTime) / kRollInterval + 1;
	if (n < 1)
		n = 1;
	else if (len < n)
		n = len;
	const float d = (float)((double)dt * (double)kRollSpeed);
	for (int k = 0; k < n; k++) {
		const int i = _order[k];
		Letter &l = _letters[i];
		if (!letterAdded(i)) {
			showLetter(i, false);
			l.fx = kRollStartX;
		}
		l.fx = l.fx - d;
		Common::Point p = upperSlot(i);
		if ((float)p.x < l.fx) {
			p.x = roundAway(l.fx);
			moving++;
		}
		moveLetter(i, p.x, p.y);
	}
	return !(len <= n && moving < 1);
}

// FUN_00484260: the letters of the lower shelf roll right and fall into
// the funnel (one of two sounds each), those of the upper shelf roll left
// under its cover. False once all are gone.
bool SynonymScene::fall(float dt) {
	const int len = currentWord().length;
	int moving = 0;
	for (int i = 0; i < len; i++) {
		if (!letterAdded(i))
			continue;
		Letter &l = _letters[i];
		if (isLocked(i) || placedIndex(i) >= 0) {
			const double d = (double)dt * (double)kFallSpeed;
			l.fx = (float)(d + (double)l.fx);
			if ((float)kLowerShelf[2] <= l.fx)
				l.fy = (float)(d + (double)l.fy);
			if (l.fy < kFunnelEnd) {
				moveLetter(i, roundAway(l.fx), roundAway(l.fy));
				moving++;
			} else {
				playSound("sndballfall%d", 2);
				removeLetter(i);
			}
		} else {
			l.fx = (float)((double)l.fx - (double)dt * (double)kRollSpeed);
			if (kUpperEnd < l.fx) {
				moveLetter(i, roundAway(l.fx), roundAway(l.fy));
				moving++;
			} else {
				removeLetter(i);
			}
		}
	}
	return moving > 0;
}

// ---- letters ---------------------------------------------------------------------

Common::String SynonymScene::letterName(int i) const {
	return Common::String::format("letter%d", i);
}

Anim *SynonymScene::letterAnim(int i) {
	return anim(letterName(i).c_str());
}

bool SynonymScene::letterAdded(int i) {
	return letterAnim(i)->isAdded();
}

// FUN_004833a0 (large) / FUN_00483800 (small): the element shows the ball
// of its letter, re-added at its position and z when the bitmap changes.
// Words are Windows-1252 like the file names ("large_\xe6" for æ).
void SynonymScene::showLetter(int i, bool large) {
	const Common::String bitmap = Common::String::format(large ? "large_%c" : "small_%c", currentWord().word[i]);
	Letter &l = _letters[i];
	if (letterAdded(i)) {
		if (l.bitmap == bitmap)
			return;
		removeLetter(i);
	}
	l.bitmap = bitmap;
	if (!_buttons.showBitmap(letterName(i), bitmap, true, kHotspotFirstLetter + i, l.x, l.y, l.z))
		warning("Synonym: bitmap/%s.bmp missing", bitmap.c_str());
}

// FUN_00412cc0
void SynonymScene::moveLetter(int i, int x, int y) {
	_letters[i].x = x;
	_letters[i].y = y;
	letterAnim(i)->setPos(x, y);
}

void SynonymScene::removeLetter(int i) {
	if (letterAdded(i))
		letterAnim(i)->remove();
}

// FUN_00412d50
void SynonymScene::setLetterZ(int i, int z) {
	_letters[i].z = z;
	if (letterAdded(i))
		letterAnim(i)->setZ(z);
}

// FUN_00482920
void SynonymScene::removeLetters() {
	for (int i = 0; i < kMaxLetters; i++)
		removeLetter(i);
	_placedCount = 0;
}

// FUN_004827e0: the letter elements of a new word
void SynonymScene::setupLetters() {
	removeLetters();
	shuffleLetters();
}

// FUN_00482830: up to five tries for an order that does not spell the word
void SynonymScene::shuffleLetters() {
	for (int tries = 0; tries < 5; tries++) {
		randomOrder(currentWord().length);
		if (!isUnshuffled())
			return;
	}
}

// FUN_00482860: a random permutation of 0..n-1 (rand() % left, without
// repeats)
void SynonymScene::randomOrder(int n) {
	int left[kMaxLetters];
	for (int i = 0; i < n; i++)
		left[i] = i;
	int count = n;
	for (int k = 0; k < n; k++) {
		const int r = (int)_vm->getRandomNumber(count - 1);
		_order[k] = left[r];
		// FUN_0040a0b0
		for (int j = r; j < count - 1; j++)
			left[j] = left[j + 1];
		count--;
	}
}

// FUN_004828c0: the shuffled letters spell the word
bool SynonymScene::isUnshuffled() {
	const Word &w = currentWord();
	for (int k = 0; k < w.length; k++)
		if (w.word[_order[k]] != w.word[k])
			return false;
	return true;
}

// FUN_00483250 / FUN_00483290: the letter of a hotspot, -1 for none
int SynonymScene::letterAt(int hotspot) {
	const int i = hotspot - kHotspotFirstLetter;
	if (i < 0 || i >= kMaxLetters || i >= currentWord().length)
		return -1;
	return i;
}

// FUN_004832b0: where a letter is on the lower shelf, -1 if not there
int SynonymScene::placedIndex(int letter) const {
	for (int k = 0; k < _placedCount; k++)
		if (_placed[k] == letter)
			return k;
	return -1;
}

// FUN_0040a0b0
void SynonymScene::removePlaced(int index) {
	for (int k = index; k < _placedCount - 1; k++)
		_placed[k] = _placed[k + 1];
	_placedCount--;
}

// FUN_00483360: the hints reveal the letters before first and from last
// on (first, last, second, second to last, ...); returns the number of
// letters left to place.
int SynonymScene::hintRange(int &first, int &last) {
	first = _hints / 2 + _hints % 2;
	last = currentWord().length - _hints / 2;
	return last - first;
}

// FUN_004832e0: a letter revealed by a hint
bool SynonymScene::isLocked(int letter) {
	int first, last;
	hintRange(first, last);
	return letter < first || (letter >= last && letter < currentWord().length);
}

// FUN_004836f0 / FUN_00483740: a letter's place on the upper shelf, by its
// place in the shuffled order (the word's length in small balls, centred)
Common::Point SynonymScene::upperSlot(int letter) {
	const int len = currentWord().length;
	int slot = -1;
	for (int k = 0; k < len; k++)
		if (_order[k] == letter) {
			slot = k;
			break;
		}
	Common::Point p = alignIn(len * kSmallSize, kSmallSize, makeRect(kUpperShelf), 0x101);
	p.x += slot * kSmallSize;
	return p;
}

// FUN_004835a0: the word's length in large balls centred on the lower
// shelf; free: without the places of the revealed letters
Common::Rect SynonymScene::wordRect(bool free) {
	const int len = currentWord().length;
	const Common::Point p = alignIn(len * kLargeSize, kLargeSize, makeRect(kLowerShelf), 0x101);
	Common::Rect r(p.x, p.y, p.x + len * kLargeSize, p.y + kLargeSize);
	if (free) {
		int first, last;
		hintRange(first, last);
		r.left += first * kLargeSize;
		r.right += (last - len) * kLargeSize;
	}
	return r;
}

// FUN_00483460: the revealed letters in their places on the lower shelf,
// the placed ones side by side centred between them
void SynonymScene::layoutLower() {
	const int len = currentWord().length;
	int first, last;
	hintRange(first, last);
	const Common::Rect r = wordRect(false);
	for (int i = 0; i < first; i++) {
		showLetter(i, true);
		moveLetter(i, r.left + i * kLargeSize, r.top);
	}
	for (int i = last; i < len; i++) {
		showLetter(i, true);
		moveLetter(i, r.left + i * kLargeSize, r.top);
	}
	Common::Point p = alignIn(_placedCount * kLargeSize, kLargeSize, wordRect(true), 0x101);
	for (int k = 0; k < _placedCount; k++) {
		showLetter(_placed[k], true);
		moveLetter(_placed[k], p.x, p.y);
		p.x += kLargeSize;
	}
}

// FUN_00483770: where a letter dropped at x goes among the placed ones
// (the nearest gap)
int SynonymScene::dropIndex(int x) {
	const Common::Point p = alignIn(_placedCount * kLargeSize, kLargeSize, wordRect(true), 0x101);
	const int i = roundAway((double)(x - p.x) * (double)0.025f);
	if (i < 0)
		return 0;
	return MIN(i, _placedCount);
}

// FUN_00483a60: the start positions of the falling letters
void SynonymScene::savePositions() {
	const int len = currentWord().length;
	for (int i = 0; i < len; i++) {
		_letters[i].fx = (float)_letters[i].x;
		_letters[i].fy = (float)_letters[i].y;
	}
}

// ---- actions ------------------------------------------------------------------------

// FUN_00483640 (event 0x10b): a letter dropped off the lower shelf goes
// back to its place above; else it is inserted and the word checked.
void SynonymScene::drop(int letter, int x, int y) {
	setLetterZ(letter, kZDropped);
	if (!makeRect(kLowerShelf).contains(x, y)) {
		const Common::Point p = upperSlot(letter);
		showLetter(letter, false);
		moveLetter(letter, p.x, p.y);
		playSound("sndmiss");
		return;
	}
	// FUN_0040a050
	const int index = dropIndex(x);
	for (int k = _placedCount; k > index; k--)
		_placed[k] = _placed[k - 1];
	_placed[index] = letter;
	_placedCount++;
	layoutLower();
	if (_state == kStatePlaying)
		check();
}

// FUN_004838c0: a click while letters are missing, the wrong sound for a
// wrong word; a right word scores and the letters go after blank1 (1 s).
bool SynonymScene::check() {
	int first, last;
	if (_placedCount != hintRange(first, last)) {
		playSound("sndclick");
		return false;
	}
	if (!isSolved()) {
		playSound("sndwrong");
		debug(1, "Synonym: wrong");
		return false;
	}
	playSound("sndcorrect");
	addWordScore();
	savePositions();
	setState(kStateWaiting);
	playAnim(kBlank1);
	return true;
}

// FUN_004839c0: the placed letters spell the word between the revealed
// ones (letters, not elements: equal letters are interchangeable)
bool SynonymScene::isSolved() {
	const Word &w = currentWord();
	int first, last;
	const int n = hintRange(first, last);
	if (n != _placedCount)
		return false;
	for (int k = 0; k < n; k++)
		if (w.word[_placed[k]] != w.word[first + k])
			return false;
	return true;
}

// FUN_00483950: basepoints per letter, halved by each hint
void SynonymScene::addWordScore() {
	const Level &l = level();
	const Word &w = currentWord();
	const int32 points = (l.basePoints * w.length) / (1 << (_hints & 0x1f));
	_score += points;
	_correct++;
	showScore();
	setClipFrame("correct", _frameCorrect, (float)_correct / (float)l.numCorrect);
	debug(1, "Synonym: right, %d points with %d hints, score %d, %d of %d", points, _hints, _score, _correct, l.numCorrect);
}

// FUN_00483aa0 (event 0x116)
void SynonymScene::onButton(int id) {
	switch (id) {
	case kButtonPass:
		playSound("sndbutton%d", 2);
		pass();
		break;
	case kButtonHelp:
		_vm->showHelp();
		break;
	case kButtonExit:
		_vm->endGame();
		break;
	case kButtonShuffle:
		shuffle();
		break;
	case kButtonHint:
		if (hint())
			playSound("sndbutton%d", 2);
		else
			playSound("sndnohint");
		break;
	default:
		break;
	}
}

// FUN_00483b40: the answer is shown on the lower shelf and the letters go
// (no points)
void SynonymScene::pass() {
	if (_state != kStatePlaying)
		return;
	int first, last;
	_placedCount = hintRange(first, last);
	for (int k = 0; k < _placedCount; k++)
		_placed[k] = first + k;
	layoutLower();
	savePositions();
	setState(kStateWaiting);
	playAnim(kBlank1);
	debug(1, "Synonym: passed");
}

// FUN_00483bc0: one more letter revealed, up to the level's hint count and
// never the whole word
bool SynonymScene::hint() {
	if (_state != kStatePlaying)
		return false;
	if (level().hintCount <= _hints)
		return false;
	if (currentWord().length <= _hints + 1)
		return false;
	_hints++;
	unplaceLocked();
	layoutLower();
	check();
	return true;
}

// FUN_00483c20: revealed letters leave the placed ones
void SynonymScene::unplaceLocked() {
	const int len = currentWord().length;
	int first, last;
	hintRange(first, last);
	for (int i = 0; i < len; i++) {
		if (i >= first && i < last)
			continue;
		const int k = placedIndex(i);
		if (k >= 0)
			removePlaced(k);
	}
}

// FUN_00483cf0: a new order for the letters still on the upper shelf
void SynonymScene::shuffle() {
	if (_state != kStatePlaying)
		return;
	shuffleLetters();
	int first, last;
	hintRange(first, last);
	for (int i = first; i < last; i++)
		if (placedIndex(i) < 0) {
			const Common::Point p = upperSlot(i);
			moveLetter(i, p.x, p.y);
		}
}

// FUN_00438250: an audio only clip, played as a clone so it may overlap;
// here the clip starts again
void SynonymScene::playSound(const Common::String &name) {
	Anim *a = defineAnim(name.c_str(), true, false, 0, 0, 0, 0, false);
	if (a->isAdded())
		a->remove();
	a->play();
	a->setRemoveWhenDone(true);
}

// FUN_00483b10: one of the numbered variants
void SynonymScene::playSound(const char *format, int variants) {
	playSound(Common::String::format(format, (int)_vm->getRandomNumber(variants - 1) + 1));
}

// ---- drag and drop -------------------------------------------------------------------

// FUN_0040c1f0(element, 0)
void SynonymScene::startDrag(int letter, int x, int y) {
	_dragLetter = letter;
	_dragMouse = Common::Point(x, y);
}

// FUN_0040c300: the element follows the mouse
void SynonymScene::followDrag(int x, int y) {
	if (x == _dragMouse.x && y == _dragMouse.y)
		return;
	const Letter &l = _letters[_dragLetter];
	moveLetter(_dragLetter, l.x + x - _dragMouse.x, l.y + y - _dragMouse.y);
	_dragMouse = Common::Point(x, y);
}

// FUN_0040c260: a last move, then the drop (event 0x10b)
void SynonymScene::endDrag(int x, int y) {
	followDrag(x, y);
	const int letter = _dragLetter;
	_dragLetter = -1;
	drop(letter, x, y);
}

// ---- events ------------------------------------------------------------------------

// FUN_004831d0 (event 0x105), then the BUTTON module's FUN_00406b50
void SynonymScene::onMouseDown(int hotspot, int x, int y) {
	if (hotspot == kHotspotStart) {
		startGame();
	} else if (_state == kStatePlaying && _dragLetter < 0) {
		const int i = letterAt(hotspot);
		if (i >= 0 && !isLocked(i)) {
			showLetter(i, true);
			startDrag(i, x, y);
			setLetterZ(i, kZDragged);
			const int k = placedIndex(i);
			if (k >= 0) {
				removePlaced(k);
				layoutLower();
			}
		}
	}
	_buttons.mouseDown(hotspot, x, y);
}

// Event 0x106 goes to the BUTTON module (a click is event 0x116), then the
// EVENT module ends a drag with event 0x10b (FUN_0040c770).
void SynonymScene::onMouseUp(int hotspot, int x, int y) {
	const int id = _buttons.mouseUp();
	if (id)
		onButton(id);
	if (_dragLetter >= 0)
		endDrag(x, y);
}

void SynonymScene::onMouseMove(int hotspot, int x, int y) {
	_buttons.setHover(hotspot);
}

// FUN_00483d80 (event 0x110)
void SynonymScene::onAnimFinished(Anim *anim) {
	const char *name = anim->name();
	if (!strcmp(name, "ballstart1")) {
		// The balls have gone into the machine: the word shows and the
		// letters of its synonym come out.
		drawText("synonymtext", _fontSynonym, firstSynonym(currentWord().synonyms), 0x101);
		setState(kStateRolling);
		playAnim("ballstart2");
		removeAnim("ballstart1");
	}
	if (!strcmp(name, "ballstart2"))
		removeAnim("ballstart2");
	if (!strcmp(name, kBlank1)) {
		setState(kStateFalling);
		playAnim("chain");
		removeAnim(kBlank1);
	}
	if (!strcmp(name, "clockring")) {
		removeAnim("clockring");
		gameOver();
	}
}

// FUN_00483f10 (event 0x112, every frame)
void SynonymScene::onUpdate() {
	if (_dragLetter >= 0) {
		const Common::Point m = _vm->mousePos();
		followDrag(m.x, m.y);
	}

	const uint32 now = _vm->getGameMillis();
	const float dt = (float)((double)(int32)(now - _lastTick) * (double)0.001f);
	bool running = false;
	if (_state != kStateIdle)
		running = updateClock(now);
	switch (_state) {
	case kStatePlaying:
		updatePass(now);
		if (!running) {
			debug(1, "Synonym: time is up");
			playAnim("clockring");
			setState(kStateWaiting);
		} else if (_testSolve > 0 && _dragLetter < 0 && (int32)(now - _stateTime) >= _testSolve) {
			autoSolve();
		}
		break;
	case kStateRolling:
		if (!rollIn(now, dt)) {
			playSound("sndstartball");
			setState(kStatePlaying);
			logWord();
		}
		break;
	case kStateFalling:
		if (!fall(dt)) {
			if (isAnimAdded("chain"))
				removeAnim("chain");
			wordDone();
		}
		break;
	default:
		break;
	}
	_lastTick = now;
}

// ---- development aids ------------------------------------------------------------

// Debug level 1: the word and where its letters are (centres, in the
// word's order) for scripted drags (autoclick "d" / "u").
void SynonymScene::logWord() {
	if (gDebugLevel < 1)
		return;
	const Word &w = currentWord();
	Common::String places;
	for (int i = 0; i < w.length; i++)
		places += Common::String::format(" %d,%d", _letters[i].x + kSmallSize / 2, _letters[i].y + kSmallSize / 2);
	const Common::Rect r = wordRect(true);
	debug(1, "Synonym: level %d word %d/%d (difficulty %d) '%s' shows '%s'; letters at%s; lower shelf %d..%d, y %d",
	      _level, _wordIndex, (int)_pool.size(), w.level, w.word.c_str(), firstSynonym(w.synonyms).c_str(), places.c_str(),
	      r.left, r.right, r.top + kLargeSize / 2);
}

// "synonym_solve=<ms>": the letters not on the lower shelf are dragged to
// the right end of the placed ones in the word's order (mouse down on the
// ball, release at the end), which solves a word nobody has touched.
void SynonymScene::autoSolve() {
	int first, last;
	hintRange(first, last);
	const Common::Rect shelf = makeRect(kLowerShelf);
	for (int i = first; i < last && _state == kStatePlaying; i++) {
		if (placedIndex(i) >= 0)
			continue;
		const Letter &l = _letters[i];
		onMouseDown(kHotspotFirstLetter + i, l.x + kSmallSize / 2, l.y + kSmallSize / 2);
		const Common::Point p = alignIn(_placedCount * kLargeSize, kLargeSize, wordRect(true), 0x101);
		const int x = CLIP<int>(p.x + _placedCount * kLargeSize + kLargeSize / 4, shelf.left, shelf.right - 1);
		onMouseUp(0, x, shelf.top + kLargeSize);
	}
}

} // End of namespace Flaaklypa
