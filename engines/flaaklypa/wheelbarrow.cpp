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
#include "common/system.h"
#include "common/textconsole.h"

#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/resources.h"
#include "flaaklypa/dialog.h"
#include "flaaklypa/wheelbarrow.h"

namespace Flaaklypa {

// Where apples appear on the roof (0x49f158): centre x, y and the bounce
// stage they start in. Picked at random by the spawn timer.
const int WheelbarrowScene::kSpawnTable[kMaxApples][3] = {
	{ 145, 191, 1 }, { 193, 78, 0 }, { 247, 166, 2 }, { 321, 182, 2 }, { 389, 103, 1 },
	{ 504, 193, 2 }, { 522, 116, 2 }, { 603, 75, 0 }, { 642, 144, 1 }, { 649, 231, 0 }
};

// The roof (0x49f1d0): when an apple in stage n reaches the y of row n it
// bounces with the given action (0/1 roof, 3 ground) and enters stage n+1.
// Apples in stage 4 (past the eaves) can be caught.
const int WheelbarrowScene::kRoofTable[5][2] = {
	{ 273, 0 }, { 300, 0 }, { 335, 0 }, { 362, 1 }, { 570, 3 }
};

// Velocity kept after a bounce, per action (0x49f108).
const float WheelbarrowScene::kBounce[4] = { 0.5f, 0.8f, 0.3f, 0.2f };

const char *const WheelbarrowScene::kDirNames[2] = { "left", "right" };
const char *const WheelbarrowScene::kStateNames[5] = { "neutral", "cycle", "stop", "dump", "bonk" };

enum {
	kSolanY = 478,          ///< top of Solan's 152x92 sprite
	kSolanZ = 100,
	kSolanMinX = 55,        ///< 0x49f20c
	kSolanMaxX = 578,       ///< 0x49f208
	kBasketX = 60,          ///< 0x49e060: Solan can tip the wheelbarrow left of this
	kAppleLimitY = 620,     ///< 0x49f210: apples below this are removed
	kGravity = 600,         ///< 0x49da18, pixels/s^2
	kSpawnDelay = 2000,     ///< first apple this long after the timer is (re)set
	kStartLives = 3,
	kBasketCapacity = 24,
	kLifeEvery = 3000,      ///< points per extra life
	kLevelBonus = 500,
	kDumpPoints = 20,       ///< per apple tipped into the basket
	kTextZ = 200
};

// VK_LEFT, VK_UP, VK_RIGHT of the original are mapped to the arrow keys.

WheelbarrowScene::WheelbarrowScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def),
	_running(false), _score(0), _level(0), _basketCount(0), _lives(0), _basketCapacity(0),
	_solanX(0), _dir(0), _speed(0), _count(0), _solanState(kNeutral), _lastTick(0), _spawnPeriod(0),
	_leftHeld(false), _rightHeld(false), _highlighted(0) {
	for (int i = 0; i < kMaxApples; i++) {
		_apples[i].active = false;
		_apples[i].type = 0;
		_apples[i].x = _apples[i].y = _apples[i].vy = 0;
		_apples[i].stage = _apples[i].state = 0;
	}
}

WheelbarrowScene::~WheelbarrowScene() {
	destroyElement(_solan);
	destroyElement(_basket);
	for (int i = 0; i < kCounterSlots; i++)
		destroyElement(_counter[i]);
	for (int i = 0; i < kMaxPies; i++)
		destroyElement(_pies[i]);
	for (int i = 0; i < kSfxClones; i++)
		destroyElement(_sfx[i]);
	for (int i = 0; i < kMaxApples; i++)
		destroyElement(_apples[i].elem);
}

// ---- animation structures owned by the scene ------------------------------

void WheelbarrowScene::initElement(Element &e, const char *name, bool smacker, bool visible, bool transparent, int x, int y, int z) {
	Common::strlcpy(e.name, name, sizeof(e.name));
	e.def.name = e.name;
	e.def.smacker = smacker;
	e.def.visible = visible;
	e.def.transparent = transparent;
	e.def.loop = 0;
	e.def.hotspot = 0;
	e.def.x = x;
	e.def.y = y;
	e.def.group = 0;
	e.def.zOrder = z;
	e.def.overlay = 0;
	delete e.anim;
	e.anim = new Anim(this, &e.def);
}

void WheelbarrowScene::destroyElement(Element &e) {
	if (e.anim) {
		e.anim->remove();
		delete e.anim;
		e.anim = nullptr;
	}
}

static int roundPos(float v) {
	// The original adds +/- 0.5 and truncates (__ftol).
	return (int)(v >= 0 ? v + 0.5f : v - 0.5f);
}

// ---- init / close --------------------------------------------------------

bool WheelbarrowScene::load() {
	if (!Scene::load())
		return false;
	_font.load("Amerigo BT_14_");
	return true;
}

// FUN_0044ee50: the init handler.
void WheelbarrowScene::onInit(int arg) {
	_running = false;
	_score = _level = _basketCount = _lives = 0;

	defineAnim("help_0", false, true, kHotspotHelp, 0, 0, kSolanZ)->add(0, 0, kSolanZ);
	defineAnim("help_1", false, true, kHotspotHelp, 0, 0, kSolanZ);
	defineAnim("exit_0", false, true, kHotspotExit, 728, 0, kSolanZ)->add(728, 0, kSolanZ);
	defineAnim("exit_1", false, true, kHotspotExit, 728, 0, kSolanZ);
	_highlighted = 0;

	// Score and lives: a label and a value area each (0x49f118..0x49f148).
	const uint32 green = _vm->_screen->format.RGBToColor(0, 255, 0);
	defineSurfaceAnim("scorelabel", 79, 25, green)->add(6, 85, kTextZ);
	defineSurfaceAnim("scoretext", 79, 25, green)->add(6, 110, kTextZ);
	defineSurfaceAnim("lifelabel", 80, 25, green)->add(715, 85, kTextZ);
	defineSurfaceAnim("lifetext", 80, 25, green)->add(715, 110, kTextZ);
	defineSurfaceAnim("starttext", 89, 33, green);
	drawText("scorelabel", _vm->getString("wheelbarrow:SCORE"));
	drawText("lifelabel", _vm->getString("wheelbarrow:LIFE"));

	_basketCapacity = 0;
	// FUN_0044f040: the apple counter at the top of the screen.
	for (int i = 0; i < kCounterSlots; i++)
		initElement(_counter[i], "apple", false, true, true, 208 + 30 * i, 3, 0);
	// FUN_0044f0b0: one pie per level, stacked up the right edge.
	for (int i = 0; i < kMaxPies; i++)
		initElement(_pies[i], "pie", false, true, true, 732, 546 - 10 * i, 0);
	initElement(_solan, "left_neutral0", false, true, true, 0, kSolanY, kSolanZ);
	initElement(_basket, "basket0", false, true, true, 54, 535, 0);
	for (int i = 0; i < kSfxClones; i++)
		initElement(_sfx[i], " ", true, false, false, 0, 0, 0);
	for (int i = 0; i < kMaxApples; i++)
		initElement(_apples[i].elem, "apple_0", true, true, true, 0, 0, 0);

	showStartScreen();
	playMusic("subgame18");
	playAnim("birds");
	_basketCapacity = kBasketCapacity;

	// With an active profile the original shows "tournament:PLAYERREADY"
	// and starts right away at level 3 * <profile value>. TODO: profiles.
}

// FUN_0044f5f0
void WheelbarrowScene::onClose() {
	killTimer(kTimerSpawn);
}

// FUN_0044efd0: the start button with its caption.
void WheelbarrowScene::showStartScreen() {
	if (!isAnimAdded("start"))
		addAnim("start", Anim::kDefaultPos, Anim::kDefaultPos, kSolanZ);
	const AnimDef *start = findAnimDef(_def, "start");
	drawText("starttext", _vm->getString("wheelbarrow:START"));
	anim("starttext")->add(start->x + 3, start->y + 0x1d, kSolanZ + 1);
}

// FUN_0044f1f0
void WheelbarrowScene::hideStartScreen() {
	if (isAnimAdded("start"))
		removeAnim("start");
	anim("starttext")->remove();
}

// FUN_0044f110: (re)starts a game at the given level. The wheelbarrow keeps
// the apples it held when the last game ended (the original does not reset
// that counter either).
void WheelbarrowScene::startGame(int level) {
	// FUN_0040d8a0(1): leaving now asks "gamec:SUBGAMEABORT" first.
	_vm->setGameRunning(true);
	hideStartScreen();
	clearApples();
	_solan.anim->remove();

	_running = false;
	_solanX = 324.0f;
	_score = 0;
	_speed = 300.0f;
	_level = 0;
	_basketCount = 0;
	_lives = 0;
	_dir = 0;
	setSolan(kNeutral);
	showCounter(_count);
	_running = true;
	_level = level;
	_lives = kStartLives;
	drawScore();
	drawLives();
	setLevel(_level);
	showBasket(_basketCount);
	_lastTick = g_engine->getGameMillis();
	debug(1, "Wheelbarrow: game started at level %d", level);
}

// FUN_00450180
void WheelbarrowScene::gameOver() {
	_vm->setGameRunning(false);
	_running = false;
	setSpawnTimer(-1);
	setSolan(kNeutral);
	if (isAnimAdded("pie"))
		removeAnim("pie");
	if (isAnimAdded("pickup"))
		removeAnim("pickup");
	if (isAnimAdded("putdown"))
		removeAnim("putdown");
	debug(1, "Wheelbarrow: game over, %d points, level %d", _score, _level);
	// Modal: the game has stopped (_running, no spawn timer), the nested
	// frames leave Solan and the apples alone.
	_vm->messageBox("interfaceh:GAMEOVER", "wheelbarrow:GAMEOVER", MessageBox::kButtonOk);
	// TODO: the high score registration FUN_0041f9a0("wheelbarrow:LONGNAME",
	// score, medals 2500/5000/10000/15000).
	// Without a profile the start button comes back.
	showStartScreen();
	clearApples();
}

// FUN_0044f520: shows the pies and sets the ripening rate of the level.
void WheelbarrowScene::setLevel(int level) {
	showPies(level);
	setSpawnTimer(3750 - 300 * level);
}

// FUN_0044f550: EVENT_AddTimer(now + 2000, period, 0); the EVENT module
// re-arms a timer with a non zero period. A negative period makes the
// original loop forever (level 13), so it is clamped here.
void WheelbarrowScene::setSpawnTimer(int period) {
	killTimer(kTimerSpawn);
	_spawnPeriod = 0;
	if (period < 0)
		return;
	_spawnPeriod = MAX(period, 50);
	setTimer(kTimerSpawn, kSpawnDelay);
}

// FUN_00450270: the timer event.
void WheelbarrowScene::onTimer(int id, int data) {
	if (id != kTimerSpawn)
		return;
	spawnApple();
	if (_spawnPeriod > 0)
		setTimer(kTimerSpawn, _spawnPeriod);
}

// ---- Solan ---------------------------------------------------------------

// FUN_0044f410: how full the wheelbarrow looks, 0..5.
int WheelbarrowScene::fullness() const {
	int f = _count * 5 / kCounterSlots;
	return CLIP(f, 0, 5);
}

// FUN_0044f300: shows Solan in a state: "<left|right>_<state><fullness>",
// a bitmap for neutral, otherwise a clip (looping while walking).
void WheelbarrowScene::setSolan(int state) {
	Common::String name = Common::String::format("%s_%s%d", kDirNames[_dir], kStateNames[state], fullness());
	_solan.anim->remove();
	Common::strlcpy(_solan.name, name.c_str(), sizeof(_solan.name));
	_solan.def.smacker = state != kNeutral;
	_solan.def.visible = 1;
	_solan.def.transparent = 1;
	_solan.def.loop = state == kCycle;
	_solan.anim->add(roundPos(_solanX), kSolanY, kSolanZ);
	if (_solan.def.smacker)
		_solan.anim->play();
	_solanState = state;
}

// FUN_0044fbe0
void WheelbarrowScene::moveSolan(float dt) {
	if (_solanState != kCycle)
		return;
	if (_dir == 0)
		_solanX -= _speed * dt;
	else if (_dir == 1)
		_solanX += _speed * dt;
	if (_solanX < (float)kSolanMinX)
		_solanX = (float)kSolanMinX;
	else if (_solanX > (float)kSolanMaxX)
		_solanX = (float)kSolanMaxX;
	_solan.anim->setPos(roundPos(_solanX), kSolanY);
}

// FUN_0044f870: after a clip that ends in a pose, continue with whatever
// arrow key is still held (GetAsyncKeyState in the original).
void WheelbarrowScene::resumeFromKeys() {
	if (_rightHeld) {
		_dir = 1;
		setSolan(kCycle);
	} else if (_leftHeld) {
		_dir = 0;
		setSolan(kCycle);
	} else {
		setSolan(kNeutral);
	}
}

// FUN_0044faf0: the dump clip has ended: the apples go into the basket.
void WheelbarrowScene::dumpBarrow() {
	if (!_running)
		return;
	_basketCount += _count;
	addScore(_count * kDumpPoints);
	debug(1, "Wheelbarrow: tipped %d apples into the basket (%d), score %d", _count, _basketCount, _score);
	_count = 0;
	showCounter(0);
	if (_basketCount < _basketCapacity) {
		showBasket(_basketCount);
		return;
	}
	// Basket full: Ludvig carries it off, the pie comes out of the window
	// (next level) and he puts down an empty one. No apples meanwhile.
	setSpawnTimer(-1);
	_basketCount = 0;
	showBasket(-1);
	playAnim("pickup");
}

// FUN_0044fd20: the catch area of the wheelbarrow, relative to Solan.
Common::Rect WheelbarrowScene::barrowRect() const {
	int x = roundPos(_solanX);
	if (_dir == 0)
		return Common::Rect(x + 39, kSolanY + 42, x + 39 + 94, kSolanY + 42 + 65);
	return Common::Rect(x + 57, kSolanY + 42, x + 57 + 112, kSolanY + 42 + 65);
}

// ---- apples --------------------------------------------------------------

int WheelbarrowScene::appleZ(const Apple &a) const {
	// FUN_0044fac0: slot index + 10 per bounce stage
	return (int)(&a - _apples) + a.stage * 10;
}

// FUN_00450270: a new apple ripens at a random spot of the roof; one in
// ten is rotten.
void WheelbarrowScene::spawnApple() {
	Apple *a = nullptr;
	for (int i = 0; i < kMaxApples; i++)
		if (!_apples[i].active) {
			a = &_apples[i];
			break;
		}
	if (!a)
		return;
	a->active = true;
	a->type = 0;
	int pick = _vm->getRandomNumber(kMaxApples - 1);
	a->x = (float)kSpawnTable[pick][0];
	a->y = (float)kSpawnTable[pick][1];
	a->stage = kSpawnTable[pick][2];
	a->vy = 0;
	a->state = 0;
	if (_vm->getRandomNumber(99) < 10)
		a->type = 1;
	debug(1, "Wheelbarrow: %s apple at %d,%d stage %d", a->type ? "rotten" : "good", (int)a->x, (int)a->y, a->stage);
	setAppleState(*a, 0);
}

// FUN_0044f980: switches the apple's clip: "<apple|rotten>_<state>". State
// 1 (falling) loops and is only started by the first bounce; the others
// play at once.
void WheelbarrowScene::setAppleState(Apple &a, int state) {
	a.elem.anim->remove();
	a.elem.def.loop = state == 1;
	if (a.type != 0 && a.type != 1)
		return;
	Common::String name = Common::String::format(a.type == 0 ? "apple_%d" : "rotten_%d", state);
	Common::strlcpy(a.elem.name, name.c_str(), sizeof(a.elem.name));
	a.elem.def.smacker = 1;
	a.elem.def.visible = 1;
	a.elem.def.transparent = 1;
	int x = roundPos(a.x), y = roundPos(a.y);
	a.elem.anim->add(x, y, appleZ(a));
	// SCENE_SetAnimCenter: (x, y) is the centre of the sprite
	Common::Rect r = a.elem.anim->rect();
	a.elem.anim->setPos(x - r.width() / 2, y - r.height() / 2);
	if (state != 1)
		a.elem.anim->play();
	a.state = state;
}

// FUN_0044f250
void WheelbarrowScene::removeApple(Apple &a) {
	a.elem.anim->remove();
	a.active = false;
}

// FUN_0044f220
void WheelbarrowScene::clearApples() {
	for (int i = 0; i < kMaxApples; i++)
		if (_apples[i].active)
			removeApple(_apples[i]);
}

WheelbarrowScene::Apple *WheelbarrowScene::appleFor(Anim *anim) {
	// FUN_0044f840
	for (int i = 0; i < kMaxApples; i++)
		if (_apples[i].active && _apples[i].elem.anim == anim)
			return &_apples[i];
	return nullptr;
}

// FUN_0044fcc0: moves every apple; false when the last life has been lost.
bool WheelbarrowScene::updateApples(float dt) {
	Common::Rect barrow = barrowRect();
	for (int i = 0; i < kMaxApples; i++)
		if (_apples[i].active && !updateApple(_apples[i], barrow, dt))
			return false;
	return true;
}

// FUN_0044fe50: a falling apple moves; if it is past the eaves and the
// area it swept touches the wheelbarrow it is caught: a good one counts
// (unless the wheelbarrow is full, then it drops through), a rotten one
// spoils the whole load.
bool WheelbarrowScene::updateApple(Apple &a, const Common::Rect &barrow, float dt) {
	if (a.state != 1)
		return true;
	Common::Rect before = a.elem.anim->rect();
	if (!moveApple(a, dt))
		return false;
	if (!a.active)
		return true;
	Common::Rect swept = a.elem.anim->rect();
	swept.extend(before);
	if (a.stage != 4 || !swept.intersects(barrow))
		return true;

	int look = fullness();
	if (a.type == 0) {
		if (_count > kCounterSlots - 1)
			return true;
		playSound("cart", 3);
		_count++;
		// 10 + 2 points per level
		addScore((int)((_level * 0.2f + 1.0f) * 10.0f));
		debug(1, "Wheelbarrow: caught an apple, %d in the wheelbarrow, score %d", _count, _score);
	} else if (a.type == 1) {
		playSound("splash", 4);
		_count = 0;
		debug(1, "Wheelbarrow: caught a rotten apple, load spoiled");
	}
	showCounter(_count);
	removeApple(a);
	if (look != fullness())
		setSolan(_solanState);
	return true;
}

// FUN_0044ff80: gravity, the bounces off the roof rows and the ground.
// Returns false when a good apple hit the ground and took the last life.
bool WheelbarrowScene::moveApple(Apple &a, float dt) {
	a.y += a.vy * dt;
	a.vy += (float)kGravity * dt;
	if (a.y > (float)kAppleLimitY) {
		removeApple(a);
		return true;
	}
	int x = roundPos(a.x), y = roundPos(a.y);
	Common::Rect r = a.elem.anim->rect();
	a.elem.anim->setPos(x - r.width() / 2, y - r.height() / 2);
	if (a.stage >= 5 || (float)kRoofTable[a.stage][0] > a.y)
		return true;

	int action = kRoofTable[a.stage][1];
	bounceSound(a, action);
	a.vy = -(kBounce[action] * a.vy);
	a.stage++;
	a.elem.anim->setZ(appleZ(a));
	if (!a.elem.anim->isPlaying())
		a.elem.anim->play();
	if (action == 3) {
		if (a.type == 0)
			return loseLife();
		if (a.type == 1)
			setAppleState(a, 2);
	}
	return true;
}

// FUN_00450110
void WheelbarrowScene::bounceSound(const Apple &a, int action) {
	if (action == 0 || action == 1) {
		if (a.type == 0)
			playSound("roof", 4);
		else if (a.type == 1)
			playSound("rotten", 3);
	} else if (action == 3) {
		if (a.type == 0)
			playSound("ground", 2);
		else if (a.type == 1)
			playSound("splash", 4);
	}
}

// FUN_004500d0
bool WheelbarrowScene::loseLife() {
	_lives--;
	playSound("lifedown", 1);
	debug(1, "Wheelbarrow: apple on the ground, %d lives left", _lives);
	drawLives();
	return _lives > 0;
}

// ---- score board ---------------------------------------------------------

// FUN_0044f8d0: every 3000 points give a life.
void WheelbarrowScene::addScore(int points) {
	int old = _score;
	_score += points;
	drawScore();
	if (old / kLifeEvery < _score / kLifeEvery) {
		_lives++;
		playSound("lifeup", 1);
		drawLives();
	}
}

void WheelbarrowScene::drawText(const char *area, const Common::String &text) {
	Graphics::ManagedSurface *s = anim(area)->surface();
	s->fillRect(Common::Rect(0, 0, s->w, s->h), s->format.RGBToColor(0, 255, 0));
	_font.drawStringCentred(*s, Common::Rect(0, 0, s->w, s->h), text);
}

// FUN_0044f4e0
void WheelbarrowScene::drawScore() {
	drawText("scoretext", Common::String::format("%d", _score));
}

// FUN_0044f4a0
void WheelbarrowScene::drawLives() {
	drawText("lifetext", Common::String::format("%d", _lives));
}

// FUN_0044f440: n apples in the counter at the top.
void WheelbarrowScene::showCounter(int n) {
	for (int i = 0; i < kCounterSlots; i++) {
		Anim *a = _counter[i].anim;
		if (!a->isAdded() && i < n)
			a->add(Anim::kDefaultPos, Anim::kDefaultPos, kSolanZ);
		else if (a->isAdded() && n <= i)
			a->remove();
	}
}

// FUN_0044f590: n pies up the right edge, each behind the one below.
void WheelbarrowScene::showPies(int n) {
	for (int i = 0; i < kMaxPies; i++) {
		Anim *a = _pies[i].anim;
		if (!a->isAdded() && i < n)
			a->add(Anim::kDefaultPos, Anim::kDefaultPos, i - 100);
		else if (a->isAdded() && n <= i)
			a->remove();
	}
}

// FUN_0044f280: the basket with n apples (four bitmaps), -1 = none.
void WheelbarrowScene::showBasket(int n) {
	_basket.anim->remove();
	if (n < 0)
		return;
	int idx = _basketCapacity ? n * 4 / _basketCapacity : 0;
	idx = CLIP(idx, 0, 3);
	Common::String name = Common::String::format("basket%d", idx);
	Common::strlcpy(_basket.name, name.c_str(), sizeof(_basket.name));
	_basket.anim->add(Anim::kDefaultPos, Anim::kDefaultPos, 50);
}

// FUN_0044f700: SCENE_PlayAnimClone of "<base><1..variants>".
void WheelbarrowScene::playSound(const char *base, int variants) {
	Common::String name = Common::String::format("%s%d", base, (int)_vm->getRandomNumber(variants - 1) + 1);
	Element *e = nullptr;
	for (int i = 0; i < kSfxClones; i++)
		if (!_sfx[i].anim->isAdded()) {
			e = &_sfx[i];
			break;
		}
	if (!e) {
		e = &_sfx[0];
		e->anim->remove();
	}
	Common::strlcpy(e->name, name.c_str(), sizeof(e->name));
	e->anim->add(0, 0, 0);
	e->anim->setRemoveWhenDone(true);
	e->anim->play();
}

// ---- events --------------------------------------------------------------

void WheelbarrowScene::highlightButton(int hotspot) {
	if (hotspot != kHotspotHelp && hotspot != kHotspotExit)
		hotspot = 0;
	if (hotspot == _highlighted)
		return;
	if (_highlighted == kHotspotHelp) {
		removeAnim("help_1");
		addAnim("help_0", 0, 0, kSolanZ);
	} else if (_highlighted == kHotspotExit) {
		removeAnim("exit_1");
		addAnim("exit_0", 728, 0, kSolanZ);
	}
	_highlighted = hotspot;
	if (hotspot == kHotspotHelp) {
		removeAnim("help_0");
		addAnim("help_1", 0, 0, kSolanZ);
	} else if (hotspot == kHotspotExit) {
		removeAnim("exit_0");
		addAnim("exit_1", 728, 0, kSolanZ);
	}
}

void WheelbarrowScene::onMouseMove(int hotspot, int x, int y) {
	highlightButton(hotspot);
}

// FUN_0044f630 (start button) and the BUTTON module's 0x116 -> FUN_00429320.
void WheelbarrowScene::onMouseDown(int hotspot, int x, int y) {
	switch (hotspot) {
	case kHotspotStart:
		startGame(0);
		break;
	case kHotspotHelp:
		_vm->showHelp();
		break;
	case kHotspotExit:
		_vm->endGame();
		break;
	default:
		break;
	}
}

// FUN_0044f640: key down. Left/right start walking, up tips the
// wheelbarrow when Solan stands at the basket with something in it.
void WheelbarrowScene::onKey(const Common::KeyState &key) {
	if (key.keycode == Common::KEYCODE_ESCAPE) {
		_vm->endGame();
		return;
	}
	if (key.keycode == Common::KEYCODE_LEFT)
		_leftHeld = true;
	else if (key.keycode == Common::KEYCODE_RIGHT)
		_rightHeld = true;
	if (!_running || _solanState == kDump)
		return;

	switch (key.keycode) {
	case Common::KEYCODE_LEFT:
		if (_solanState != kCycle || _dir != 0) {
			_dir = 0;
			setSolan(kCycle);
		}
		break;
	case Common::KEYCODE_UP:
		if (_solanX <= (float)kBasketX && _dir == 0 && _count > 0 && _basket.anim->isAdded()) {
			playSound("empty", 1);
			setSolan(kDump);
		}
		break;
	case Common::KEYCODE_RIGHT:
		if (_solanState != kCycle || _dir != 1) {
			_dir = 1;
			setSolan(kCycle);
		}
		break;
	default:
		break;
	}
}

// FUN_0044f730: key up stops the walk in that direction.
void WheelbarrowScene::onKeyUp(const Common::KeyState &key) {
	if (key.keycode == Common::KEYCODE_LEFT)
		_leftHeld = false;
	else if (key.keycode == Common::KEYCODE_RIGHT)
		_rightHeld = false;
	if (!_running || _solanState == kDump)
		return;
	if (key.keycode == Common::KEYCODE_LEFT && _dir == 0)
		setSolan(kNeutral);
	else if (key.keycode == Common::KEYCODE_RIGHT && _dir == 1)
		setSolan(kNeutral);
}

// FUN_0044f780
void WheelbarrowScene::onAnimFinished(Anim *a) {
	Apple *apple = appleFor(a);
	if (apple) {
		// FUN_0044f950: ripe -> falling; splat -> gone
		if (apple->state == 0)
			setAppleState(*apple, 1);
		else if (apple->state == 2)
			removeApple(*apple);
		return;
	}
	if (a == _solan.anim) {
		if (_solanState == kDump)
			dumpBarrow();
		else if (_solanState != kStop && _solanState != kBonk)
			return;
		resumeFromKeys();
		return;
	}
	const Common::String n(a->name());
	if (n == "pickup") {
		removeAnim("pickup");
		playAnim("pie");
	} else if (n == "pie") {
		removeAnim("pie");
		if (_running) {
			_level++;
			setLevel(_level);
			addScore(kLevelBonus);
			debug(1, "Wheelbarrow: level %d, score %d", _level, _score);
			playAnim("putdown");
		}
	} else if (n == "putdown") {
		removeAnim("putdown");
		showBasket(0);
	}
}

// FUN_0044fb70: the frame tick, dt in seconds capped at 0.1.
void WheelbarrowScene::onUpdate() {
	if (!_running)
		return;
	uint32 now = g_engine->getGameMillis();
	float dt = (float)(int)(now - _lastTick) * 0.001f;
	if (dt >= 0.1f)
		dt = 0.1f;
	_lastTick = now;
	moveSolan(dt);
	if (!updateApples(dt))
		gameOver();
}

} // End of namespace Flaaklypa
