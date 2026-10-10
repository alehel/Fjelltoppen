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

// "Dra meg baklengs!" - the whackamole sub game. The original's handler is
// at 0x44d980 and dispatches: 0x103 -> FUN_0044e1f0 (init), 0x104 ->
// FUN_0044e300 (close), 0x105 -> FUN_0044da80 (mouse down), 0x108/0x109 ->
// FUN_0044ed00 (mouse move), 0x110 -> FUN_0044e350 (anim finished), 0x111
// -> FUN_0044ecd0 (timer), 0x112 -> FUN_0044e740 (tick), 0x116 ->
// FUN_0044ece0 (buttons).

#include "common/debug.h"
#include "common/system.h"
#include "common/textconsole.h"

#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/music.h"
#include "flaaklypa/dialog.h"
#include "flaaklypa/whackamole.h"

namespace Flaaklypa {

// Level table at 0x4e59f8 (18 entries of 5 ints). FUN_0044df80 wraps around
// after level 18 with halved durations.
const WhackamoleScene::LevelDef WhackamoleScene::kLevels[kLevelCount] = {
	{ 21,  6, 3,  4, 60000 },
	{ 24,  9, 3,  4, 60000 },
	{ 27, 12, 3,  5, 60000 },
	{ 30,  9, 3,  5, 60000 },
	{ 33, 12, 3,  6, 60000 },
	{ 36, 15, 3,  6, 60000 },
	{ 39, 18, 2,  7, 50000 },
	{ 42, 21, 2,  8, 50000 },
	{ 45, 24, 2,  9, 50000 },
	{ 48, 27, 2, 12, 50000 },
	{ 51, 30, 2, 14, 50000 },
	{ 54, 33, 2, 15, 50000 },
	{ 57, 36, 1, 16, 40000 },
	{ 60, 39, 1, 17, 40000 },
	{ 63, 42, 1, 18, 40000 },
	{ 66, 45, 1, 19, 40000 },
	{ 69, 48, 1, 20, 40000 },
	{ 72, 51, 1, 21, 40000 }
};

// The clip arrays of the original (12 x 0xa8 bytes each, "bird house 1a.lst"
// at 0x4debb8 first) hold the houses 1..7 followed by the nests 1..5. Some
// of the unmirrored bird clips carry a ".lst" suffix in their file names.
const char *const WhackamoleScene::kHoleNames[kHoles] = {
	"house 1", "house 2", "house 3", "house 4", "house 5", "house 6", "house 7",
	"nest 1", "nest 2", "nest 3", "nest 4", "nest 5"
};

const bool WhackamoleScene::kHoleHasLst[kHoles] = {
	true, true, false, false, false, true, false,
	true, true, true, true, true
};

// While a level runs the system cursor is hidden (FUN_0040ef90(0) /
// FUN_0040f140(0) in FUN_0044df80) and the larva clip follows the mouse.
// The scene framework ignores clicks while its cursor is hidden, so an
// empty cursor is mapped to every hotspot of the scene instead.
const CursorEntry WhackamoleScene::kNoCursor[] = {
	{ 0, "" }, { 1, "" }, { 2, "" }, { 3, "" }, { 4, "" }, { 5, "" }, { 6, "" },
	{ 7, "" }, { 8, "" }, { 9, "" }, { 10, "" }, { 11, "" }, { 12, "" },
	{ kHotspotExit, "" }, { kHotspotHelp, "" }, { kHotspotStart, "" },
	{ -1, nullptr }
};

static const int kPercentThreshold = 7000;    ///< DAT_0068a830, times 0.01: 70 %
static const int kLarvaOffsetX = 25;          ///< the larva clip is drawn at mouse - (25, 16)
static const int kLarvaOffsetY = 16;

WhackamoleScene::WhackamoleScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def),
	_highlighted(0), _running(false), _messageBox(false), _balance(0), _appeared(0), _fed(0), _points(0), _level(0),
	_duration(0), _levelStart(0), _levelDef(&kLevels[0]), _birdInterval(1), _squirrelInterval(1),
	_birds(0), _squirrels(0), _nextBird(0), _nextSquirrel(0), _cursorHidden(false) {
	static const char *const kSuffixes[kKindCount] = {
		"a", "b1", "b2", "b3", "c", "ar", "b1r", "b2r", "b3r", "cr", "a", "b1", "c", "b1.s"
	};
	for (int hole = 0; hole < kHoles; hole++) {
		_busy[hole] = false;
		_waits[hole] = 0;
		for (int kind = 0; kind < kKindCount; kind++) {
			const char *animal = (kind >= kSquirrelA && kind <= kSquirrelC) ? "squirrel" : "bird";
			const char *lst = (kind <= kBirdC && kHoleHasLst[hole]) ? ".lst" : "";
			_names[kind][hole] = Common::String::format("%s %s%s%s", animal, kHoleNames[hole], kSuffixes[kind], lst);
			_clips[kind][hole] = anim(_names[kind][hole].c_str());
		}
	}
}

WhackamoleScene::~WhackamoleScene() {
}

bool WhackamoleScene::load() {
	if (!Scene::load())
		return false;
	_font.load("Amerigo BT_14_");
	return true;
}

// ---- helpers -------------------------------------------------------------

// FUN_0040c530: rand() % n, 0 for n == 0.
int WhackamoleScene::rnd(int n) const {
	return n > 0 ? (int)_vm->getRandomNumber(n - 1) : 0;
}

// FUN_0044e720: index of the clip in one of the 12 element arrays, or -1.
int WhackamoleScene::holeOf(Anim *a, Kind kind) const {
	for (int hole = 0; hole < kHoles; hole++)
		if (_clips[kind][hole] == a)
			return hole;
	return -1;
}

// FUN_0044dcf0
Anim *WhackamoleScene::squirrelAt(int hole) const {
	for (int kind = kSquirrelA; kind <= kSquirrelC; kind++)
		if (_clips[kind][hole]->isAdded())
			return _clips[kind][hole];
	return nullptr;
}

// FUN_0044dd50
Anim *WhackamoleScene::birdAt(int hole) const {
	for (int kind = kBirdA; kind <= kBirdC; kind++)
		if (_clips[kind][hole]->isAdded())
			return _clips[kind][hole];
	return nullptr;
}

// FUN_0044dde0
Anim *WhackamoleScene::mirroredBirdAt(int hole) const {
	for (int kind = kBirdAR; kind <= kBirdCR; kind++)
		if (_clips[kind][hole]->isAdded())
			return _clips[kind][hole];
	return nullptr;
}

// FUN_0044e8f0: no animal clip is on the hole.
bool WhackamoleScene::holeFree(int hole) const {
	for (int kind = kBirdA; kind <= kSquirrelC; kind++)
		if (_clips[kind][hole]->isAdded())
			return false;
	return true;
}

// FUN_0044e8c0: ten random tries, -1 when all of them hit occupied holes.
int WhackamoleScene::pickFreeHole() const {
	for (int i = 0; i < 10; i++) {
		int hole = rnd(kHoles);
		if (holeFree(hole))
			return hole;
	}
	return -1;
}

// SCENE_AddAnim(clip, -1, -1, 10) + SCENE_PlayAnim, as the handler does for
// every animal clip.
void WhackamoleScene::startClip(Kind kind, int hole) {
	Anim *a = _clips[kind][hole];
	a->add(Anim::kDefaultPos, Anim::kDefaultPos, kZClips);
	a->play();
}

// FUN_0044e130: removes every animal clip and chirp.
void WhackamoleScene::removeAllClips() {
	for (int kind = kBirdA; kind < kKindCount; kind++)
		for (int hole = 0; hole < kHoles; hole++)
			if (_clips[kind][hole]->isAdded())
				_clips[kind][hole]->remove();
}

// FUN_0044dcb0: the original copies the name into a scratch element and
// plays a clone of it (SCENE_PlayAnimClone), so several can overlap. Here
// the sound effect is restarted instead.
void WhackamoleScene::playSound(const char *name) {
	Anim *a = anim(name);
	if (a->isPlaying())
		a->stop();
	a->play();
}

// FUN_0044df10: the feeding average in percent.
float WhackamoleScene::percent() const {
	if (_appeared == 0)
		return 0.0f;
	return (float)_fed * 100.0f / (float)_appeared;
}

// ---- texts (TEXT module areas, FUN_004079c0 centred) --------------------

void WhackamoleScene::drawText(const char *area, const Common::String &text) {
	Graphics::ManagedSurface *s = anim(area)->surface();
	s->fillRect(Common::Rect(0, 0, s->w, s->h), s->format.RGBToColor(0, 255, 0));
	_font.drawStringCentred(*s, Common::Rect(0, 0, s->w, s->h), text);
}

// FUN_0044de70
void WhackamoleScene::drawPoints() {
	drawText("pointstext", Common::String::format("%s: %d", _vm->getString("whackamole:POINTS").c_str(), _points));
}

// FUN_0044dec0
void WhackamoleScene::drawPercent() {
	drawText("percenttext", Common::String::format("%s: %d", _vm->getString("whackamole:PERCENT").c_str(), (int)percent()));
}

// FUN_0044ea90: remaining time as mm:ss, redrawn every tick in the original.
void WhackamoleScene::drawTime() {
	int left = _duration / 1000 - (int)(g_engine->getGameMillis() - _levelStart) / 1000;
	Common::String text = Common::String::format("%s: %02d:%02d", _vm->getString("whackamole:TIME").c_str(), left / 60, left % 60);
	if (text == _timeText)
		return;
	_timeText = text;
	drawText("timetext", text);
}

// ---- screen --------------------------------------------------------------

// FUN_0044e1f0
void WhackamoleScene::onInit(int arg) {
	_running = false;
	playMusic("ambient10");

	// Text areas (FUN_00407930 with the rectangles at 0x49f0b8.., z 11).
	const uint32 green = _vm->_screen->format.RGBToColor(0, 255, 0);
	defineSurfaceAnim("pointstext", 753 - 618, 204 - 175, green)->add(618, 175, kZText);
	defineSurfaceAnim("leveltext", 714 - 622, 288 - 271, green)->add(622, 271, kZText);
	defineSurfaceAnim("timetext", 717 - 620, 321 - 297, green)->add(620, 297, kZText);
	defineSurfaceAnim("percenttext", 754 - 619, 248 - 224, green)->add(619, 224, kZText);
	_timeText.clear();

	// Buttons (BUTTON module, z 200): help at (2, 1), exit at (726, 2).
	defineAnim("help out", false, true, kHotspotHelp, 2, 1, kZButtons)->add(2, 1, kZButtons);
	defineAnim("help in", false, true, kHotspotHelp, 2, 1, kZButtons);
	defineAnim("exit out", false, true, kHotspotExit, 726, 2, kZButtons)->add(726, 2, kZButtons);
	defineAnim("exit in", false, true, kHotspotExit, 726, 2, kZButtons);
	_highlighted = 0;

	// Sound effects played as clones of the scratch element at 0x4e5b60
	// (visible = 0 in the table). They are 4x4 audio only clips without a
	// palette, so they must not be decoded for drawing; defineAnim() only
	// makes visible elements, hence the cast on its heap allocated def.
	static const char *const kSounds[] = { "false", "thank you on bird language1", "thank you on bird language2" };
	for (int i = 0; i < ARRAYSIZE(kSounds); i++) {
		Anim *a = defineAnim(kSounds[i], true, false, 0, 0, 0);
		const_cast<AnimDef *>(a->def())->visible = 0;
	}

	addAnim("startbuttn", Anim::kDefaultPos, Anim::kDefaultPos, kZButtons);

	// TODO: in tournament mode (FUN_00419490) the original shows the
	// "player ready" box (FUN_00419660) and starts the game at once.
}

// FUN_0044e300
void WhackamoleScene::onClose() {
	_vm->_music->stop();
}

void WhackamoleScene::highlightButton(int hotspot) {
	if (hotspot != kHotspotHelp && hotspot != kHotspotExit)
		hotspot = 0;
	if (hotspot == _highlighted)
		return;
	if (_highlighted == kHotspotHelp) {
		removeAnim("help in");
		addAnim("help out", 2, 1, kZButtons);
	} else if (_highlighted == kHotspotExit) {
		removeAnim("exit in");
		addAnim("exit out", 726, 2, kZButtons);
	}
	_highlighted = hotspot;
	if (hotspot == kHotspotHelp) {
		removeAnim("help out");
		addAnim("help in", 2, 1, kZButtons);
	} else if (hotspot == kHotspotExit) {
		removeAnim("exit out");
		addAnim("exit in", 726, 2, kZButtons);
	}
}

void WhackamoleScene::onMouseMove(int hotspot, int x, int y) {
	highlightButton(hotspot);
}

// ---- game ----------------------------------------------------------------

// FUN_0044df40: the start sign was clicked.
void WhackamoleScene::startGame() {
	removeAnim("startbuttn");
	removeAllClips();
	_level = 0;
	_points = 0;
	// FUN_0040d8a0(1): leaving the scene now asks "gamec:SUBGAMEABORT" first.
	_vm->setGameRunning(true);
	_running = true;
	drawPoints();
	drawPercent();
	startLevel();
}

// FUN_0044df80
void WhackamoleScene::startLevel() {
	int idx = _level % kLevelCount;
	_level++;
	if (_level < kLevelCount + 1) {
		_duration = kLevels[idx].duration;
	} else {
		// Past level 18 the table repeats with halved times, rounded down
		// to 5 s (the factor never gets below 1).
		int f = MAX(1, 2 - _level / kLevelCount);
		_duration = kLevels[idx].duration * f / 2;
		_duration -= _duration % 5000;
	}
	_levelDef = &kLevels[idx];
	for (int hole = 0; hole < kHoles; hole++)
		_busy[hole] = false;
	_birdInterval = _duration / (_levelDef->birds + 1);
	_squirrelInterval = _duration / (_levelDef->squirrels + 1);
	_birds = 0;
	_squirrels = 0;
	_levelStart = g_engine->getGameMillis();
	_nextBird = _levelStart + rnd(_birdInterval);
	_nextSquirrel = _levelStart + rnd(_squirrelInterval);
	_balance = 0;
	_fed = 0;
	_appeared = 0;
	drawText("leveltext", Common::String::format("%s: %d", _vm->getString("whackamole:LEVEL").c_str(), _level));
	debug(1, "Whackamole: level %d, %d ms, %d birds, %d squirrels, wait range %d",
	      _level, _duration, _levelDef->birds, _levelDef->squirrels, _levelDef->waitRange);

	// The system cursor goes away and the larva clip (z 250) takes over.
	setCursorTable(kNoCursor);
	Anim *larva = anim("cursor");
	larva->add(Anim::kDefaultPos, Anim::kDefaultPos, kZCursor);
	larva->play();
	_cursorHidden = false;
	Common::Point mouse = _vm->getEventManager()->getMousePos();
	larva->setPos(mouse.x - kLarvaOffsetX, mouse.y - kLarvaOffsetY);
	_timeText.clear();
	drawPoints();
	drawPercent();
}

// FUN_0044eb30: the level time is up.
void WhackamoleScene::endLevel() {
	anim("cursor")->remove();
	setCursorTable(_def->cursors);
	removeAllClips();
	drawPoints();
	drawPercent();
	float p = percent();
	if (p >= (float)kPercentThreshold * 0.01f) {
		debug(1, "Whackamole: level %d cleared with %d %%", _level, (int)p);
		// The game is still running and the level time is up: onUpdate()
		// skips while the box is open (the original's box swallows the
		// frame events), else every nested frame would end the level again.
		_messageBox = true;
		_vm->messageBox("whackamole:WELLDONE", "whackamole:NEXTLEVEL", MessageBox::kButtonOk);
		_messageBox = false;
		_points += (int)p * kPointsPerPercent;
		startLevel();
		return;
	}
	_running = false;
	_vm->setGameRunning(false);
	debug(1, "Whackamole: game over at level %d with %d %%, %d points", _level, (int)p, _points);
	_vm->messageBox("interfaceh:GAMEOVER", "whackamole:GAMEOVER", MessageBox::kButtonOk);
	// TODO: high score registration FUN_0041f9a0(0, points, medals at
	// 0x49f0f8 = {2500, 5000, 10000, 20000}, 0).
	// In tournament mode (FUN_00419490) the start sign does not come back.
	addAnim("startbuttn", Anim::kDefaultPos, Anim::kDefaultPos, kZButtons);
}

// FUN_0044e830: a bird (50 % mirrored) appears in a free hole.
bool WhackamoleScene::spawnBird() {
	int hole = pickFreeHole();
	if (hole == -1)
		return false;
	startClip(rnd(1000) < 500 ? kBirdA : kBirdAR, hole);
	_waits[hole] = rnd(_levelDef->waitRange) + 1;
	_appeared++;
	return true;
}

// FUN_0044ea30
bool WhackamoleScene::spawnSquirrel() {
	int hole = pickFreeHole();
	if (hole == -1)
		return false;
	startClip(kSquirrelA, hole);
	_waits[hole] = rnd(_levelDef->waitRange) + 1;
	return true;
}

// FUN_0044ec50: the larva vanishes for a moment after a bird ate it. The
// original computes (15000 - points) * 250 / 10000 but only tests its sign;
// the timer is always 250 ms.
void WhackamoleScene::hideLarva() {
	if (_points >= kCursorHideLimit)
		return;
	_cursorHidden = true;
	setTimer(kTimerCursor, kCursorHideTime);
	anim("cursor")->setPos(kScreenWidth, kScreenHeight);
}

// FUN_0044ecd0
void WhackamoleScene::onTimer(int id, int data) {
	_cursorHidden = false;
}

// FUN_0044da80
void WhackamoleScene::onMouseDown(int hotspot, int x, int y) {
	// BUTTON module events (FUN_0044ece0)
	if (hotspot == kHotspotExit) {
		_vm->endGame();
		return;
	}
	if (hotspot == kHotspotHelp) {
		_vm->showHelp();
		return;
	}

	if (!_running) {
		if (hotspot == kHotspotStart)
			startGame();
		return;
	}
	if (hotspot < 1 || hotspot > kHoles)
		return;
	int hole = hotspot - 1;
	if (_busy[hole])
		return;

	Anim *cur = squirrelAt(hole);
	if (cur) {
		// Fed the squirrel: counts as a missed bird.
		if (cur != clip(kSquirrelC, hole)) {
			cur->remove();
			_busy[hole] = true;
			startClip(kSquirrelC, hole);
		}
		playSound("false");
		_balance--;
		_appeared++;
		drawPoints();
		drawPercent();
		debug(1, "Whackamole: squirrel fed in hole %d", hotspot);
		return;
	}

	for (int mirrored = 0; mirrored < 2; mirrored++) {
		cur = mirrored ? mirroredBirdAt(hole) : birdAt(hole);
		if (!cur)
			continue;
		Kind back = mirrored ? kBirdCR : kBirdC;
		if (cur != clip(back, hole)) {
			cur->remove();
			if (clip(kBirdSound, hole)->isAdded())
				clip(kBirdSound, hole)->remove();
			_busy[hole] = true;
			startClip(back, hole);
		}
		playSound(mirrored ? "thank you on bird language2" : "thank you on bird language1");
		_balance++;
		_fed++;
		hideLarva();
		_points += kPointsPerBird;
		drawPoints();
		drawPercent();
		debug(1, "Whackamole: bird fed in hole %d, %d points, %d/%d", hotspot, _points, _fed, _appeared);
		return;
	}
}

// FUN_0044e350
void WhackamoleScene::onAnimFinished(Anim *a) {
	a->remove();

	// A fed animal has gone back in: the hole takes clicks again.
	int hole = holeOf(a, kSquirrelC);
	if (hole == -1)
		hole = holeOf(a, kBirdCR);
	if (hole == -1)
		hole = holeOf(a, kBirdC);
	if (hole != -1) {
		_busy[hole] = false;
		return;
	}

	// Squirrel: waits, then goes back in.
	hole = MAX(holeOf(a, kSquirrelA), holeOf(a, kSquirrelB1));
	if (hole != -1) {
		if (_waits[hole] < 1) {
			startClip(kSquirrelC, hole);
		} else {
			startClip(kSquirrelB1, hole);
			_waits[hole]--;
		}
		return;
	}

	// Bird: a random waiting clip (with the chirp) per remaining wait, then
	// back in.
	bool mirrored = false;
	hole = MAX(MAX(holeOf(a, kBirdA), holeOf(a, kBirdB1)), MAX(holeOf(a, kBirdB2), holeOf(a, kBirdB3)));
	if (hole == -1) {
		mirrored = true;
		hole = MAX(MAX(holeOf(a, kBirdAR), holeOf(a, kBirdB1R)), MAX(holeOf(a, kBirdB2R), holeOf(a, kBirdB3R)));
	}
	if (hole == -1)
		return;
	if (_waits[hole] > 0) {
		static const Kind kWait[2][3] = { { kBirdB1, kBirdB2, kBirdB3 }, { kBirdB1R, kBirdB2R, kBirdB3R } };
		startClip(kWait[mirrored ? 1 : 0][rnd(3)], hole);
		clip(kBirdSound, hole)->play();
		_waits[hole]--;
	} else {
		startClip(mirrored ? kBirdCR : kBirdC, hole);
		if (clip(kBirdSound, hole)->isAdded())
			clip(kBirdSound, hole)->remove();
	}
}

// FUN_0044e740 (frame tick) plus FUN_0044ed00 (the larva follows the mouse;
// the original gets every mouse move, the framework only hotspot changes).
void WhackamoleScene::onUpdate() {
	if (!_running || _messageBox)
		return;

	Anim *larva = anim("cursor");
	if (larva->isAdded()) {
		if (_cursorHidden) {
			larva->setPos(kScreenWidth, kScreenHeight);
		} else {
			Common::Point mouse = _vm->getEventManager()->getMousePos();
			larva->setPos(mouse.x - kLarvaOffsetX, mouse.y - kLarvaOffsetY);
		}
	}

	drawTime();
	uint32 now = g_engine->getGameMillis();
	if ((int)(now - _levelStart) > _duration) {
		endLevel();
		return;
	}
	if ((int)(now - _nextBird) > 0 && _birds < _levelDef->birds) {
		if (spawnBird()) {
			_birds++;
			_nextBird = _levelStart + _birdInterval * _birds + rnd(_birdInterval);
		}
	}
	if ((int)(now - _nextSquirrel) > 0 && _squirrels < _levelDef->squirrels) {
		if (spawnSquirrel()) {
			_squirrels++;
			_nextSquirrel = _levelStart + _squirrelInterval * _squirrels + rnd(_squirrelInterval);
		}
	}
}

} // End of namespace Flaaklypa
