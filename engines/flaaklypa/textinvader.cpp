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
#include "common/debug.h"
#include "common/stream.h"
#include "common/system.h"
#include "common/textconsole.h"

#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/resources.h"
#include "flaaklypa/dialog.h"
#include "flaaklypa/hiscore.h"
#include "flaaklypa/textinvader.h"

namespace Flaaklypa {

// Per difficulty tables of the original (0x4de620 ..): starting speed in
// pixels per second, speed factor per level, slow down duration, words per
// bonus word, words per level.
static const float kSpeed[3] = { 8.0f, 10.0f, 12.0f };
static const float kSpeedFactor[3] = { 1.02f, 1.03f, 1.04f };
static const int kSlowTime[3] = { 15000, 10000, 6000 };
static const int kWordsPerBonus[3] = { 10, 15, 20 };
static const int kWordsPerLevel[3] = { 12, 10, 8 };

enum {
	kZBackdropHill = 6,
	kZSign = 7,
	kZScore = 8,
	kZWord = 3,
	kZTyped = 4,
	kZStar = 5,
	kZMeter = 100,
	kSpawnDelay = 500,       ///< a new word follows a lost or finished one within this
	kClearRespawn = 6000,    ///< ... and a yellow word within this (0x6843a0)
	kRemoveDelay = 500       ///< a finished word stays this long
};

// The 4x4 sound clips are played through an invisible template element
// (" " in the scene tables) whose name the original overwrites; here each
// has a definition of its own.
static const AnimDef kSoundDefs[] = {
	{ "wrong", 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ "right", 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ "start", 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ "drop", 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

// The scene tables hold "skilt" twice: the clip (sign turning round, index
// 4) and the bitmap (the sign with the score on it). Scene::anim() finds
// the clip; this is the bitmap.
static const AnimDef kSignBitmapDef = { "skilt", 0, 1, 1, 0, 4, 0, 518, 0, 0, 0 };

TextinvaderScene::TextinvaderScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def),
	_lights(nullptr), _meter(nullptr), _signClip(nullptr), _signBitmap(nullptr), _scoreText(nullptr),
	_running(false), _starting(false), _startTime(0), _lastTick(0), _timerSeq(0), _highlighted(0),
	_difficulty(0), _slotCount(1), _maxLen(1), _score(0), _energy(100), _speed(8.0f), _slow(1.0f),
	_wordsTyped(0), _wordsRemoved(0), _level(0), _nextBonus(0), _bonusMilestone(0), _target(-1) {
}

TextinvaderScene::~TextinvaderScene() {
	for (auto *a : _ownAnims) {
		a->remove();
		delete a;
	}
	delete _lights;
}

// rand() % n of the original (FUN_0040c530).
int TextinvaderScene::rnd(int n) {
	if (n <= 0)
		return 0;
	return _vm->getRandomNumber(n - 1);
}

// ---- loading -------------------------------------------------------------

bool TextinvaderScene::load() {
	if (!Scene::load())
		return false;
	// FUN_0044c740: the fonts
	_fontBlue.load("Comic Sans MS_16_lightish_blue");
	_fontGreen.load("Comic Sans MS_16_lightish_green");
	_fontScore.load("Comic Sans MS_10_");
	_fontYellow.load("Comic Sans MS_16_yellow");
	_fontOrange.load("Comic Sans MS_16_orange");
	_fontRed.load("Comic Sans MS_16_red");
	_lights = resources()->loadBitmap(_name, "bitmap/julelys1.bmp", _vm->_screen->format);
	if (!_lights)
		warning("Textinvader: bitmap/julelys1.bmp missing");
	loadWordList();
	return true;
}

// FUN_0044d7e0: wordlist/wordlist.txt, one word per line, sorted by length
// (1..31 characters; longer or empty lines are dropped).
void TextinvaderScene::loadWordList() {
	Common::SeekableReadStream *s = resources()->open(_name, "wordlist/wordlist.txt");
	if (!s) {
		warning("Textinvader: wordlist/wordlist.txt missing");
		return;
	}
	while (!s->eos()) {
		Common::String line = s->readLine();
		uint cr = line.findFirstOf('\r');
		if (cr != Common::String::npos)
			line = line.substr(0, cr);
		int len = line.size();
		if (len > 0 && len < kMaxWordLen)
			_words[len].push_back(line);
	}
	delete s;
}

// FUN_0044d770: a random word of a random length up to maxLen (lengths
// without words are skipped).
const Common::String &TextinvaderScene::pickWord(int maxLen) {
	static const Common::String empty;
	bool any = false;
	for (int l = 1; l <= maxLen && l < kMaxWordLen; l++)
		if (!_words[l].empty())
			any = true;
	if (!any)
		return empty;
	// Length 32 has no words (the original's count for it is zero).
	int len;
	do {
		len = rnd(maxLen) + 1;
	} while (len >= kMaxWordLen || _words[len].empty());
	return _words[len][rnd(_words[len].size())];
}

// ---- screen --------------------------------------------------------------

// FUN_0044c740 (init handler)
void TextinvaderScene::onInit(int arg) {
	const uint32 green = _vm->_screen->format.RGBToColor(0, 255, 0);

	// The score area on the sign: rect (20, 546)-(105, 566) at z 8.
	_scoreText = defineSurfaceAnim("scoretext", 85, 20, green);
	_scoreText->add(20, 546, kZScore);

	// BUTTON module: help (id 3) at (2, 1), exit (id 2) at (726, 2).
	defineAnim("help_0", false, true, kHotspotHelp, 2, 1, 0)->add(2, 1, 0);
	defineAnim("help_1", false, true, kHotspotHelp, 2, 1, 0);
	defineAnim("exit_0", false, true, kHotspotExit, 726, 2, 0)->add(726, 2, 0);
	defineAnim("exit_1", false, true, kHotspotExit, 726, 2, 0);
	_highlighted = 0;

	addAnim("fjell", Anim::kDefaultPos, Anim::kDefaultPos, kZBackdropHill);
	addAnim("julelys2", Anim::kDefaultPos, Anim::kDefaultPos, kZSign);

	// PROGRESS_Create (FUN_004089a0): the lit chain of lights julelys1 at
	// (165, 493), z 100, horizontal, empty.
	_meter = defineSurfaceAnim("meter", _lights ? _lights->w : 350, _lights ? _lights->h : 24, green);
	_meter->add(165, 493, kZMeter);
	_starting = false;
	_running = false;

	_signClip = anim("skilt");
	if (!_signClip->isSmacker())
		warning("Textinvader: expected the skilt clip first in the scene tables");
	_signBitmap = new Anim(this, &kSignBitmapDef);
	_ownAnims.push_back(_signBitmap);
	for (int i = 0; i < kSoundCount; i++) {
		_sounds[i] = new Anim(this, &kSoundDefs[i]);
		_ownAnims.push_back(_sounds[i]);
	}
	for (int i = 0; i < kMaxSlots; i++) {
		Slot &s = _slots[i];
		s.text = defineSurfaceAnim(Common::String::format("word%d", i).c_str(), 1, 1, green);
		s.typed = defineSurfaceAnim(Common::String::format("typed%d", i).c_str(), 1, 1, green);
		s.star = new Anim(this, anim("li")->def());
		_ownAnims.push_back(s.star);
	}

	playMusic("track22");
	showStartSign();
	_lastTick = g_engine->getGameMillis();
	// TODO: profiles. With an active profile the original shows the
	// "tournament:PLAYERREADY" message box and starts the game at once
	// (FUN_0044d720(4, 0, 0)).
}

void TextinvaderScene::onClose() {
	// FUN_0044cf70 frees the fonts, buttons, word list and meter.
}

// FUN_0044c8b0: the sign that says "Start" (first frame of skilt.smk).
void TextinvaderScene::showStartSign() {
	if (_signClip->isAdded())
		_signClip->remove();
	if (_signBitmap->isAdded())
		_signBitmap->remove();
	_signClip->add(Anim::kDefaultPos, Anim::kDefaultPos, kZSign);
	Graphics::ManagedSurface *s = _scoreText->surface();
	s->fillRect(Common::Rect(0, 0, s->w, s->h), s->format.RGBToColor(0, 255, 0));
}

// FUN_0044cdf0: the blank sign the score is written on.
void TextinvaderScene::showScoreSign() {
	if (_signClip->isAdded())
		_signClip->remove();
	if (_signBitmap->isAdded())
		_signBitmap->remove();
	_signBitmap->add(Anim::kDefaultPos, Anim::kDefaultPos, kZSign);
}

void TextinvaderScene::highlightButton(int hotspot) {
	if (hotspot != kHotspotHelp && hotspot != kHotspotExit)
		hotspot = 0;
	if (hotspot == _highlighted)
		return;
	if (_highlighted == kHotspotHelp) {
		removeAnim("help_1");
		addAnim("help_0", 2, 1, 0);
	} else if (_highlighted == kHotspotExit) {
		removeAnim("exit_1");
		addAnim("exit_0", 726, 2, 0);
	}
	_highlighted = hotspot;
	if (hotspot == kHotspotHelp) {
		removeAnim("help_0");
		addAnim("help_1", 2, 1, 0);
	} else if (hotspot == kHotspotExit) {
		removeAnim("exit_0");
		addAnim("exit_1", 726, 2, 0);
	}
}

void TextinvaderScene::onMouseMove(int hotspot, int x, int y) {
	highlightButton(hotspot);
}

// 0x105 -> FUN_0044d720 and the BUTTON module's 0x116 -> FUN_0044d4d0.
void TextinvaderScene::onMouseDown(int hotspot, int x, int y) {
	if (hotspot == kHotspotExit) {
		_vm->endGame();
		return;
	}
	if (hotspot == kHotspotHelp) {
		_vm->showHelp();
		return;
	}
	if (hotspot == kHotspotSign && !_running && !_starting) {
		// The sign turns round and the lights come on.
		_startTime = g_engine->getGameMillis();
		_starting = true;
		_signClip->play();
		_sounds[kSoundStart]->play();
	}
}

// 0x110 -> FUN_0044c920: every finished clip is removed; the turning sign
// starts the game.
void TextinvaderScene::onAnimFinished(Anim *a) {
	a->remove();
	if (a == _signClip) {
		_starting = false;
		startGame();
	}
}

// ---- the game ------------------------------------------------------------

// FUN_0044c950
void TextinvaderScene::startGame() {
	_difficulty = 0; // TODO: profile setting (FUN_004194a0)
	_nextBonus = rnd(3);
	_target = -1;
	_speed = kSpeed[_difficulty];
	_wordsTyped = 0;
	_bonusMilestone = 0;
	_slow = 1.0f;
	_level = 0;
	_wordsRemoved = 0;
	_score = 0;
	_energy = 100;
	_slotCount = 1;
	_maxLen = 1;
	showScoreSign();
	_vm->setGameRunning(true);
	_running = true;
	redrawAll();
	for (int i = 0; i < kMaxSlots; i++)
		_slots[i].active = false;
	for (int i = 0; i < _slotCount; i++)
		spawnWord(i);
	updateMeter();
	_lastTick = g_engine->getGameMillis();
}

// The original's timers all carry the slot (or a code) as data and never
// replace each other; a running number keeps them apart here.
void TextinvaderScene::addTimer(uint32 delayMs, int data) {
	setTimer(_timerSeq++, delayMs, data);
}

// 0x111 -> FUN_0044cea0
void TextinvaderScene::onTimer(int id, int data) {
	if (data == kTimerSlowEnd) {
		_slow = 1.0f;
		return;
	}
	if (data >= kTimerRemove) {
		int slot = data - kTimerRemove;
		removeWord(slot);
		addTimer(rnd(kSpawnDelay), slot);
		return;
	}
	spawnWord(data);
}

// FUN_0044ca50: a new word for a slot. Every n-th word typed makes the
// next word a bonus word; the colours cycle so no two in a row are alike.
void TextinvaderScene::spawnWord(int slot) {
	if (slot < 0 || slot >= kMaxSlots)
		return;
	Slot &s = _slots[slot];
	if (s.active || !_running)
		return;
	s.done = false;

	int milestone = _wordsTyped / kWordsPerBonus[_difficulty];
	if (milestone > _bonusMilestone) {
		s.bonus = _nextBonus + 1;
		_nextBonus = (rnd(2) + 1 + _nextBonus) % 3;
		_bonusMilestone = milestone;
	} else {
		s.bonus = kBonusNone;
	}

	// No two words in the sky start with the same letter.
	for (int tries = 0; tries < 1000; tries++) {
		s.word = pickWord(_maxLen);
		bool clash = false;
		for (int i = 0; i < _slotCount; i++)
			if (i != slot && _slots[i].active && !_slots[i].word.empty() && !s.word.empty() && _slots[i].word[0] == s.word[0])
				clash = true;
		if (!clash)
			break;
	}

	s.w = _fontBlue.stringWidth(s.word);
	s.h = _fontBlue.height();

	// A random place in the top 150 lines that overlaps no other word
	// (up to a hundred tries).
	for (int tries = 0; ; ) {
		s.x = rnd(640 - s.w) + 80;
		s.y = rnd(150);
		bool ok = true;
		for (int i = 0; i < _slotCount; i++)
			if (i != slot && _slots[i].active && _slots[i].rect().intersects(s.rect()))
				ok = false;
		if (ok || tries++ > 99)
			break;
	}

	const uint32 green = _vm->_screen->format.RGBToColor(0, 255, 0);
	s.text->createSurface(MAX(s.w, 1), MAX(s.h, 1), green);
	s.text->add(s.x, s.y, kZWord);
	s.typed->createSurface(MAX(s.w, 1), MAX(s.h, 1), green);
	s.typed->add(s.x, s.y, kZTyped);
	s.fy = (float)s.y;
	s.active = true;
	s.starX = s.w / 2 - 64;
	s.starY = s.h / 2 - 64;
	s.star->add(s.x + s.starX, s.y + s.starY, kZStar);
	s.star->play();
	drawWord(slot);
	debug(1, "Textinvader: slot %d word '%s' bonus %d at %d,%d", slot, s.word.c_str(), s.bonus, s.x, s.y);
}

// FUN_0044cd30: the word in the colour of its bonus, typed prefix cleared.
void TextinvaderScene::drawWord(int slot) {
	Slot &s = _slots[slot];
	if (!s.active || s.done)
		return;
	const uint32 green = _vm->_screen->format.RGBToColor(0, 255, 0);
	Graphics::ManagedSurface *t = s.text->surface();
	t->fillRect(Common::Rect(0, 0, t->w, t->h), green);
	Graphics::ManagedSurface *p = s.typed->surface();
	p->fillRect(Common::Rect(0, 0, p->w, p->h), green);
	const BitmapFont *font = &_fontBlue;
	if (s.bonus == kBonusYellow)
		font = &_fontYellow;
	else if (s.bonus == kBonusOrange)
		font = &_fontOrange;
	else if (s.bonus == kBonusRed)
		font = &_fontRed;
	font->drawString(*t, 0, 0, s.word);
}

// FUN_0044cdb0: redraw everything and forget what was typed.
void TextinvaderScene::redrawAll() {
	for (int i = 0; i < _slotCount; i++)
		drawWord(i);
	_typed.clear();
	_target = -1;
}

// FUN_0044cf00
void TextinvaderScene::removeWord(int slot) {
	Slot &s = _slots[slot];
	if (!s.active)
		return;
	_wordsRemoved++;
	s.active = false;
	s.text->remove();
	s.typed->remove();
	if (s.star->isAdded())
		s.star->remove();
}

// FUN_0044ce40: the chain of lights shows the energy, the sign the score.
void TextinvaderScene::updateMeter() {
	if (_energy < 0)
		_energy = 0;
	// PROGRESS_Set (FUN_00408b00): the left round(width * value) columns of
	// julelys1 on a key coloured surface.
	Graphics::ManagedSurface *m = _meter->surface();
	m->fillRect(Common::Rect(0, 0, m->w, m->h), m->format.RGBToColor(0, 255, 0));
	float v = CLIP(_energy * 0.01f, 0.0f, 1.0f);
	int fill = (int)(m->w * v + 0.5f);
	if (fill > 0 && _lights)
		m->blitFrom(*_lights, Common::Rect(0, 0, fill, _lights->h), Common::Point(0, 0));

	Graphics::ManagedSurface *s = _scoreText->surface();
	s->fillRect(Common::Rect(0, 0, s->w, s->h), s->format.RGBToColor(0, 255, 0));
	_fontScore.drawStringCentred(*s, Common::Rect(0, 0, s->w, s->h), Common::String::format("%d", _score));
}

// FUN_0044d3b0: the first letter picks the word to type.
void TextinvaderScene::findTarget() {
	_targetWord.clear();
	for (int i = 0; i < _slotCount; i++) {
		const Slot &s = _slots[i];
		if (s.active && !s.done && !s.word.empty() && s.word[0] == _typed[0]) {
			_target = i;
			_targetWord = s.word;
			return;
		}
	}
}

// FUN_0044d320: 0 the typed text is a prefix of the word, 1 a mistake, 2 the whole word.
int TextinvaderScene::checkTyped() const {
	if (_targetWord.size() < _typed.size())
		return 1;
	if (_typed == _targetWord)
		return 2;
	return strncmp(_typed.c_str(), _targetWord.c_str(), _typed.size()) != 0 ? 1 : 0;
}

// 0x10c / 0x10e -> FUN_0044cfe0 / FUN_0044d030. The original takes the
// character of the key event when isalpha() or '-' (C locale: only the
// ASCII letters) and lower cases it, so words with æ, ø or å cannot be
// finished in the original either.
bool TextinvaderScene::handlesKey(const Common::KeyState &key) {
	// The handler returns 1 when FUN_0044cfe0 says the key's virtual key
	// code is a letter for isalpha() or 0x2d (VK_INSERT): A..Z and the codes
	// 0x61..0x7a, which are the keypad keys and F1..F11. So typing never
	// pauses (P) and F1 opens no help here.
	Common::KeyCode k = key.keycode;
	return (k >= Common::KEYCODE_a && k <= Common::KEYCODE_z) ||
		(k >= Common::KEYCODE_KP1 && k <= Common::KEYCODE_KP9) ||
		k == Common::KEYCODE_KP_MULTIPLY || k == Common::KEYCODE_KP_PLUS || k == Common::KEYCODE_KP_MINUS ||
		k == Common::KEYCODE_KP_PERIOD || k == Common::KEYCODE_KP_DIVIDE ||
		(k >= Common::KEYCODE_F1 && k <= Common::KEYCODE_F11) || k == Common::KEYCODE_INSERT;
}

void TextinvaderScene::onKey(const Common::KeyState &key) {
	if (!_running)
		return;
	int c = key.ascii;
	if (c >= 'A' && c <= 'Z')
		c += 'a' - 'A';
	if (!((c >= 'a' && c <= 'z') || c == '-'))
		return;

	_typed += (char)c;
	if (_typed.size() == 1)
		findTarget();
	int result = checkTyped();
	debug(1, "Textinvader: key '%c' typed '%s' target %d '%s' -> %s", c, _typed.c_str(), _target, _targetWord.c_str(),
	      result == 0 ? "ok" : result == 1 ? "wrong" : "word done");
	if (result == 0) {
		// A correct letter.
		_score += 3;
		if (_target >= 0) {
			Graphics::ManagedSurface *p = _slots[_target].typed->surface();
			p->fillRect(Common::Rect(0, 0, p->w, p->h), p->format.RGBToColor(0, 255, 0));
			_fontGreen.drawString(*p, 0, 0, _typed);
		}
	} else if (result == 1) {
		// A mistake: lose energy and start over.
		_energy -= 3;
		_sounds[kSoundWrong]->play();
		redrawAll();
	} else {
		// The whole word.
		_wordsTyped++;
		_score += (int)_typed.size() * 4;
		_sounds[kSoundRight]->play();
		Slot &s = _slots[_target];
		Graphics::ManagedSurface *p = s.typed->surface();
		p->fillRect(Common::Rect(0, 0, p->w, p->h), p->format.RGBToColor(0, 255, 0));
		_fontGreen.drawString(*p, 0, 0, _typed);
		s.done = true;
		addTimer(kRemoveDelay, kTimerRemove + _target);
		applyBonus(s.bonus);
		redrawAll();
	}
	updateMeter();
	debug(1, "Textinvader: score %d energy %d", _score, _energy);
	gameOver();
}

// FUN_0044d220
void TextinvaderScene::applyBonus(int bonus) {
	switch (bonus) {
	case kBonusYellow:
		// Everything in the sky goes; new words follow within six seconds.
		// (The original adds 3 points per word times a slot field that is
		// never set, so nothing is scored.)
		for (int i = 0; i < _slotCount; i++)
			if (_slots[i].active)
				removeWord(i);
		for (int i = 0; i < _slotCount; i++)
			addTimer(rnd(kClearRespawn), i);
		break;
	case kBonusOrange:
		_slow = 0.5f;
		addTimer(kSlowTime[_difficulty], kTimerSlowEnd);
		break;
	case kBonusRed:
		_energy += 10;
		if (_energy > 100)
			_energy = 100;
		updateMeter();
		break;
	default:
		break;
	}
}

// FUN_0044d450: out of energy.
void TextinvaderScene::gameOver() {
	if (_energy > 0)
		return;
	for (int i = 0; i < kMaxSlots; i++)
		removeWord(i);
	_vm->setGameRunning(false);
	_running = false;
	debug(1, "Textinvader: game over, %d points", _score);
	// Modal: with _running false the nested frames neither move nor spawn
	// words; onUpdate() returns once this comes back from its slot loop.
	_vm->messageBox("interfaceh:GAMEOVER", "textinvader:GAMEOVER", MessageBox::kButtonOk);
	// FUN_0041f9a0(0, score, 0x49f0a0, 0), then (not in tournament mode)
	// the start sign comes back (FUN_0044c8b0).
	static const int kMedals[4] = { 2500, 5000, 10000, 20000 };
	registerScore(_vm, "", _score, kMedals);
	showStartSign();
}

// FUN_0044d680: every n removed words is a level; odd levels add a word to
// the sky, even ones let the words get longer, all speed the fall up.
void TextinvaderScene::checkLevel() {
	int level = _wordsRemoved / kWordsPerLevel[_difficulty];
	if (level <= _level)
		return;
	if ((level & 1) == 0) {
		if (_maxLen < kMaxWordLen)
			_maxLen++;
	} else if (_slotCount < kMaxSlots) {
		int slot = _slotCount++;
		addTimer(rnd(kSpawnDelay), slot);
	}
	_speed *= kSpeedFactor[_difficulty];
	_level = level;
	debug(1, "Textinvader: level %d, %d slots, words up to %d letters, %.2f px/s", level, _slotCount, _maxLen, _speed);
}

// 0x112 -> FUN_0044d4f0: the words fall; one that reaches the ground is
// lost.
void TextinvaderScene::onUpdate() {
	uint32 now = g_engine->getGameMillis();
	float dt = (now - _lastTick) * 0.001f;
	_lastTick = now;

	if (!_running) {
		if (_starting) {
			// The lights come on while the sign turns (full after 300 ms).
			Graphics::ManagedSurface *m = _meter->surface();
			m->fillRect(Common::Rect(0, 0, m->w, m->h), m->format.RGBToColor(0, 255, 0));
			float v = CLIP((now - _startTime) * 0.00333333f, 0.0f, 1.0f);
			int fill = (int)(m->w * v + 0.5f);
			if (fill > 0 && _lights)
				m->blitFrom(*_lights, Common::Rect(0, 0, fill, _lights->h), Common::Point(0, 0));
		}
		return;
	}

	for (int i = 0; i < _slotCount; i++) {
		Slot &s = _slots[i];
		if (!s.active)
			continue;
		s.fy += _slow * _speed * dt;
		s.y = (int)s.fy;
		if (s.y < kGroundY || s.done) {
			s.text->setPos(s.x, s.y);
			s.typed->setPos(s.x, s.y);
			s.star->setPos(s.x + s.starX, s.y + s.starY);
		} else {
			_energy -= 5;
			debug(1, "Textinvader: slot %d '%s' lost, energy %d", i, s.word.c_str(), _energy);
			_sounds[kSoundDrop]->play();
			removeWord(i);
			addTimer(rnd(kSpawnDelay), i);
			if (_target == i)
				redrawAll();
			updateMeter();
			gameOver();
			if (!_running)
				return;
		}
	}
	checkLevel();
}

} // End of namespace Flaaklypa
