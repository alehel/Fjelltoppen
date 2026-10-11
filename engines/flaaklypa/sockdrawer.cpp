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
#include "flaaklypa/dialog.h"
#include "flaaklypa/hiscore.h"
#include "flaaklypa/sockdrawer.h"
#include "flaaklypa/tournament.h"

namespace Flaaklypa {

// Level table at 0x49ef48: socks in the cabinet, number of sock kinds to
// choose from, time in ms. Level 4 really has 950 s (a typo of the original).
const SockdrawerScene::Level SockdrawerScene::kLevelTable[kLevels] = {
	{ 4, 3, 20000 }, { 6, 4, 35000 }, { 8, 5, 50000 }, { 10, 6, 75000 }, { 12, 7, 950000 },
	{ 14, 8, 110000 }, { 16, 9, 110000 }, { 18, 10, 90000 }, { 20, 12, 80000 }, { 20, 14, 70000 },
	{ 20, 16, 60000 }, { 20, 18, 60000 }, { 20, 20, 50000 }, { 20, 22, 50000 }, { 20, 24, 40000 }
};

// Sock bitmaps (Sock<n+1>.bmp) in the order they come into play (0x49effc):
// the first ones are easy to tell apart, the later ones look alike.
const int SockdrawerScene::kSockOrder[kSockTypes] = {
	0, 1, 2, 6, 7, 9, 10, 13, 16, 18, 5, 8, 11, 12, 14, 17, 3, 4, 15, 19, 20, 21, 22, 23
};

// Medal thresholds passed to the high score registration (0x49f080).
const int SockdrawerScene::kMedals[4] = { 2500, 5000, 10000, 20000 };

enum {
	kDrawerX0 = 0xbd,       ///< DAT_0067ef40
	kDrawerY0 = 0x76,
	kDrawerSpacing = 0x72,
	kSockOffsetX = 15,
	kSockOffsetY = 13,
	kPairDelayMs = 400,
	kPairPoints = 25,
	kTimerFrames = 30,      ///< timertop00..30, timerbottom02..32
	kMaxLevel = 14
};

SockdrawerScene::SockdrawerScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def),
	_state(kStateIdle), _score(0), _level(0), _xBase(kDrawerX0), _levelStart(0), _timerFrame(0),
	_pairTimer(false), _highlighted(0) {
	for (int i = 0; i < kDrawers; i++) {
		Drawer &d = _drawers[i];
		d.hasSock = false;
		d.closed = true;
		d.variant = 0;
		d.sockType = 0;
	}
}

SockdrawerScene::~SockdrawerScene() {
	for (auto *a : _ownAnims) {
		a->remove();
		delete a;
	}
}

bool SockdrawerScene::load() {
	if (!Scene::load())
		return false;
	_font14.load("Amerigo BT_14_");
	_font18.load("Amerigo BT_18_");
	return true;
}

// ---- elements with changing file names -------------------------------------

void SockdrawerScene::initElement(Element &e, bool smacker, bool visible, bool transparent, bool loop, int hotspot, int x, int y, int z) {
	e.def.name = e.name;
	e.def.smacker = smacker;
	e.def.visible = visible;
	e.def.transparent = transparent;
	e.def.loop = loop;
	e.def.hotspot = hotspot;
	e.def.x = x;
	e.def.y = y;
	e.def.group = 0;
	e.def.zOrder = z;
	e.def.overlay = 0;
	if (!e.anim) {
		e.anim = new Anim(this, &e.def);
		_ownAnims.push_back(e.anim);
	}
}

void SockdrawerScene::setElementName(Element &e, const char *name) {
	Common::strlcpy(e.name, name, sizeof(e.name));
}

// FUN_0044baf0: the sound effects are played as clones of the audio only
// slot 0x4de4a0 (SCENE_PlayAnimClone). Here each sound has its own element.
void SockdrawerScene::playSound(const char *name) {
	for (int i = 0; i < 4; i++) {
		Element &e = _sounds[i];
		if (e.name[0] && scumm_stricmp(e.name, name))
			continue;
		if (!e.name[0]) {
			initElement(e, true, false, false, false, 0, 0, 0, 0);
			setElementName(e, name);
		}
		if (e.anim->isPlaying())
			e.anim->stop();
		e.anim->play();
		return;
	}
	warning("Sockdrawer: no free sound slot for '%s'", name);
}

// ---- screen --------------------------------------------------------------

// FUN_0044b720
void SockdrawerScene::onInit(int arg) {
	const uint32 green = _vm->_screen->format.RGBToColor(0, 255, 0);

	_xBase = kDrawerX0;
	// Scene title in Amerigo BT_18_ centred in (335, 44)-(601, 86), score in
	// Amerigo BT_14_ centred in (55, 192)-(122, 216), both at z -20.
	defineSurfaceAnim("titletext", 266, 42, green)->add(335, 44, kZText);
	drawText("titletext", _font18, _vm->getString("sockdrawer:SCENENAME"));
	defineSurfaceAnim("scoretext", 67, 24, green)->add(55, 192, kZText);

	defineAnim("help_0", false, true, kHotspotHelp, 0, 0, 0)->add(0, 0, 0);
	defineAnim("help_1", false, true, kHotspotHelp, 0, 0, 0);
	defineAnim("exit_0", false, true, kHotspotExit, 728, 0, 0)->add(728, 0, 0);
	defineAnim("exit_1", false, true, kHotspotExit, 728, 0, 0);
	_highlighted = 0;

	addAnim("start", Anim::kDefaultPos, Anim::kDefaultPos, kZStart);
	addAnim("points", Anim::kDefaultPos, Anim::kDefaultPos, kZTimerClips);
	_pairTimer = false;

	initDrawers();
	resetGame();
	resetTimerDisplay();
	playMusic("subgame3");

	// Tournament mode (FUN_00419490): "Is <player> ready?" (FUN_00419660),
	// then the game starts at once (as a click on the start cabinet, but
	// without FUN_0040d8a0(1)) at the player's level (FUN_004194a0: 0 easy,
	// 1 medium, 2 hard) * 5, i.e. table entry 0, 5 or 10.
	if (_vm->tournament()->isActive()) {
		_vm->tournament()->showPlayerReady();
		resetGame();
		int level = 0;
		_vm->tournament()->player(nullptr, &level);
		_level = level * 5;
		debug(1, "Sockdrawer: tournament mode, starting at level %d", _level);
		startRound();
		playAnim("start");
	}
}

// FUN_0044b860: the hatches, 5 x 4, 114 pixels apart. Each shows the first
// frame of its "open" clip (a closed hatch) until it is played.
void SockdrawerScene::initDrawers() {
	for (int i = 0; i < kDrawers; i++) {
		Drawer &d = _drawers[i];
		d.hasSock = false;
		d.closed = true;
		d.variant = _vm->getRandomNumber(0);   // rand() % 1: only drawer1 exists
		int x = _xBase + (i % kDrawerCols) * kDrawerSpacing;
		int y = (i / kDrawerCols) * kDrawerSpacing + kDrawerY0;
		initElement(d.door, true, true, true, false, 0, x, y, kZDoor);
		// The original passes (closed == 0) here, so the hatch starts out
		// as the first frame of the "open" clip, which shows it closed.
		setElementName(d.door, Common::String::format("drawer%dopen", d.variant + 1).c_str());
		d.door.anim->remove();
		d.door.anim->add(Anim::kDefaultPos, Anim::kDefaultPos, kZDoor);
		initElement(d.sock, false, true, true, false, 0, x + kSockOffsetX, y + kSockOffsetY, kZSock);
		d.sock.name[0] = 0;
		d.sock.anim->remove();
		d.sockType = 0;
	}
}

// FUN_0044b950
void SockdrawerScene::setDoorName(Drawer &d) {
	setElementName(d.door, Common::String::format("drawer%d%s", d.variant + 1, d.closed ? "close" : "open").c_str());
}

// FUN_0044b980: the hourglass with all sand at the bottom.
void SockdrawerScene::resetTimerDisplay() {
	if (_timerTop.anim && _timerTop.anim->isAdded())
		_timerTop.anim->remove();
	if (_timerBottom.anim && _timerBottom.anim->isAdded())
		_timerBottom.anim->remove();
	if (isAnimAdded("timermiddlerunning"))
		removeAnim("timermiddlerunning");
	initElement(_timerBottom, false, true, true, false, 0, 46, 473, kZTimerBitmaps);
	setElementName(_timerBottom, Common::String::format("timerbottom%02d", kTimerFrames + 2).c_str());
	_timerBottom.anim->add(Anim::kDefaultPos, Anim::kDefaultPos, kZTimerBitmaps);
	_timerFrame = kTimerFrames + 1;
}

// FUN_0044ba30
void SockdrawerScene::resetGame() {
	_level = 0;
	_score = 0;
	_state = kStateIdle;
	setScoreText(0);
}

// FUN_0044ba50
void SockdrawerScene::setScoreText(int score) {
	drawText("scoretext", _font14, Common::String::format("%d", score));
}

void SockdrawerScene::drawText(const char *name, const BitmapFont &font, const Common::String &text) {
	Graphics::ManagedSurface *s = anim(name)->surface();
	s->fillRect(Common::Rect(0, 0, s->w, s->h), s->format.RGBToColor(0, 255, 0));
	font.drawStringCentred(*s, Common::Rect(0, 0, s->w, s->h), text);
}

// FUN_0044ba90: close every hatch, then carry on with the next level once
// they have closed.
void SockdrawerScene::startRound() {
	closeAllDrawers();
	_state = kStateClosing;
	step();
}

// FUN_0044bab0
void SockdrawerScene::closeAllDrawers() {
	for (int i = 0; i < kDrawers; i++)
		if (!_drawers[i].closed)
			closeDrawer(i);
	playSound("allclose");
}

// FUN_0044bb30
void SockdrawerScene::closeDrawer(int i) {
	Drawer &d = _drawers[i];
	d.door.anim->remove();
	d.closed = true;
	setDoorName(d);
	d.door.anim->add(Anim::kDefaultPos, Anim::kDefaultPos, kZDoor);
	d.door.anim->play();
}

// FUN_0044bb80: the state machine, run after every hatch clip has ended.
void SockdrawerScene::step() {
	if (_state == kStateClosing) {
		if (noDoorPlaying())
			newLevel();
	} else if (_state == kStateOpening) {
		if (noDoorPlaying())
			startTimerRotate();
	} else if (_state == kStatePlaying) {
		checkPair();
	}
}

// FUN_0044bbb0
bool SockdrawerScene::noDoorPlaying() {
	for (int i = 0; i < kDrawers; i++)
		if (doorPlaying(i))
			return false;
	return true;
}

// FUN_0044bbe0
bool SockdrawerScene::doorPlaying(int i) {
	return _drawers[i].door.anim->isPlaying();
}

// FUN_0044bc10: the hourglass is turned over.
void SockdrawerScene::startTimerRotate() {
	if (_timerTop.anim && _timerTop.anim->isAdded())
		_timerTop.anim->remove();
	if (isAnimAdded("timermiddlerunning"))
		removeAnim("timermiddlerunning");
	if (_timerBottom.anim && _timerBottom.anim->isAdded())
		_timerBottom.anim->remove();
	addAnim("timerrotate", Anim::kDefaultPos, Anim::kDefaultPos, kZTimerBitmaps);
	playAnim("timerrotate");
}

// FUN_0044bc90
void SockdrawerScene::newLevel() {
	removeAllSocks();
	placeSocks();
	openEmptyDrawers();
	_state = kStateOpening;
	step();
}

// FUN_0044bcb0
void SockdrawerScene::removeAllSocks() {
	for (int i = 0; i < kDrawers; i++)
		if (_drawers[i].hasSock)
			removeSock(i);
}

// FUN_0044bce0
void SockdrawerScene::removeSock(int i) {
	_drawers[i].sock.anim->remove();
	_drawers[i].hasSock = false;
}

// FUN_0044bd10
void SockdrawerScene::placeSocks() {
	int pairs = kLevelTable[_level].socks / 2;
	for (int i = 0; i < pairs; i++)
		placePair();
}

// FUN_0044bd60
void SockdrawerScene::placePair() {
	int type = pickSockType();
	int d = pickFreeDrawer();
	putSock(d, type);
	d = pickFreeDrawer();
	putSock(d, type);
}

// FUN_0044bd90: a random kind among the first "kinds" of kSockOrder that is
// not in the cabinet yet; after 100 misses the first unused one.
int SockdrawerScene::pickSockType() {
	int kinds = kLevelTable[_level].kinds;
	for (int i = 0; i < 100; i++) {
		int type = kSockOrder[_vm->getRandomNumber(kinds - 1)];
		if (!sockTypeInUse(type))
			return type;
	}
	return firstFreeSockType();
}

// FUN_0044bde0
bool SockdrawerScene::sockTypeInUse(int type) {
	for (int i = 0; i < kDrawers; i++)
		if (_drawers[i].hasSock && _drawers[i].sockType == type)
			return true;
	return false;
}

// FUN_0044be10
int SockdrawerScene::firstFreeSockType() {
	int kinds = kLevelTable[_level].kinds;
	for (int i = 0; i < kinds; i++)
		if (!sockTypeInUse(kSockOrder[i]))
			return kSockOrder[i];
	return -1;
}

// FUN_0044be60
int SockdrawerScene::pickFreeDrawer() {
	for (int i = 0; i < 100; i++) {
		int d = _vm->getRandomNumber(kDrawers - 1);
		if (!_drawers[d].hasSock)
			return d;
	}
	return firstFreeDrawer();
}

// FUN_0044be90
int SockdrawerScene::firstFreeDrawer() {
	for (int i = 0; i < kDrawers; i++)
		if (!_drawers[i].hasSock)
			return i;
	return -1;
}

// FUN_0044beb0 (+ FUN_0044bf00): bitmap/Sock<type+1>.bmp behind the hatch.
void SockdrawerScene::putSock(int drawer, int type) {
	if (drawer < 0)
		return;
	Drawer &d = _drawers[drawer];
	setElementName(d.sock, Common::String::format("Sock%02d", type + 1).c_str());
	d.sock.anim->add(Anim::kDefaultPos, Anim::kDefaultPos, kZSock);
	d.sockType = type;
	d.hasSock = true;
}

// FUN_0044bf20: hatches without a sock stand open.
void SockdrawerScene::openEmptyDrawers() {
	for (int i = 0; i < kDrawers; i++)
		if (!_drawers[i].hasSock)
			openDrawer(i);
	playSound("allopen");
}

// FUN_0044bf60
void SockdrawerScene::openDrawer(int i) {
	Drawer &d = _drawers[i];
	d.door.anim->remove();
	d.closed = false;
	setDoorName(d);
	d.door.anim->add(Anim::kDefaultPos, Anim::kDefaultPos, kZDoor);
	d.door.anim->play();
}

// FUN_0044bfb0: once two socks show, the pair is judged 400 ms later; a
// matching pair scores right away.
void SockdrawerScene::checkPair() {
	if (_state == kStateIdle)
		return;
	int idx[kDrawers];
	if (countOpenSocks(idx) != 2)
		return;
	setTimer(kTimerPair, kPairDelayMs);
	_pairTimer = true;
	if (_drawers[idx[0]].sockType == _drawers[idx[1]].sockType)
		addPoints(kPairPoints);
}

// FUN_0044c040: open hatches with a sock behind them.
int SockdrawerScene::countOpenSocks(int *idx) {
	int n = 0;
	for (int i = 0; i < kDrawers; i++)
		if (_drawers[i].hasSock && !_drawers[i].closed) {
			if (idx)
				idx[n] = i;
			n++;
		}
	return n;
}

// FUN_0044c080
void SockdrawerScene::addPoints(int n) {
	_score += n;
	// SCENE_PlayAnim restarts a clip that is still playing.
	if (isAnimPlaying("points"))
		anim("points")->stop();
	playAnim("points");
	setScoreText(_score);
}

// ---- input ---------------------------------------------------------------

// FUN_0044c0e0
void SockdrawerScene::onMouseDown(int hotspot, int x, int y) {
	if (hotspot == kHotspotExit) {
		// Button 2 -> FUN_0040cc30: back to the parent scene (asks
		// "gamec:SUBGAMEABORT" while a game runs).
		_vm->endGame();
		return;
	}
	if (hotspot == kHotspotHelp) {
		_vm->showHelp();
		return;
	}
	if (hotspot == kHotspotStart) {
		if (_state == kStateIdle) {
			_vm->setGameRunning(true);
			resetGame();
			startRound();
			playAnim("start");
		}
	} else if (hotspot > 100 && hotspot < 121) {
		drawerClicked(hotspot - kHotspotDrawer0);
	}
}

// FUN_0044c130: a closed hatch opens while fewer than two socks show.
void SockdrawerScene::drawerClicked(int i) {
	if (_state != kStatePlaying || !_drawers[i].closed)
		return;
	if (doorPlaying(i))
		return;
	if (countOpenSocks(nullptr) < 2) {
		openDrawer(i);
		playSound("oneopen");
	}
}

void SockdrawerScene::highlightButton(int hotspot) {
	if (hotspot != kHotspotHelp && hotspot != kHotspotExit)
		hotspot = 0;
	if (hotspot == _highlighted)
		return;
	if (_highlighted == kHotspotHelp) {
		removeAnim("help_1");
		addAnim("help_0", 0, 0, 0);
	} else if (_highlighted == kHotspotExit) {
		removeAnim("exit_1");
		addAnim("exit_0", 728, 0, 0);
	}
	_highlighted = hotspot;
	if (hotspot == kHotspotHelp) {
		removeAnim("help_0");
		addAnim("help_1", 0, 0, 0);
	} else if (hotspot == kHotspotExit) {
		removeAnim("exit_0");
		addAnim("exit_1", 728, 0, 0);
	}
}

void SockdrawerScene::onMouseMove(int hotspot, int x, int y) {
	highlightButton(hotspot);
}

// ---- animation events ----------------------------------------------------

// FUN_0044c190
void SockdrawerScene::onAnimFinished(Anim *a) {
	for (int i = 0; i < kDrawers; i++)
		if (_drawers[i].door.anim == a) {
			step();
			return;
		}
	const Common::String n(a->name());
	if (n == "start") {
		a->remove();
	} else if (n == "timermiddlestart") {
		timerStartDone();
	} else if (n == "timermiddleend") {
		a->remove();
		gameOver();
	} else if (n == "timerrotate") {
		timerRotateDone();
	}
}

// FUN_0044c230: the hourglass has been turned; the level clock starts.
void SockdrawerScene::timerRotateDone() {
	if (isAnimAdded("timerrotate"))
		removeAnim("timerrotate");
	setTimerFrame(0);
	addAnim("timermiddlestart", Anim::kDefaultPos, Anim::kDefaultPos, kZTimerClips);
	playAnim("timermiddlestart");
	_levelStart = g_engine->getGameMillis();
	_state = kStatePlaying;
}

// FUN_0044c290: timertop<frame> and timerbottom<frame + 2>.
void SockdrawerScene::setTimerFrame(int frame) {
	if (_timerTop.anim && _timerTop.anim->isAdded())
		_timerTop.anim->remove();
	if (_timerBottom.anim && _timerBottom.anim->isAdded())
		_timerBottom.anim->remove();
	initElement(_timerTop, false, true, true, false, 0, 45, 359, kZTimerBitmaps);
	setElementName(_timerTop, Common::String::format("timertop%02d", frame).c_str());
	initElement(_timerBottom, false, true, true, false, 0, 46, 473, kZTimerBitmaps);
	setElementName(_timerBottom, Common::String::format("timerbottom%02d", frame + 2).c_str());
	_timerTop.anim->add(Anim::kDefaultPos, Anim::kDefaultPos, kZTimerBitmaps);
	_timerBottom.anim->add(Anim::kDefaultPos, Anim::kDefaultPos, kZTimerBitmaps);
	_timerFrame = frame;
}

// FUN_0044c340
void SockdrawerScene::timerStartDone() {
	if (isAnimAdded("timermiddlestart"))
		removeAnim("timermiddlestart");
	addAnim("timermiddlerunning", Anim::kDefaultPos, Anim::kDefaultPos, kZTimerClips);
	playAnim("timermiddlerunning");
}

// FUN_0044c380: time is up.
void SockdrawerScene::gameOver() {
	_vm->setGameRunning(false);
	_state = kStateIdle;
	debug(1, "Sockdrawer: game over at level %d with %d points", _level, _score);
	// Modal: the state is idle, so the pair timer and the hatches do nothing
	// in the nested frames.
	_vm->messageBox("interfaceh:GAMEOVER", "sockdrawer:ENDMESSAGE", MessageBox::kButtonOk);
	// FUN_0041f9a0(0, score, 0x49f080, 0x44c3e0)
	registerScore(_vm, "", _score, kMedals);
	// In tournament mode the score went to the tournament, which has moved
	// on (the next player or the results), and the callback is not run.
	if (_vm->tournament()->isActive())
		return;
	// The callback (0x44c3e0) of the high score dialog: close the hatches
	// and show the start cabinet again.
	closeAllDrawers();
	addAnim("start", Anim::kDefaultPos, Anim::kDefaultPos, kZStart);
}

// ---- timer ---------------------------------------------------------------

// FUN_0044c400: judge the two open socks.
void SockdrawerScene::onTimer(int id, int data) {
	if (id != kTimerPair)
		return;
	_pairTimer = false;
	if (_state == kStateIdle)
		return;
	int idx[kDrawers];
	if (countOpenSocks(idx) != 2)
		return;
	if (_drawers[idx[0]].sockType != _drawers[idx[1]].sockType) {
		closeDrawer(idx[0]);
		closeDrawer(idx[1]);
		playSound("oneclose");
		return;
	}
	removeSock(idx[0]);
	removeSock(idx[1]);
	if (allSocksGone()) {
		// Time bonus: 200 * level * remaining / level time.
		const Level &lv = kLevelTable[_level];
		int remaining = (int)(_levelStart + lv.timeMs - g_engine->getGameMillis());
		int bonus = (int)((float)(_level * 200) * (float)remaining / (float)lv.timeMs);
		addPoints(bonus);
		nextLevel();
		startRound();
	}
}

// FUN_0044c520
void SockdrawerScene::nextLevel() {
	if (_level < kMaxLevel)
		_level++;
}

// FUN_0044c540
bool SockdrawerScene::allSocksGone() {
	for (int i = 0; i < kDrawers; i++)
		if (_drawers[i].hasSock)
			return false;
	return true;
}

// ---- frame tick ----------------------------------------------------------

// FUN_0044c560
void SockdrawerScene::onUpdate() {
	if (_state != kStatePlaying)
		return;
	if (updateTimer())
		return;
	if (isAnimAdded("timermiddlerunning"))
		removeAnim("timermiddlerunning");
	resetTimerDisplay();
	addAnim("timermiddleend", Anim::kDefaultPos, Anim::kDefaultPos, kZTimerClips);
	playAnim("timermiddleend");
	_state = kStateIdle;
}

// FUN_0044c5d0: the sand runs through 30 frames; false when time is up.
bool SockdrawerScene::updateTimer() {
	int timeMs = kLevelTable[_level].timeMs;
	int elapsed = (int)(g_engine->getGameMillis() - _levelStart);
	if (timeMs < elapsed)
		return false;
	int frame = (int)(30.0f / (float)timeMs * (float)elapsed);
	if (frame != _timerFrame)
		setTimerFrame(frame);
	return true;
}

} // End of namespace Flaaklypa
