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
#include "common/formats/ini-file.h"
#include "common/system.h"
#include "common/textconsole.h"

#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/dialog.h"
#include "flaaklypa/lettersort.h"
#include "flaaklypa/resources.h"

namespace Flaaklypa {

// Level table at 0x4db5a0: bags to lift, seconds between bags, seconds
// between letters, countries in play. Level 10 never ends (the check at
// 0x43f2a0 stops at level 9).
const LettersortScene::LevelDef LettersortScene::kLevels_[kLevels] = {
	{  5, 14, 4,  3 },
	{ 12, 12, 4,  4 },
	{ 20, 11, 4,  5 },
	{ 29, 10, 3,  6 },
	{ 39,  9, 3,  7 },
	{ 49,  8, 2,  8 },
	{ 59,  8, 2,  9 },
	{ 69,  7, 2, 10 },
	{ 79,  7, 2, 11 },
	{  0,  6, 2, 12 }
};

enum {
	// Rectangles at 0x49ea80 (the letter floor) and 0x49ea90 (the belt area).
	kFloorLeft = 28,
	kFloorTop = 311,
	kFloorRight = 772,
	kFloorBottom = 573,
	kBeltLeft = 27,
	// The letter structs use a nominal size of 161x104 (the bitmaps are 150x97).
	kLetterW = 161,
	kLetterH = 104,
	kBagW = 84,
	kBagH = 62,
	kBagY = 126,                 ///< bags enter at (-57, 126)
	kFlagOffsetY = 47,
	kBagFallX = 614,             ///< a bag past this x falls off the belt
	kBeltBottom = 188,           ///< drops above this y go to the bags
	kClawY = 48,
	kPusherY = 115,
	kPusherW = 164,
	kPusherH = 73,
	// z orders
	kZButtons = 0,
	kZStart = -15,
	kZFlag = -15,
	kZBagDefault = -30,
	kZClaw = -10,
	kZPusher = -45,
	kZStamp = -45,
	kZBell = -45,
	kZSolanPost = -44,
	kZSolanIdle = -46,
	kZDrag = 61,                 ///< a letter being carried is above all others
	kLetterZRange = 60,
	kScoreLetter = 10,
	kScoreBag = 100,
	kSolanIdleInterval = 120,    ///< seconds
	kGoldLetterFirst = 100,      ///< every 120th letter from the 100th on is gold
	kGoldLetterPeriod = 120,
	kGoldBagPeriod = 20,
	kLettersPerCountryMax = 11,
	kBagsPerCountryMax = 4,
	kLetterCrowd = 22            ///< with this many letters on the floor bags follow the letters
};

LettersortScene::Sprite::Sprite() : anim(nullptr) {
	memset(&def, 0, sizeof(def));
	name[0] = 0;
	def.name = name;
}

LettersortScene::LettersortScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def),
	_countryCount(0), _running(false), _score(0), _level(0), _lettersSpawned(0), _bagsSpawned(0),
	_lettersSorted(0), _bagsCleared(0), _capacity(0), _bagZ(0), _letterZ(0), _bellCount(0),
	_postX(0), _postY(0), _pusherActive(false), _pusherX(0), _pusherY(0), _pusherPhase(0), _pusherStart(0),
	_startTime(0), _lastSecond(0), _dragLetter(-1), _dragOffX(0), _dragOffY(0), _highlighted(0) {
	// FUN_0043de30: the letter structs, hotspots 100..129
	for (int i = 0; i < kMaxLetters; i++) {
		Letter &l = _letters[i];
		l.active = false;
		l.x = l.y = l.country = l.variant = 0;
		l.gold = false;
		defineSprite(l.sprite, false, true, false, false, kHotspotLetter + i, 0, 0, 0);
	}
	// FUN_0043ddc0: the bag structs, a colour keyed bag and an opaque flag
	for (int b = 0; b < kMaxBags; b++) {
		Bag &bag = _bags[b];
		bag.active = false;
		bag.x = bag.y = bag.country = bag.fill = 0;
		bag.gold = false;
		defineSprite(bag.bag, false, true, true, false, 0, 0, 0, 0);
		defineSprite(bag.flag, false, true, false, false, 0, 0, 0, 0);
	}
	// FUN_0043e180: the claw structs, colour keyed clips
	for (int c = 0; c < kMaxClaws; c++) {
		Claw &claw = _claws[c];
		claw.active = false;
		claw.x = claw.y = claw.bag = claw.state = 0;
		claw.gold = false;
		defineSprite(claw.sprite, true, true, true, false, 0, 0, 0, 0);
	}
	// The scene table entries the handler renames at run time.
	defineSprite(_solanPost, true, true, true, false, 0, 290, 241, 0);
	defineSprite(_solanIdle, true, true, true, false, 0, 290, 241, 0);
	defineSprite(_bagout, true, true, true, false, 0, 614, 112, 0);
	defineSprite(_stamp, false, true, true, false, 0, 626, 63, 0);
}

LettersortScene::~LettersortScene() {
	for (int i = 0; i < kMaxLetters; i++) {
		removeSprite(_letters[i].sprite);
		delete _letters[i].sprite.anim;
	}
	for (int b = 0; b < kMaxBags; b++) {
		removeSprite(_bags[b].bag);
		delete _bags[b].bag.anim;
		removeSprite(_bags[b].flag);
		delete _bags[b].flag.anim;
	}
	for (int c = 0; c < kMaxClaws; c++) {
		removeSprite(_claws[c].sprite);
		delete _claws[c].sprite.anim;
	}
	Sprite *singles[] = { &_solanPost, &_solanIdle, &_bagout, &_stamp };
	for (Sprite *s : singles) {
		removeSprite(*s);
		delete s->anim;
	}
	for (Sprite *s : _sounds) {
		removeSprite(*s);
		delete s->anim;
		delete s;
	}
}

// ---- sprites ---------------------------------------------------------------

void LettersortScene::defineSprite(Sprite &s, bool smacker, bool visible, bool transparent, bool loop, int hotspot, int x, int y, int z) {
	s.def.name = s.name;
	s.def.smacker = smacker;
	s.def.visible = visible;
	s.def.transparent = transparent;
	s.def.loop = loop;
	s.def.hotspot = hotspot;
	s.def.x = x;
	s.def.y = y;
	s.def.group = 0;
	s.def.zOrder = z;
	s.def.overlay = 0;
	delete s.anim;
	s.anim = new Anim(this, &s.def);
}

void LettersortScene::nameSprite(Sprite &s, const Common::String &name) {
	Common::strlcpy(s.name, name.c_str(), sizeof(s.name));
}

void LettersortScene::removeSprite(Sprite &s) {
	if (s.anim && s.anim->isAdded())
		s.anim->remove();
}

// FUN_0043e2a0: plays "<name> <n>" with a random n in 1..variants as a
// clone of the sound effect struct, so several can sound at once.
void LettersortScene::playSound(const char *name, int variants) {
	Common::String file = Common::String::format("%s %d", name, variants > 0 ? (int)_vm->getRandomNumber(variants - 1) + 1 : 1);
	Sprite *s = nullptr;
	for (Sprite *c : _sounds)
		if (!c->anim->isAdded()) {
			s = c;
			break;
		}
	if (!s) {
		s = new Sprite();
		defineSprite(*s, true, false, false, false, 0, 0, 0, 0);
		_sounds.push_back(s);
	}
	nameSprite(*s, file);
	s->anim->play();
}

const char *LettersortScene::countryName(int country) const {
	if (country < 0 || country >= _countryCount)
		return "";
	return _countries[country].c_str();
}

// ---- loading ---------------------------------------------------------------

bool LettersortScene::load() {
	if (!Scene::load())
		return false;
	_font18.load("Amerigo BT_18_");
	_font10.load("Amerigo BT_10_");
	return true;
}

// FUN_0043dee0: the countries of country.ini, section by section in file
// order (scandinavia, europe, world), each section shuffled. Level n uses
// the first kLevels_[n].countries entries.
void LettersortScene::loadCountries() {
	Common::INIFile ini;
	_countryCount = 0;
	if (!resources()->loadIni(_name, "country.ini", ini)) {
		warning("Lettersort: country.ini missing");
		return;
	}
	const Common::INIFile::SectionList &sections = ini.getSections();
	for (const auto &section : sections) {
		int n = 0;
		Common::String value;
		while (ini.getKey(Common::String::format("resource%d", n), section.name, value))
			n++;
		// FUN_0040a150 picks a random unused index each time
		uint32 used = 0;
		for (int k = 0; k < n && _countryCount < kMaxCountries; k++) {
			int free = n - k;
			int pick = _vm->getRandomNumber(free - 1);
			int idx = -1;
			for (int j = 0; j < n; j++) {
				if (used & (1 << j))
					continue;
				if (pick-- == 0) {
					idx = j;
					break;
				}
			}
			used |= 1 << idx;
			ini.getKey(Common::String::format("resource%d", idx), section.name, value);
			value.trim();
			_countries[_countryCount++] = value;
		}
	}
	debug(1, "Lettersort: %d countries", _countryCount);
}

// ---- init (FUN_0043dc30) ------------------------------------------------

void LettersortScene::onInit(int arg) {
	const uint32 green = _vm->_screen->format.RGBToColor(0, 255, 0);

	_capacity = 4;
	_running = false;
	_bellCount = 0;
	_pusherActive = false;
	_clawQueue.clear();
	_lastSecond = 0;
	_startTime = g_engine->getGameMillis();

	// BUTTON module: help (id 3) at (0, 0), exit (id 2) at (728, 0)
	defineAnim("help_0", false, true, kHotspotHelp, 0, 0, kZButtons)->add(0, 0, kZButtons);
	defineAnim("help_1", false, true, kHotspotHelp, 0, 0, kZButtons);
	defineAnim("exit_0", false, true, kHotspotExit, 728, 0, kZButtons)->add(728, 0, kZButtons);
	defineAnim("exit_1", false, true, kHotspotExit, 728, 0, kZButtons);
	_highlighted = 0;

	addAnim("s1_1_start", Anim::kDefaultPos, Anim::kDefaultPos, kZStart);
	addAnim("borderleft", Anim::kDefaultPos, Anim::kDefaultPos, 0);
	addAnim("s1_1_frame", Anim::kDefaultPos, Anim::kDefaultPos, 0);
	addAnim("s1_1_bell", Anim::kDefaultPos, Anim::kDefaultPos, kZBell);

	// Text areas (rectangles at 0x49eaa0, 0x49eac0, 0x49eab0)
	defineSurfaceAnim("scoretext", 108, 49, green)->add(46, 232, 0);
	Anim *title = defineSurfaceAnim("titletext", 487, 43, green);
	title->add(154, 3, 0);
	_font18.drawStringCentred(*title->surface(), Common::Rect(0, 0, 487, 43), _vm->getString("lettersort:LONGNAME"));
	Anim *points = defineSurfaceAnim("pointstext", 109, 19, green);
	points->add(45, 276, -1);
	_font10.drawStringCentred(*points->surface(), Common::Rect(0, 0, 109, 19), _vm->getString("lettersort:POINTS"));
	drawScore();

	playMusic("subgame8");

	// TODO: tournament mode (FUN_00419490) starts the game at once with
	// the level set by the tournament's difficulty.
}

// FUN_0043e020: clears the floor and the belt and all counters.
void LettersortScene::resetGame() {
	_running = false;
	_score = 0;
	_level = 0;
	_lettersSpawned = 0;
	_bagsSpawned = 0;
	_lettersSorted = 0;
	_bagsCleared = 0;
	for (int i = 0; i < kMaxLetters; i++)
		if (_letters[i].active)
			removeLetter(i);
	for (int b = 0; b < kMaxBags; b++)
		if (_bags[b].active)
			removeBag(b);
	_clawQueue.clear();
	_dragLetter = -1;
	drawScore();
}

// FUN_0043dea0: the start button was pressed.
void LettersortScene::startGame() {
	// The original also reads a registry value here (FUN_0040b8b0).
	_vm->setGameRunning(true);
	resetGame();
	loadCountries();
	_bagZ = kZBagDefault;
	playAnim("s1_1_start");
}

// FUN_0043ef00
void LettersortScene::setLevel(int level) {
	_level = level;
	debug(1, "Lettersort: level %d (%d countries)", level + 1, kLevels_[level].countries);
	playSolanPost();
	showStamp(level);
}

// FUN_0043ef20: Solan delivers a batch of letters through the window. One
// of four clips; the first level always uses solpost1. The clips ending in
// "1" have a second part ("2") that plays once the letters are down.
void LettersortScene::playSolanPost() {
	removeSprite(_solanIdle);
	int pick = _level < 1 ? 0 : (int)_vm->getRandomNumber(3);
	const char *name;
	switch (pick) {
	case 1:
		name = "soldrop1";
		_postX = 260;
		_postY = 225;
		break;
	case 2:
		name = "solopp";
		_postX = 294;
		_postY = 242;
		break;
	case 3:
		name = "soldown";
		_postX = 294;
		_postY = 242;
		break;
	default:
		name = "solpost1";
		_postX = 294;
		_postY = 241;
		break;
	}
	// The original only renames the struct and lets a running delivery
	// finish; restarting it keeps the game state consistent.
	removeSprite(_solanPost);
	nameSprite(_solanPost, name);
	_solanPost.anim->add(_postX, _postY, kZSolanPost);
	_solanPost.anim->play();
}

// FUN_0043f850: Solan looks in now and then while nothing is delivered.
void LettersortScene::playSolanIdle() {
	if (_solanPost.anim->isAdded())
		return;
	removeSprite(_solanIdle);
	int x, y;
	const char *name;
	if (_vm->getRandomNumber(1) == 0) {
		name = "soljump";
		x = 292;
		y = 239;
	} else {
		name = "solrocket";
		x = 291;
		y = 241;
	}
	nameSprite(_solanIdle, name);
	_solanIdle.anim->add(x, y, kZSolanIdle);
	_solanIdle.anim->play();
}

// FUN_0043f010: the level stamp on the wall.
void LettersortScene::showStamp(int level) {
	removeSprite(_stamp);
	nameSprite(_stamp, Common::String::format("level%02d", level + 1));
	_stamp.anim->add(Anim::kDefaultPos, Anim::kDefaultPos, kZStamp);
	playSound("stamp", 1);
}

// FUN_0043e8c0
void LettersortScene::addScore(int n) {
	_score += n;
	drawScore();
}

// FUN_0043e140
void LettersortScene::drawScore() {
	Graphics::ManagedSurface *s = anim("scoretext")->surface();
	s->fillRect(Common::Rect(0, 0, s->w, s->h), s->format.RGBToColor(0, 255, 0));
	_font18.drawStringCentred(*s, Common::Rect(0, 0, s->w, s->h), Common::String::format("%d", _score));
}

// ---- input ---------------------------------------------------------------

void LettersortScene::highlightButton(int hotspot) {
	if (hotspot != kHotspotHelp && hotspot != kHotspotExit)
		hotspot = 0;
	if (hotspot == _highlighted)
		return;
	if (_highlighted == kHotspotHelp) {
		removeAnim("help_1");
		addAnim("help_0", 0, 0, kZButtons);
	} else if (_highlighted == kHotspotExit) {
		removeAnim("exit_1");
		addAnim("exit_0", 728, 0, kZButtons);
	}
	_highlighted = hotspot;
	if (hotspot == kHotspotHelp) {
		removeAnim("help_0");
		addAnim("help_1", 0, 0, kZButtons);
	} else if (hotspot == kHotspotExit) {
		removeAnim("exit_0");
		addAnim("exit_1", 728, 0, kZButtons);
	}
}

void LettersortScene::onMouseMove(int hotspot, int x, int y) {
	highlightButton(hotspot);
}

void LettersortScene::onKey(const Common::KeyState &key) {
	if (key.keycode == Common::KEYCODE_ESCAPE)
		_vm->endGame();
}

// FUN_0043e230 (0x105) and the BUTTON module's presses (0x116 -> FUN_0043e370).
void LettersortScene::onMouseDown(int hotspot, int x, int y) {
	if (hotspot == kHotspotExit) {
		_vm->endGame();
		return;
	}
	if (hotspot == kHotspotHelp) {
		_vm->showHelp();
		return;
	}
	if (hotspot == kHotspotStart) {
		if (!isAnimPlaying("s1_1_start")) {
			playSound("click", 1);
			startGame();
		}
		return;
	}
	if (_running && hotspot >= kHotspotLetter && hotspot < kHotspotLetter + kMaxLetters) {
		playSound("pickup", 2);
		pickupLetter(hotspot - kHotspotLetter, x, y);
	}
}

// The EVENT module's drag ends with event 0x10b -> FUN_0043e390.
void LettersortScene::onMouseUp(int hotspot, int x, int y) {
	if (_dragLetter < 0)
		return;
	int i = _dragLetter;
	_dragLetter = -1;
	dropLetter(i, x, y);
}

// ---- letters -------------------------------------------------------------

// FUN_0043ed30
int LettersortScene::freeLetter() const {
	for (int i = 0; i < kMaxLetters; i++)
		if (!_letters[i].active)
			return i;
	return -1;
}

// FUN_0043e5f0
void LettersortScene::letterBitmapName(Letter &l) {
	if (l.gold)
		nameSprite(l.sprite, "ltrGold01");
	else
		nameSprite(l.sprite, Common::String::format("ltr%s%02d", countryName(l.country), l.variant + 1));
}

// FUN_0043e630: puts a letter on the floor on top of the others. The z
// counter runs 0..59; past that every letter drops by 60 and the bell is
// put just below the lowest one.
void LettersortScene::showLetter(Letter &l) {
	if (_letterZ < 0 || _letterZ >= kLetterZRange) {
		int minZ = INT_MAX;
		for (int i = 0; i < kMaxLetters; i++) {
			Letter &o = _letters[i];
			if (!o.active || !o.sprite.anim->isAdded())
				continue;
			o.sprite.anim->setZ(o.sprite.anim->z() - kLetterZRange);
			if (o.sprite.anim->z() <= minZ)
				minZ = o.sprite.anim->z();
		}
		anim("s1_1_bell")->setZ(minZ - 1);
		_letterZ -= kLetterZRange;
	}
	l.sprite.anim->add(l.x, l.y, _letterZ++);
}

// FUN_0043ee70 / FUN_0043ed60: a random spot on the floor away from the mouse.
void LettersortScene::randomLetterPos(int &x, int &y) {
	Common::Point m = _vm->getEventManager()->getMousePos();
	Common::Rect mouse(m.x - 16, m.y - 16, m.x + 16, m.y + 16);
	int w = kFloorRight - kFloorLeft - kLetterW;
	int h = kFloorBottom - kFloorTop - kLetterH;
	for (int tries = 0; tries < 100; tries++) {
		x = (int)_vm->getRandomNumber(w - 1) + kFloorLeft;
		y = (int)_vm->getRandomNumber(h - 1) + kFloorTop;
		// FUN_0043ee30 treats touching edges as overlapping
		if (y > mouse.bottom || x > mouse.right || mouse.top > y + kLetterH || mouse.left > x + kLetterW)
			return;
	}
}

// FUN_0043e6d0: keeps a dropped letter on the floor, with a little jitter.
void LettersortScene::clampLetterPos(int &x, int &y) {
	int jx = (int)_vm->getRandomNumber(4) - 2;
	int jy = (int)_vm->getRandomNumber(4) - 2;
	if (x < kFloorLeft)
		x = kFloorLeft + jx;
	else if (x > kFloorRight - kLetterW)
		x = kFloorRight - kLetterW + jx;
	if (y < kFloorTop)
		y = kFloorTop + jy;
	else if (y > kFloorBottom - kLetterH)
		y = kFloorBottom - kLetterH + jy;
}

// FUN_0043ece0: letters on the floor per country.
int LettersortScene::letterHistogram(int *hist) const {
	int count = 0;
	for (int c = 0; c < kMaxCountries; c++)
		hist[c] = 0;
	for (int i = 0; i < kMaxLetters; i++)
		if (_letters[i].active) {
			count++;
			if (_letters[i].country >= 0 && _letters[i].country < kMaxCountries)
				hist[_letters[i].country]++;
		}
	return count;
}

// FUN_0043ec70 / FUN_0043ecb0: a random country (of the n in play) with
// fewer than limit entries; after 100 tries the first such one.
int LettersortScene::pickBelow(int limit, const int *hist, int n) {
	if (n <= 0)
		return -1;
	for (int tries = 0; tries < 100; tries++) {
		int r = (int)_vm->getRandomNumber(n - 1);
		if (hist[r] < limit)
			return r;
	}
	for (int r = 0; r < n; r++)
		if (hist[r] < limit)
			return r;
	return -1;
}

// FUN_0043fab0 / FUN_0043faf0: the same with more than limit entries.
int LettersortScene::pickAbove(int limit, const int *hist, int n) {
	if (n <= 0)
		return -1;
	for (int tries = 0; tries < 100; tries++) {
		int r = (int)_vm->getRandomNumber(n - 1);
		if (hist[r] > limit)
			return r;
	}
	for (int r = 0; r < n; r++)
		if (hist[r] > limit)
			return r;
	return -1;
}

// FUN_0043ec40: at most 11 letters of a country on the floor.
int LettersortScene::pickLetterCountry() {
	int hist[kMaxCountries];
	letterHistogram(hist);
	int c = pickBelow(kLettersPerCountryMax, hist, kLevels_[_level].countries);
	return c < 0 ? 0 : c;
}

// FUN_0043fa40: a new bag's country. Normally one with fewer than four
// bags on the belt; once the floor is crowded one that has more than two
// letters waiting.
int LettersortScene::pickBagCountry() {
	int letters[kMaxCountries];
	int n = kLevels_[_level].countries;
	int count = letterHistogram(letters);
	int c;
	if (count < kLetterCrowd) {
		int bags[kMaxCountries];
		for (int i = 0; i < kMaxCountries; i++)
			bags[i] = 0;
		for (int b = 0; b < kMaxBags; b++)
			if (_bags[b].active && _bags[b].country >= 0 && _bags[b].country < kMaxCountries)
				bags[_bags[b].country]++;
		c = pickBelow(kBagsPerCountryMax, bags, n);
	} else {
		c = pickAbove(2, letters, n);
	}
	return c < 0 ? 0 : c;
}

// FUN_0043ebc0: Solan throws in one letter.
void LettersortScene::spawnLetter() {
	int i = freeLetter();
	if (i < 0)
		return;
	Letter &l = _letters[i];
	l.active = true;
	randomLetterPos(l.x, l.y);
	// FUN_0043eed0: the 100th letter and every 120th after it is gold
	l.gold = _lettersSpawned >= kGoldLetterFirst && (_lettersSpawned - kGoldLetterFirst) % kGoldLetterPeriod == 0;
	l.country = pickLetterCountry();
	l.variant = (int)_vm->getRandomNumber(11);
	letterBitmapName(l);
	showLetter(l);
	_lettersSpawned++;
	debug(1, "Lettersort: letter %d (%s) at %d,%d", i, l.sprite.name, l.x, l.y);
}

// FUN_0043eba0: the batch at the start of a level.
void LettersortScene::dropLetters() {
	for (int i = 0; i < 10; i++)
		spawnLetter();
	playSound("dropall", 2);
}

// FUN_0043e110
void LettersortScene::removeLetter(int i) {
	removeSprite(_letters[i].sprite);
	_letters[i].active = false;
	if (_dragLetter == i)
		_dragLetter = -1;
}

// FUN_0043e2d0: the letter shrinks to its "Tiny" picture under the mouse
// and follows it (FUN_0040c1f0 of the EVENT module).
void LettersortScene::pickupLetter(int i, int x, int y) {
	Letter &l = _letters[i];
	removeSprite(l.sprite);
	// FUN_0043e330
	nameSprite(l.sprite, Common::String::format("Tiny %s", l.gold ? "Gold" : countryName(l.country)));
	l.sprite.anim->add(x, y, kZDrag);
	// FUN_00412cf0 centres the element on the mouse
	Common::Rect r = l.sprite.anim->rect();
	_dragOffX = r.width() / 2;
	_dragOffY = r.height() / 2;
	l.sprite.anim->setPos(x - _dragOffX, y - _dragOffY);
	_dragLetter = i;
}

// FUN_0043e390: the letter is released.
void LettersortScene::dropLetter(int i, int x, int y) {
	int b = bagAt(x);
	if (y > kBeltBottom) {
		playSound("drop", 1);
		placeLetter(i, x, y);
		return;
	}
	if (!_running || b < 0 || _bags[b].fill >= _capacity) {
		debug(1, "Lettersort: letter %d dropped on no bag (%d)", i, b);
		playSound("drop", 1);
		returnLetter(i);
		return;
	}
	if (!letterMatches(i, b)) {
		debug(1, "Lettersort: letter %d (%s) is wrong for bag %d (%s)", i, _letters[i].sprite.name, b, countryName(_bags[b].country));
		playSound("wrong", 1);
		returnLetter(i);
		return;
	}
	debug(1, "Lettersort: letter %d (%s) sorted into bag %d (%s)", i, _letters[i].sprite.name, b, countryName(_bags[b].country));
	playSound("drop", 1);
	_lettersSorted++;
	bool gold = _letters[i].gold;
	removeLetter(i);
	addScore(kScoreLetter);
	fillBag(b, gold ? _capacity : 1);
	if (_bags[b].fill >= _capacity) {
		queueClaw(b);
		if (_bags[b].gold)
			queueAllBags();
	}
}

// FUN_0043e570: put down on the floor, centred on the mouse.
void LettersortScene::placeLetter(int i, int x, int y) {
	Letter &l = _letters[i];
	x -= kLetterW / 2;
	y -= kLetterH / 2;
	clampLetterPos(x, y);
	removeSprite(l.sprite);
	letterBitmapName(l);
	l.x = x;
	l.y = y;
	showLetter(l);
}

// FUN_0043e750: back to where it was picked up.
void LettersortScene::returnLetter(int i) {
	Letter &l = _letters[i];
	removeSprite(l.sprite);
	letterBitmapName(l);
	showLetter(l);
}

// ---- bags ----------------------------------------------------------------

// FUN_0043fb70
int LettersortScene::freeBag() const {
	for (int b = 0; b < kMaxBags; b++)
		if (!_bags[b].active)
			return b;
	return -1;
}

// FUN_0043e540
void LettersortScene::bagBitmapName(Bag &bag) {
	nameSprite(bag.bag, Common::String::format("bag%d%s", bag.fill, bag.gold ? "gold" : ""));
}

// FUN_0043f950: a new bag appears left of the belt, to be pushed on.
void LettersortScene::spawnBag() {
	int b = freeBag();
	if (b < 0)
		return;
	Bag &bag = _bags[b];
	bag.active = true;
	bag.x = kBeltLeft - kBagW;
	bag.y = kBagY;
	bag.country = pickBagCountry();
	bag.fill = 0;
	// FUN_0043fba0: every 20th bag is gold
	bag.gold = _bagsSpawned > 0 && _bagsSpawned % kGoldBagPeriod == 0;
	bagBitmapName(bag);
	nameSprite(bag.flag, Common::String::format("flg%s", countryName(bag.country)));
	bag.bag.anim->add(bag.x, bag.y, _bagZ);
	bag.flag.anim->add(bag.x, bag.y + kFlagOffsetY, kZFlag);
	_bagsSpawned++;
	debug(1, "Lettersort: bag %d for %s%s", b, countryName(bag.country), bag.gold ? " (gold)" : "");
}

// FUN_0043e090
void LettersortScene::removeBag(int b) {
	removeSprite(_bags[b].bag);
	removeSprite(_bags[b].flag);
	_bags[b].active = false;
}

// FUN_0043e840
Common::Rect LettersortScene::bagRect(int b) const {
	return Common::Rect(_bags[b].x, _bags[b].y, _bags[b].x + kBagW, _bags[b].y + kBagH);
}

// FUN_0043e800 + FUN_0043ea30: the active bags from left to right.
void LettersortScene::sortedBags(Common::Array<int> &bags) const {
	bags.clear();
	for (int b = 0; b < kMaxBags; b++)
		if (_bags[b].active)
			bags.push_back(b);
	for (uint i = 1; i < bags.size(); i++) {
		int v = bags[i];
		int j = i - 1;
		while (j >= 0 && _bags[bags[j]].x > _bags[v].x) {
			bags[j + 1] = bags[j];
			j--;
		}
		bags[j + 1] = v;
	}
}

// FUN_0043e790: the bag under a mouse x; only x counts, the bags all sit
// on the belt.
int LettersortScene::bagAt(int x) const {
	Common::Array<int> bags;
	sortedBags(bags);
	for (uint i = 0; i < bags.size(); i++) {
		Common::Rect r = bagRect(bags[i]);
		if (r.left < x && x < r.right)
			return bags[i];
	}
	return -1;
}

// FUN_0043f580
void LettersortScene::setBagPos(int b, int x, int y) {
	Bag &bag = _bags[b];
	bag.x = x;
	bag.y = y;
	bag.bag.anim->setPos(x, y);
	bag.flag.anim->setPos(x, y + kFlagOffsetY);
}

// FUN_0043e4d0: adds letters to a bag and shows the fuller bitmap.
int LettersortScene::fillBag(int b, int n) {
	Bag &bag = _bags[b];
	int fill = CLIP(bag.fill + n, 0, _capacity);
	if (fill != bag.fill) {
		bag.fill = fill;
		debug(1, "Lettersort: bag %d holds %d letters", b, fill);
		removeSprite(bag.bag);
		bagBitmapName(bag);
		bag.bag.anim->add(bag.x, bag.y, _bagZ);
	}
	return bag.fill;
}

// FUN_0043e880: gold letters go anywhere.
bool LettersortScene::letterMatches(int i, int b) const {
	return _letters[i].gold || _letters[i].country == _bags[b].country;
}

// FUN_0043e9b0
bool LettersortScene::clawHolds(int b) const {
	for (int c = 0; c < kMaxClaws; c++)
		if (_claws[c].active && _claws[c].bag == b)
			return true;
	return false;
}

// FUN_0043e8e0: a full bag waits for the claw, once.
void LettersortScene::queueClaw(int b) {
	if (clawHolds(b))
		return;
	for (uint i = 0; i < _clawQueue.size(); i++)
		if (_clawQueue[i] == b)
			return;
	_clawQueue.push_back(b);
}

// FUN_0043e9f0: a full gold bag takes every bag off the belt.
void LettersortScene::queueAllBags() {
	Common::Array<int> bags;
	sortedBags(bags);
	for (uint i = 0; i < bags.size(); i++)
		queueClaw(bags[i]);
}

// ---- the pusher ----------------------------------------------------------

// FUN_0043fbc0: the steam pusher comes in from the left with every new bag.
void LettersortScene::startPusher() {
	_pusherActive = true;
	_pusherX = kBeltLeft - 248;
	_pusherY = kPusherY;
	_pusherPhase = 1000;
	_pusherStart = g_engine->getGameMillis();
	playSound("pusher", 1);
	Anim *p = anim("pusher");
	if (p->isAdded())
		p->setPos(_pusherX, _pusherY);
	else
		p->add(_pusherX, _pusherY, kZPusher);
}

// FUN_0043f670: one second out to x = -23, one second back to x = -137.
// Returns false once the stroke is over.
bool LettersortScene::updatePusher() {
	const int x0 = kBeltLeft - 248;
	const int xMid = kBeltLeft - 50;
	const int xEnd = kBeltLeft - 164;
	float t = (float)(g_engine->getGameMillis() - _pusherStart) * 0.001f;
	int x;
	if (_pusherPhase >= 0 && t >= 1.0f) {
		_pusherPhase = -1000;
		x = x0 + (int)((float)(xMid - x0) * 1.0f + 0.5f);
	} else if (t <= 1.0f) {
		x = x0 + (int)((float)(xMid - x0) * t + 0.5f);
	} else if (t <= 2.0f) {
		x = xMid + (int)((float)(xEnd - xMid) * (t - 1.0f) + 0.5f);
	} else {
		return false;
	}
	_pusherX = x;
	anim("pusher")->setPos(_pusherX, _pusherY);
	return true;
}

// FUN_0043f4f0: the pusher shoves the chain of bags along the belt.
void LettersortScene::pushBags() {
	Common::Array<int> bags;
	sortedBags(bags);
	int right = _pusherX + kPusherW;
	for (uint i = 0; i < bags.size(); i++) {
		int b = bags[i];
		if (_bags[b].x >= right)
			return;
		setBagPos(b, right, _bags[b].y);
		right = _bags[b].x + kBagW;
	}
}

// FUN_0043f400: the rightmost bag, if it is past the end of the belt.
int LettersortScene::fallingBag() const {
	Common::Array<int> bags;
	sortedBags(bags);
	if (bags.empty())
		return -1;
	int b = bags.back();
	if (_bags[b].x > kBagFallX)
		return b;
	return -1;
}

// FUN_0043f460: the bag falls off; the game stops (FUN_0043f4e0) and the
// alarm follows the clip.
void LettersortScene::dropBag(int b) {
	bool gold = _bags[b].gold;
	removeBag(b);
	removeSprite(_bagout);
	nameSprite(_bagout, gold ? "bagoutgold" : "bagout");
	_bagout.anim->add(Anim::kDefaultPos, Anim::kDefaultPos, _bagZ - 1);
	_bagout.anim->play();
	_running = false;
	debug(1, "Lettersort: bag %d fell off the belt, game over with %d points", b, _score);
}

// ---- claws ---------------------------------------------------------------

// FUN_0043fcb0
int LettersortScene::freeClaw() const {
	for (int c = 0; c < kMaxClaws; c++)
		if (!_claws[c].active)
			return c;
	return -1;
}

// FUN_0043f210
void LettersortScene::clawClipName(Claw &c) {
	const char *kind = "grab";
	if (c.state == 1)
		kind = "retry";
	else if (c.state == 2)
		kind = c.gold ? "gold" : "brown";
	nameSprite(c.sprite, Common::String::format("claw%s", kind));
}

// FUN_0043fc30: a claw comes down over a full bag.
void LettersortScene::startClaw(int b) {
	int i = freeClaw();
	if (i < 0)
		return;
	Claw &c = _claws[i];
	c.active = true;
	c.state = 0;
	c.bag = b;
	c.gold = _bags[b].gold;
	clawClipName(c);
	// FUN_0043f2f0
	c.x = _bags[b].x;
	c.y = kClawY;
	c.sprite.anim->add(c.x, c.y, kZClaw);
	c.sprite.anim->play();
	debug(1, "Lettersort: claw %d comes down on bag %d at x %d", i, b, c.x);
}

// FUN_0043f370
void LettersortScene::removeClaw(int i) {
	removeSprite(_claws[i].sprite);
	_claws[i].active = false;
}

// FUN_0043f0a0: the claw's state machine, driven by its clips ending.
void LettersortScene::clawFinished(int i) {
	Claw &c = _claws[i];
	bool bagValid = c.bag >= 0 && c.bag < kMaxBags && _bags[c.bag].active;
	if (c.state == 0) {
		// Grabbed: the bag must still be where the claw came down.
		removeSprite(c.sprite);
		if (!bagValid || _claws[i].x != _bags[c.bag].x) {
			c.state = 1;
			debug(1, "Lettersort: claw %d missed bag %d, retrying", i, c.bag);
		} else {
			c.state = 2;
			removeBag(c.bag);
			addScore(kScoreBag);
			debug(1, "Lettersort: claw %d lifts bag %d, score %d", i, c.bag, _score);
		}
		clawClipName(c);
		c.sprite.anim->add(c.x, c.y, kZClaw);
		c.sprite.anim->play();
	} else if (c.state == 1) {
		// Missed: try again over the bag's new place, if it is still there.
		if (_running && bagValid) {
			removeSprite(c.sprite);
			c.state = 0;
			clawClipName(c);
			c.x = _bags[c.bag].x;
			c.y = kClawY;
			c.sprite.anim->add(c.x, c.y, kZClaw);
			c.sprite.anim->play();
		} else {
			removeClaw(i);
		}
	} else if (c.state == 2) {
		removeClaw(i);
		_bagsCleared++;
		if (_running && levelComplete()) {
			// FUN_0043f2d0
			if (_level + 1 < kLevels)
				setLevel(_level + 1);
		}
	}
}

// FUN_0043f2a0
bool LettersortScene::levelComplete() const {
	if (_level > 8)
		return false;
	return _bagsCleared >= kLevels_[_level].bags;
}

// ---- time ----------------------------------------------------------------

// FUN_0043f750: the 250 ms timer, acting once per second.
void LettersortScene::secondTick(int sec) {
	if (isAnimPlaying("s1_1_bell"))
		bellTick();
	if (!_running)
		return;
	const LevelDef &lv = kLevels_[_level];
	if (sec % kSolanIdleInterval == 0 && !_solanPost.anim->isAdded())
		playSolanIdle();
	if (sec % lv.bagInterval == 0) {
		spawnBag();
		startPusher();
	}
	if (sec % lv.letterInterval == 0 && !_solanPost.anim->isPlaying())
		spawnLetter();
	if (!_clawQueue.empty()) {
		int b = _clawQueue[0];
		_clawQueue.remove_at(0);
		if (b >= 0 && b < kMaxBags && _bags[b].active)
			startClaw(b);
	}
}

// FUN_0043fce0: the alarm rings for a few seconds, then the game is over.
void LettersortScene::bellTick() {
	_bellCount--;
	if (_bellCount >= 0)
		return;
	anim("s1_1_bell")->stop();
	_vm->setGameRunning(false);
	debug(1, "Lettersort: game over, %d points (%d letters sorted, %d bags lifted)", _score, _lettersSorted, _bagsCleared);
	// Modal: the game has stopped (_running) and the bell no longer plays,
	// so the nested frames do not come back here.
	_vm->messageBox("interfaceh:GAMEOVER", "lettersort:GAMEOVER", MessageBox::kButtonOk);
	// TODO: the high score registration FUN_0041f9a0 (medals at 2500, 5000,
	// 10000 and 20000 points), whose callback LAB_0043fd50 follows.
	gameOverDone();
}

// LAB_0043fd50: back to the start button.
void LettersortScene::gameOverDone() {
	resetGame();
	removeSprite(_stamp);
	addAnim("s1_1_start", Anim::kDefaultPos, Anim::kDefaultPos, kZStart);
}

void LettersortScene::onUpdate() {
	// The carried letter follows the mouse (FUN_0040c300).
	if (_dragLetter >= 0) {
		Common::Point m = _vm->getEventManager()->getMousePos();
		_letters[_dragLetter].sprite.anim->setPos(m.x - _dragOffX, m.y - _dragOffY);
	}

	int sec = (int)((g_engine->getGameMillis() - _startTime) / 1000);
	if (sec != _lastSecond) {
		_lastSecond = sec;
		secondTick(sec);
	}

	// FUN_0043f3c0 (0x112): the pusher stroke
	if (_pusherActive) {
		if (!updatePusher()) {
			// FUN_0043f650
			_pusherActive = false;
			removeAnim("pusher");
		} else {
			pushBags();
			int b = fallingBag();
			if (b >= 0)
				dropBag(b);
		}
	}
}

// FUN_0043ea80 (0x110)
void LettersortScene::onAnimFinished(Anim *a) {
	if (a == _solanPost.anim) {
		removeSprite(_solanPost);
		size_t len = strlen(_solanPost.name);
		char last = len ? _solanPost.name[len - 1] : 0;
		if (last != '2') {
			dropLetters();
			if (last == '1') {
				_solanPost.name[len - 1] = '2';
				_solanPost.anim->add(_postX, _postY, kZSolanPost);
				_solanPost.anim->play();
			}
		}
		return;
	}
	if (a == _solanIdle.anim) {
		removeSprite(_solanIdle);
		return;
	}
	if (a == _bagout.anim) {
		removeSprite(_bagout);
		// FUN_0043f3a0
		_bellCount = 2;
		playAnim("s1_1_bell");
		return;
	}
	if (!strcmp(a->name(), "s1_1_start")) {
		a->remove();
		_running = true;
		// TODO: tournament difficulty * 3 as the start level (FUN_004194a0)
		setLevel(0);
		return;
	}
	for (int c = 0; c < kMaxClaws; c++)
		if (a == _claws[c].sprite.anim) {
			clawFinished(c);
			return;
		}
	for (Sprite *s : _sounds)
		if (a == s->anim) {
			a->remove();
			return;
		}
}

} // End of namespace Flaaklypa
