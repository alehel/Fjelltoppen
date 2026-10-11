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

// "Reodors Tallmaskin" - the mathlab sub game of add-on set 3. The
// original's handler is at 0x47fe50 and dispatches: 0x103 -> FUN_0047ff70
// (init), 0x104 -> FUN_00480f80 (close), 0x105 -> FUN_004814f0 (mouse
// down), 0x109 -> FUN_00432fd0 (empty), 0x10b -> FUN_004817f0 (drop of
// the dragged ball), 0x110 -> FUN_00481070 (anim finished), 0x111 ->
// FUN_00482220 (removes the timer's element; no timer is set), 0x112 ->
// FUN_00481930 (tick), 0x116 -> FUN_00481350 (buttons).

#include "common/config-manager.h"
#include "common/debug.h"
#include "common/system.h"

#include "flaaklypa/dialog.h"
#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/hiscore.h"
#include "flaaklypa/mathlab.h"
#include "flaaklypa/resources.h"

namespace Flaaklypa {

// The level table at 0x5120b0 (9 ints per level). Levels go up to index 23
// (FUN_00480530 clamps 24 to 22), one past the 23 rows: that "level" reads
// the next bytes of .data (0x5123ec), which give tanks of 0, 50, 100, 30 and
// 0, balls that are all 1, no time and a level tank of size 0.
const MathlabScene::Level MathlabScene::kLevels[kLevelRows] = {
	{ {   12,   16,  14,   11,   15 },   2,  4, 1, 80 },
	{ {   17,   15,  16,   13,   18 },   3,  5, 1, 60 },
	{ {   13,   16,  49,   21,   37 },   3,  6, 2, 60 },
	{ {   21,   39,  22,   51,   22 },   5,  6, 2, 50 },
	{ {   44,   60,  35,   79,   41 },   5,  8, 2, 50 },
	{ {   48,   82,  62,  108,   38 },  10, 10, 3, 40 },
	{ {   91,   91,  77,  109,   45 },  10, 12, 3, 40 },
	{ {   97,   89, 112,   61,  146 },  10, 14, 3, 40 },
	{ {  121,   58,  72,   99,  154 },  20, 16, 4, 30 },
	{ {  155,   99,  87,  129,   71 },  20, 18, 4, 30 },
	{ {  269,  201, 117,   87,  143 },  30, 20, 4, 30 },
	{ {  258,  193, 283,   69,  111 },  30, 20, 4, 30 },
	{ {  332,  129, 476,  259,   79 },  40, 20, 5, 20 },
	{ {  387,  156, 112,  258,  236 },  40, 20, 5, 20 },
	{ {  425,  109, 225,  128,  254 },  45, 20, 5, 20 },
	{ {  565,  517, 345,  258,  323 },  50, 20, 5, 20 },
	{ {  474,  494, 373,  392,  418 },  50, 20, 5, 10 },
	{ {  634,  667, 386,  599,  376 },  60, 20, 6, 10 },
	{ {  416,  763, 456,  632,  445 },  70, 20, 6, 10 },
	{ {  683, 1067, 456,  777,  189 },  80, 20, 6,  5 },
	{ {  776, 1229, 245, 1059,  579 },  90, 20, 6,  5 },
	{ { 1156, 1629, 676, 1201,  434 }, 100, 20, 6,  5 },
	{ { 1320, 2129, 479, 1559, 1257 }, 100, 20, 6,  5 },
	{ {    0,   50, 100,   30,    0 },   0,  0, 0,  0 }
};

// 0x513630: the tap's frame for each tank; it starts at the middle one.
const int MathlabScene::kTapFrames[kTanks] = { 20, 15, 11, 7, 0 };

// 0x4a03f8, FUN_0041f9d0 (no car parts with the medal)
const int MathlabScene::kMedals[4] = { 2500, 5000, 15000, 30000 };

// 0x4a03e8: points per tank poured in once, twice, three or more times
static const float kPourFactor[4] = { 1.0f, 1.5f, 1.25f, 1.0f };
// 0x51279c: the bonus of the full tanks by the number of full tanks
static const float kFullFactor[6] = { 1.0f, 1.0f, 1.2f, 1.5f, 2.0f, 3.0f };

// Text areas (FUN_00407930, z 15): the tanks' fill (0x4a0438) and capacity
// (0x4a0488), the container digits (x at 0x4a04e8, y 215..232) and the sum
// digits (x at 0x4a0500, y 321..338)
static const int kFillRects[5][4] = {
	{ 160, 448, 208, 466 }, { 240, 481, 289, 504 }, { 328, 491, 381, 513 }, { 460, 486, 511, 506 }, { 545, 438, 590, 458 }
};
static const int kCapRects[5][4] = {
	{ 161, 479, 208, 497 }, { 239, 517, 290, 539 }, { 328, 526, 381, 547 }, { 458, 516, 510, 537 }, { 543, 469, 588, 488 }
};
static const int kBoxDigitX[6] = { 212, 240, 338, 365, 486, 512 };
static const int kSumDigitX[4] = { 323, 349, 375, 403 };
// The tanks' meters (FUN_004089a0(x, y, 15, "fluid_<n>", 0, 0x10))
static const int kTankMeterPos[5][2] = { { 217, 448 }, { 301, 475 }, { 394, 486 }, { 440, 481 }, { 529, 436 } };
// FUN_00412b90(water5 + tank * 0xa8): the stream into each tank
static const char *const kWater[5] = { "water5", "water4", "water3", "water2", "water1" };

bool MathlabScene::_thirdDisabled = true;
bool MathlabScene::_hatchOpen = true;

MathlabScene::MathlabScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def), _buttons(this),
	_soundRandom("mathlab"), _dragging(false), _dragData(0), _clockBase(0), _state(kStateIdle), _level(0),
	_score(0), _shownScore(0), _pumped(0), _target(0), _deadline(0), _timeBase(0), _penalty(0), _pending(0),
	_generation(1), _pumping(false), _tapState(0), _tapFrame(0), _tapFrom(0), _tapStart(0) {
	for (int t = 0; t < kTanks; t++) {
		_capacity[t] = _fill[t] = _pourCount[t] = _blinkValue[t] = 0;
		_blinkStart[t] = 0;
	}
	for (int b = 0; b < kBoxes; b++)
		_box[b] = 0;
	_op[0] = _op[1] = kOpAdd;
}

MathlabScene::~MathlabScene() {
	for (int i = 0; i < kBallPool; i++)
		destroyElement(_balls[i]);
	for (int i = 0; i < kSoundClones; i++)
		destroyElement(_sounds[i]);
	destroyElement(_drag);
	delete _levelMeter.bitmap;
	for (int t = 0; t < kTanks; t++)
		delete _tankMeters[t].bitmap;
}

bool MathlabScene::load() {
	if (!Scene::load())
		return false;
	// FUN_004809d0
	_red14.load("Amerigo BT_14_red");
	_red16.load("Amerigo BT_16_red");
	_black16.load("Amerigo BT_16_black");
	_yellow14.load("Amerigo BT_14_yellow");
	_green14.load("Amerigo BT_14_green");
	return true;
}

// ---- helpers ------------------------------------------------------------------------

// FUN_0040c450, counted from the scene's start (see NOTES.md on the end of a
// round)
uint32 MathlabScene::now() const {
	return _vm->getGameMillis() - _clockBase;
}

// FUN_0040c530: rand() % n, 0 for n == 0
int MathlabScene::rnd(int n) {
	return n > 0 ? (int)_vm->getRandomNumber(n - 1) : 0;
}

void MathlabScene::initElement(Element &e, const char *name, bool smacker, bool visible, int hotspot, int x, int y) {
	Common::strlcpy(e.name, name, sizeof(e.name));
	e.def.name = e.name;
	e.def.smacker = smacker;
	e.def.visible = visible;
	e.def.transparent = 1;
	e.def.loop = 0;
	e.def.hotspot = hotspot;
	e.def.x = x;
	e.def.y = y;
	e.def.group = 0;
	e.def.zOrder = 0;
	e.def.overlay = 0;
	e.used = false;
	if (!e.anim)
		e.anim = new Anim(this, &e.def);
}

void MathlabScene::destroyElement(Element &e) {
	if (e.anim) {
		e.anim->remove();
		delete e.anim;
		e.anim = nullptr;
	}
}

// FUN_004804c0: the name goes into the sound element 0x5127b8 (" ", an
// audio only clip) and a clone of it plays (SCENE_PlayAnimClone), so the
// same sound may overlap itself.
void MathlabScene::playSound(const char *name) {
	Element *e = nullptr;
	for (int i = 0; i < kSoundClones && !e; i++)
		if (!_sounds[i].anim->isAdded())
			e = &_sounds[i];
	if (!e) {
		e = &_sounds[0];
		e->anim->remove();
	}
	Common::strlcpy(e->name, name, sizeof(e->name));
	e->anim->add(0, 0, 0);
	e->anim->setRemoveWhenDone(true);
	e->anim->play();
}

// FUN_00407930: a text area (key coloured, z 15)
void MathlabScene::createLabel(const char *name, int left, int top, int right, int bottom) {
	defineSurfaceAnim(name, right - left, bottom - top, _vm->_screen->format.RGBToColor(0, 255, 0))->add(left, top, kZText);
}

// FUN_004079c0: 0x201 right aligned, 0x101 centred, both centred vertically;
// FUN_00407a10 (flags 0) with "" clears the area
void MathlabScene::drawLabel(const Common::String &name, const BitmapFont &font, const Common::String &text, bool right) {
	Graphics::ManagedSurface *s = anim(name.c_str())->surface();
	if (!s)
		return;
	s->fillRect(Common::Rect(s->w, s->h), s->format.RGBToColor(0, 255, 0));
	if (text.empty())
		return;
	const int w = font.stringWidth(text);
	font.drawString(*s, right ? s->w - w : (s->w - w) / 2, (s->h - font.height()) / 2, text);
}

// FUN_004807d0 (FUN_004807b0 with the red font): "%d" right aligned
void MathlabScene::drawNumber(const Common::String &name, const BitmapFont &font, int value) {
	drawLabel(name, font, Common::String::format("%d", value), true);
}

void MathlabScene::clearLabel(const Common::String &name) {
	drawLabel(name, _red14, "", false);
}

// FUN_004089a0(x, y, 15, bitmap, 0, 0x10): the bitmap fills the meter from
// the bottom (the original's element is one pixel wider and one to the
// left, with the bitmap at x + 1 and its first column always clear)
void MathlabScene::createMeter(Meter &m, const char *elem, const char *bitmap, int x, int y) {
	m.elem = elem;
	m.bitmap = _vm->_resources->loadBitmap(_name, Common::String::format("bitmap/%s.bmp", bitmap), _vm->_screen->format);
	const int w = m.bitmap ? m.bitmap->w : 1, h = m.bitmap ? m.bitmap->h : 1;
	defineSurfaceAnim(elem, w, h, _vm->_screen->format.RGBToColor(0, 255, 0))->add(x, y, kZText);
	setMeter(m, 0.0f);
}

// FUN_00408b00: the bottom round(h * value) rows of the bitmap
void MathlabScene::setMeter(Meter &m, float value) {
	Graphics::ManagedSurface *s = anim(m.elem.c_str())->surface();
	if (!s)
		return;
	s->fillRect(Common::Rect(s->w, s->h), s->format.RGBToColor(0, 255, 0));
	if (!m.bitmap || !(value > 0.0f))   // also NaN (an empty tank of size 0)
		return;
	if (value > 1.0f)
		value = 1.0f;
	const float f = (float)m.bitmap->h * value;
	const int n = MIN<int>((int)(f > 0.0f ? f + 0.5f : f - 0.5f), m.bitmap->h);
	if (n < 1)
		return;
	const int h = m.bitmap->h;
	s->blitFrom(*m.bitmap, Common::Rect(0, h - n, m.bitmap->w, h), Common::Point(0, h - n));
}

// ---- screen -------------------------------------------------------------------------

// FUN_0047ff70
void MathlabScene::onInit(int arg) {
	_clockBase = _vm->getGameMillis();
	_soundRandom.setSeed(_vm->getRandomNumber(0x7fffffff));
	playMusic("subgame20");
	_state = kStateIdle;
	_score = 0;
	initScreen();
	_generation = 1;

	// TODO: tournament mode (FUN_00419490): the "player ready" box
	// (FUN_00419660) and the round starts at once at the level of the
	// player's difficulty (FUN_004194a0: startRound(difficulty)).

	// Development aid: "test_mathlab=<level>" starts a round at that level
	// at once (as tournament mode does with its difficulty: levels 0..2 get
	// 100 / 60 / 30 extra seconds, higher ones only the level's time);
	// "test_mathlab_score=<n>" then sets the score.
	if (ConfMan.hasKey("test_mathlab")) {
		removeAnim("start");
		startRound(CLIP(ConfMan.getInt("test_mathlab"), 0, (int)kMaxLevel));
		if (ConfMan.hasKey("test_mathlab_score")) {
			_score = ConfMan.getInt("test_mathlab_score");
			showScore();
		}
	}
}

// FUN_00480f80: frees the labels, fonts and meters (the engine does)
void MathlabScene::onClose() {
	_dragging = false;
}

// FUN_004809d0
void MathlabScene::initScreen() {
	// FUN_00480d70: the ball structures, copies of the one at 0x512ef8
	// (ball%02d.smk at (166, 55)) with the hotspots 200..229
	for (int i = 0; i < kBallPool; i++) {
		destroyElement(_balls[i]);
		initElement(_balls[i], "ball01", true, true, kHotspotBall0 + i, 166, 55);
	}
	for (int i = 0; i < kSoundClones; i++)
		initElement(_sounds[i], " ", true, false, 0, 0, 0);
	initElement(_drag, "ball_0", false, true, 0, 0, 0);
	// FUN_00480d50
	for (int i = 0; i < kSlots; i++)
		_slots[i] = Slot();

	createMeter(_levelMeter, "levelmeter", "tank_fluidindicator", 0x2a1, 0x6c);
	for (int t = 0; t < kTanks; t++)
		createMeter(_tankMeters[t], Common::String::format("tankmeter%d", t).c_str(),
		            Common::String::format("fluid_%d", t).c_str(), kTankMeterPos[t][0], kTankMeterPos[t][1]);

	_buttons.setFont(&_red14);
	_buttons.add(kHotspotHelp, 0, 0, "help_0", "help_1", nullptr, nullptr, "", true);
	_buttons.add(kHotspotExit, 0x2d8, 0, "exit_0", "exit_1", nullptr, nullptr, "", true);
	// Not in tournament mode
	addAnim("start", Anim::kDefaultPos, Anim::kDefaultPos, kZStatic);
	addAnim("labels", Anim::kDefaultPos, Anim::kDefaultPos, kZStatic);
	addAnim("ballopener", Anim::kDefaultPos, Anim::kDefaultPos, kZStatic);
	addAnim("tap", Anim::kDefaultPos, Anim::kDefaultPos, kZStatic);
	resetTap();
	addAnim("fan", Anim::kDefaultPos, Anim::kDefaultPos, kZStatic);

	// The texts (z 15): score 0x4a0408, a second area at 0x4a0418 that is
	// only ever cleared (not created here), level 0x4a0428, clock 0x4a04d8
	createLabel("score", 698, 212, 765, 229);
	createLabel("level", 723, 189, 746, 204);
	createLabel("clock", 318, 33, 407, 65);
	for (int t = 0; t < kTanks; t++) {
		createLabel(Common::String::format("cap%d", t).c_str(), kCapRects[t][0], kCapRects[t][1], kCapRects[t][2], kCapRects[t][3]);
		createLabel(Common::String::format("fill%d", t).c_str(), kFillRects[t][0], kFillRects[t][1], kFillRects[t][2], kFillRects[t][3]);
	}
	for (int b = 0; b < kBoxes; b++) {
		for (int i = 0; i < 2; i++) {
			const int x = kBoxDigitX[b * 2 + i];
			createLabel(Common::String::format("box%d_%d", b, i).c_str(), x, 0xd7, x + 14, 0xe8);
		}
		initCounter(_boxCounters[b], Common::String::format("box%d_", b).c_str(), 2);
	}
	for (int i = 0; i < 4; i++)
		createLabel(Common::String::format("sum%d", i).c_str(), kSumDigitX[i], 0x141, kSumDigitX[i] + 14, 0x152);
	initCounter(_sumCounter, "sum", 4);

	clearTankLabels();
	resetBoxes();
}

// FUN_00480ee0: the tap turns to the middle tank, nothing waits
void MathlabScene::resetTap() {
	_tapFrame = kTapFrames[2];
	anim("tap")->showFrame(_tapFrame);
	_pours.clear();
	_tapState = 0;
}

// FUN_00480f20
void MathlabScene::clearTankLabels() {
	for (int t = 0; t < kTanks; t++) {
		clearLabel(Common::String::format("cap%d", t));
		clearLabel(Common::String::format("fill%d", t));
	}
}

// FUN_00406ac0(group, 0) + FUN_00406720(id, 1); nothing for tabs that do
// not exist (yet)
void MathlabScene::selectTab(int group, int id) {
	const int first = group == 1 ? kButtonPlus1 : kButtonPlus2;
	for (int i = first; i < first + 3; i++)
		_buttons.setState(i, SceneButtons::kStateNormal);
	_buttons.setState(id, SceneButtons::kStatePressed);
}

// ---- counters -----------------------------------------------------------------------

// FUN_00480dc0
void MathlabScene::initCounter(Counter &c, const char *label, int digits) {
	c.label = label;
	c.digits = digits;
	c.value = 0;
	setCounter(c, 0);
	updateCounter(c);
}

// FUN_00480500
void MathlabScene::setCounter(Counter &c, int value) {
	c.to = value;
	c.from = c.value;
	c.running = true;
	c.start = now();
}

// FUN_00480df0: 250 steps per second towards the value; every digit is
// drawn (leading zeros too), black, or red with a "-" over the first one
// for negative values
void MathlabScene::updateCounter(Counter &c) {
	if (!c.running)
		return;
	const int d = (int)(now() - c.start) / 4;
	if (c.from < c.to) {
		c.value = c.from + d;
		if (c.value > c.to) {
			c.value = c.to;
			c.running = false;
		}
	} else {
		c.value = c.from - d;
		if (c.value < c.to) {
			c.value = c.to;
			c.running = false;
		}
	}
	int n = ABS(c.value);
	for (int i = c.digits - 1; i >= 0; i--) {
		drawNumber(Common::String::format("%s%d", c.label.c_str(), i), c.value < 0 ? _red16 : _black16, n % 10);
		n /= 10;
	}
	if (c.value < 0)
		drawLabel(c.label + "0", _red16, "-", true);
}

// FUN_00480940: empty containers, + selected, the sum follows
void MathlabScene::resetBoxes() {
	_op[0] = _op[1] = kOpAdd;
	for (int b = 0; b < kBoxes; b++) {
		_box[b] = 0;
		setCounter(_boxCounters[b], 0);
	}
	selectTab(1, kButtonPlus1);
	selectTab(2, kButtonPlus2);
	showResult();
	if (_state == kStatePlaying)
		playSound("click2");
}

// FUN_00481790: left to right, no precedence
int MathlabScene::result() const {
	int r = _box[0];
	for (int i = 0; i < 2; i++) {
		switch (_op[i]) {
		case kOpAdd:
			r += _box[i + 1];
			break;
		case kOpSub:
			r -= _box[i + 1];
			break;
		case kOpMul:
			r *= _box[i + 1];
			break;
		default:
			break;
		}
	}
	return r;
}

// FUN_004817d0
void MathlabScene::showResult() {
	setCounter(_sumCounter, result());
}

// ---- tanks --------------------------------------------------------------------------

// FUN_00480810
void MathlabScene::resetTank(int tank) {
	setMeter(_tankMeters[tank], 0.0f);
	_fill[tank] = 0;
	_pourCount[tank] = 0;
	drawTank(tank);
}

// FUN_00480850: the meter, the capacity (red) and the contents (yellow,
// green when exactly full, red above)
void MathlabScene::drawTank(int tank) {
	setMeter(_tankMeters[tank], (float)_fill[tank] / (float)_capacity[tank]);
	drawNumber(Common::String::format("cap%d", tank), _red14, _capacity[tank]);
	const BitmapFont &font = _fill[tank] < _capacity[tank] ? _yellow14 : (_fill[tank] == _capacity[tank] ? _green14 : _red16);
	drawNumber(Common::String::format("fill%d", tank), font, _fill[tank]);
}

// FUN_00481ed0: an overfilled tank shows its contents blinking for 3 s
void MathlabScene::updateBlink() {
	for (int t = 0; t < kTanks; t++) {
		if (!_blinkValue[t])
			continue;
		const int phase = (int)(now() - _blinkStart[t]) / 300;
		if (phase < 10) {
			if (phase & 1)
				drawNumber(Common::String::format("fill%d", t), _red14, _blinkValue[t]);
			else
				clearLabel(Common::String::format("fill%d", t));
		} else {
			_blinkValue[t] = 0;
			drawTank(t);
		}
	}
}

// FUN_00481d10: the results queue for the tap. It turns to the tank (one
// frame per 50 ms, "pipe squeek1"), pours (the water clip, "ball in fluid")
// and takes the next one when the water clip has ended. A tank over its
// capacity flushes all five and costs 10 seconds.
void MathlabScene::updateTap() {
	if (_pours.empty() || _tapState == 2)
		return;
	if (_tapState == 0) {
		playSound("pipe squeek1");
		_tapFrom = _tapFrame;
		_tapStart = now();
		_tapState = 1;
	}
	const Pour p = _pours[0];
	const int target = kTapFrames[p.tank];
	const int steps = (int)(now() - _tapStart) / kTapFrameMs;
	int frame;
	bool reached;
	if (target < _tapFrom) {
		frame = _tapFrom - steps;
		reached = frame <= target;
	} else {
		frame = _tapFrom + steps;
		reached = target <= frame;
	}
	if (reached) {
		const int t = p.tank;
		_pourCount[t]++;
		_fill[t] += p.amount;
		if (_fill[t] > _capacity[t]) {
			debug(1, "Mathlab: tank %d overflows with %d of %d", t, _fill[t], _capacity[t]);
			_blinkValue[t] = _fill[t];
			_blinkStart[t] = now();
			loseTime(10);
			for (int i = 0; i < kTanks; i++)
				resetTank(i);
			playSound("Flush out1P");
			if (!isAnimAdded("overflow"))
				playAnim("overflow");
		} else {
			debug(1, "Mathlab: tank %d gets %d: %d of %d", t, p.amount, _fill[t], _capacity[t]);
			drawTank(t);
		}
		playAnim(kWater[t]);
		playSound("ball in fluid");
		_pours.remove_at(0);
		_tapState = 2;
		frame = target;
	}
	_tapFrame = frame;
	anim("tap")->showFrame(frame);
}

// FUN_00481470: the red button
void MathlabScene::pumpPressed() {
	if (_pumping)
		return;
	bool any = false;
	for (int t = 0; t < kTanks; t++)
		if (_fill[t] != 0)
			any = true;
	if (!any)
		return;
	_pumping = true;
	playSound("Flush in2P");
	if (!isAnimPlaying("new_pump"))
		playAnim("new_pump");
	if (!isAnimAdded("flush"))
		playAnim("flush");
}

// FUN_00481f70: the tanks go into the level tank once the tap is idle
void MathlabScene::pump() {
	int full = 0;
	for (int t = 0; t < kTanks; t++)
		if (_fill[t] == _capacity[t])
			full++;
	int bonus = 0;
	const int before = _score;
	for (int t = 0; t < kTanks; t++)
		scoreTank(t, bonus);
	if (bonus > 0) {
		bonus = (int)((double)bonus * (double)kFullFactor[full]);
		_score += bonus;
	}
	debug(1, "Mathlab: pumped, %d full, bonus %d, %d points (score %d), level tank %d of %d",
	      full, bonus, _score - before, _score, _pumped, _target);
	showScore();
}

// FUN_00481ff0(tank, 0): a tank within its capacity scores its contents
// plus contents * 1.5 / 1.25 / 1 for one / two / more pours; an exactly
// full one also gives 10 seconds and its size to the bonus
void MathlabScene::scoreTank(int tank, int &bonus) {
	const int f = _fill[tank];
	if (f > 0 && f <= _capacity[tank]) {
		if (f == _capacity[tank]) {
			loseTime(-10);
			bonus += _capacity[tank];
		}
		loseTime(0);
		_score += f;
		_pumped += f;
		const int k = MIN(_pourCount[tank], 3);
		_score += (int)((double)f * (double)kPourFactor[k]);
	}
	resetTank(tank);
}

// FUN_00480900: the score shown (and registered at the end), the level tank
void MathlabScene::showScore() {
	_shownScore = _score;
	drawNumber("score", _red14, _score);
	setMeter(_levelMeter, (float)_pumped / (float)_target);
}

// ---- balls --------------------------------------------------------------------------

// FUN_00480390
int MathlabScene::allocBall() {
	for (int i = 0; i < kBallPool; i++) {
		if (!_balls[i].used) {
			_balls[i].used = true;
			return i;
		}
	}
	return -1;
}

// FUN_00481700
void MathlabScene::freeBall(int index) {
	if (index >= 0 && index < kBallPool)
		_balls[index].used = false;
}

// FUN_00480440: qsort with LAB_00480460, the balls on the rail first, the
// one furthest along first (here stable)
void MathlabScene::sortSlots() {
	for (int i = 1; i < kSlots; i++) {
		Slot s = _slots[i];
		int j = i - 1;
		while (j >= 0) {
			const Slot &o = _slots[j];
			const bool after = (!o.active && s.active) || (o.active && s.active && o.frame < s.frame);
			if (!after)
				break;
			_slots[j + 1] = _slots[j];
			j--;
		}
		_slots[j + 1] = s;
	}
}

// FUN_00481660
MathlabScene::Slot *MathlabScene::findSlot(int hotspot) {
	for (int i = 0; i < kSlots; i++)
		if (_slots[i].active && _slots[i].ball >= 0 && _balls[_slots[i].ball].def.hotspot == hotspot)
			return &_slots[i];
	return nullptr;
}

// FUN_004803d0: at most three balls of a number, and with balls above 10
// at most four of 10 and up before another one above 10
bool MathlabScene::rejectNumber(int n) const {
	if (kLevels[_level].ballRange > 10) {
		int big = 0;
		for (int i = 0; i < kSlots; i++)
			if (_slots[i].active && _slots[i].number > 9)
				big++;
		if (big > 3 && n > 10)
			return true;
	}
	int same = 0;
	for (int i = 0; i < kSlots; i++)
		if (_slots[i].active && _slots[i].number == n)
			same++;
	return same > 2;
}

// The drum starts: its start clip, the fan with its sound (at 0.4 of the
// effects volume) and the rattle loop
void MathlabScene::startDrum() {
	playAnim("startballs");
	anim("fan")->play();
	playAnim("fan fade LONG");
	anim("fan fade LONG")->setVolume((int)(0.4f * 255));
	playAnim("balls1");
}

// FUN_00480180: a new ball for a free place on the rail, behind the last
// one; the drum shows a ball coming out ("newball", or once the drum turns)
bool MathlabScene::addBall() {
	int active = 0;
	while (active < kSlots && _slots[active].active)
		active++;
	if (active == kSlots)
		return false;

	const bool ending = isAnimAdded("endballs");
	const bool starting = isAnimPlaying("startballs");
	const bool turning = isAnimAdded("balls");
	bool emit = false;
	if (!starting && !ending) {
		if (!turning) {
			startDrum();
			_pending++;
		} else {
			emit = true;
		}
	} else if (turning) {
		emit = true;
	} else {
		_pending++;
	}
	if (emit) {
		if (!isAnimAdded("newball")) {
			playAnim("newball");
			playSound("newball1");
		} else {
			_pending++;
		}
	}

	// The new ball starts in the pipe (negative frames show the empty first
	// frame) at least 5 frames behind every other ball
	int start = ending ? -42 : (starting ? -36 : -30);
	int limit = kFrameRest;
	for (int i = 0; i < kSlots; i++) {
		if (!_slots[i].active)
			continue;
		const int v = _slots[i].frame - 5;
		limit = MIN(limit, v);
		start = MIN(start, v);
	}

	for (int i = 0; i < kSlots; i++) {
		if (_slots[i].active)
			continue;
		// Deviation: level 23 only has balls of 1, the original loops forever
		// once three of them are on the rail; give up after 1000 draws.
		int n, tries = 0;
		do {
			n = rnd(kLevels[_level].ballRange) + 1;
		} while (rejectNumber(n) && ++tries < 1000);
		const int ball = allocBall();
		if (ball < 0)
			return false;   // the original would crash (all 30 structures in use)
		Slot &s = _slots[i];
		s.number = n;
		s.stepTime = now();
		s.active = true;
		s.frame = start;
		s.limit = limit;
		s.ball = ball;
		Element &e = _balls[ball];
		Common::sprintf_s(e.name, "ball%02d", n);
		e.anim->add(Anim::kDefaultPos, Anim::kDefaultPos, kZBall);
		if (limit >= 0)
			e.anim->showFrame(0);
		s.out = 0;
		s.sound = 0;
		debug(2, "Mathlab: new ball %d (hotspot %d) at frame %d, limit %d", n, e.def.hotspot, start, limit);
		sortSlots();
		return true;
	}
	sortSlots();
	return true;
}

// FUN_004816a0
void MathlabScene::removeBall(int hotspot) {
	for (int i = 0; i < kSlots; i++) {
		Slot &s = _slots[i];
		if (!s.active || s.ball < 0 || _balls[s.ball].def.hotspot != hotspot)
			continue;
		_balls[s.ball].anim->remove();
		freeBall(s.ball);
		s.active = false;
		sortSlots();
		return;
	}
}

// FUN_00480730: the hatch at the end of the rail opens (a new level, the
// end of the round); every ball on the rail falls out
void MathlabScene::openHatch() {
	if (_hatchOpen)
		return;
	_hatchOpen = true;
	if (!isAnimAdded("ballopener"))
		addAnim("ballopener", Anim::kDefaultPos, Anim::kDefaultPos, kZStatic);
	anim("ballopener")->play();
	playSound("openerP");
	for (int i = 0; i < kSlots; i++)
		if (_slots[i].active && _slots[i].frame >= 0)
			_slots[i].out = _generation;
	_generation++;
}

// FUN_00481aa0: the balls roll one frame per 66 ms up to their limit:
// the first to frame 36 (50 while the hatch is open), each one 5 frames
// behind the one before. The balls left in the pipe wait while the hatch is
// open; it closes when the last falling ball is gone.
void MathlabScene::moveBalls() {
	if (!_slots[0].active)
		return;
	if (_slots[0].limit < kFrameRest)
		_slots[0].limit = kFrameRest;
	if (_hatchOpen)
		_slots[0].limit = kFrameGone;
	for (int i = 1; i < kSlots; i++) {
		Slot &s = _slots[i];
		if (!s.active)
			continue;
		if (s.limit < _slots[i - 1].limit - 5)
			s.limit = _slots[i - 1].limit - 5;
		if (s.out == 0 && s.limit > kFrameRest)
			s.limit = kFrameRest;
	}

	const uint32 t = now();
	for (int i = 0; i < kSlots; i++) {
		// Indexed like the original: a ball removed at frame 50 re-sorts the
		// slots under the loop, and the one moving into its place waits
		Slot &s = _slots[i];
		if (s.ball >= 0 && _balls[s.ball].anim->isAdded()) {
			if (s.frame >= 6 && s.sound == 0) {
				// FUN_00481ca0
				const int r = _soundRandom.getRandomNumber(29);
				playSound(r < 10 ? "ball1" : (r < 20 ? "ball2" : "ball3"));
				s.sound++;
			} else if (s.frame >= 37 && s.sound == 1) {
				// FUN_00481ce0
				playSound(_soundRandom.getRandomNumber(19) < 10 ? "balls out new1" : "balls out new2");
				s.sound++;
			}
		}
		if (!s.active || (_hatchOpen && s.out == 0))
			continue;
		const int frame = s.frame;
		int dt = (int)(t - s.stepTime);
		int next = frame;
		if (dt > kFrameMs) {
			const int steps = (dt - 1) / kFrameMs;
			next = frame + steps;
			dt -= steps * kFrameMs;
		}
		if (frame == next)
			continue;
		if (next > s.limit)
			next = s.limit;
		s.frame = next;
		s.stepTime = t - dt;
		if (next < 0)
			continue;
		Anim *a = _balls[s.ball].anim;
		a->showFrame(next);
		if (next == s.limit && frame != next)
			logBall(s);
		if (next == kFrameGone)
			removeBall(_balls[s.ball].def.hotspot);
	}

	if (!_hatchOpen)
		return;
	for (int i = 0; i < kSlots; i++)
		if (_slots[i].active && _slots[i].out != 0)
			return;
	for (int i = 0; i < kSlots; i++)
		if (_slots[i].active && _slots[i].out == 0)
			_slots[i].stepTime = t;
	removeAnim("ballopener");
	playAnim("ballcloser");
	if (_state != kStateOver)
		playSound("closerP");
	_hatchOpen = false;
	if (_state != kStateOver)
		for (int i = 0; i < kSlots; i++)
			addBall();
}

// Development aid (debug level 2): where a ball stopped, for scripted drags
void MathlabScene::logBall(const Slot &s) const {
	if (gDebugLevel < 2)
		return;
	const Anim *a = _balls[s.ball].anim;
	const Graphics::ManagedSurface *f = const_cast<Anim *>(a)->surface();
	int x0 = 9999, y0 = 9999, x1 = -1, y1 = -1;
	if (f) {
		const uint32 key = f->getPixel(0, 0);
		for (int y = 0; y < f->h; y++)
			for (int x = 0; x < f->w; x++)
				if (f->getPixel(x, y) != key) {
					x0 = MIN(x0, x);
					y0 = MIN(y0, y);
					x1 = MAX(x1, x);
					y1 = MAX(y1, y);
				}
	}
	debug(2, "Mathlab: ball %d (hotspot %d) rests at frame %d, centre %d,%d", s.number, a->hotspot(),
	      s.frame, a->x() + (x0 + x1) / 2, a->y() + (y0 + y1) / 2);
}

// ---- game ---------------------------------------------------------------------------

// FUN_0047ffd0: a round starts at a level (0 from the start ball, the
// difficulty in tournament mode); levels 0, 1 and 2 get 100, 60 and 30
// seconds on top of the level's time
void MathlabScene::startRound(int level) {
	_penalty = 0;
	_score = 0;
	_shownScore = 0;
	_level = level;
	_timeBase = (int32)now();
	_thirdDisabled = true;
	if (level == 0)
		_timeBase += 100000;
	else if (level == 1)
		_timeBase += 60000;
	else if (level == 2)
		_timeBase += 30000;
	_deadline = _timeBase;
	setupLevel(true);
	if (_thirdDisabled && !isAnimAdded("disable2"))
		addAnim("disable2", Anim::kDefaultPos, Anim::kDefaultPos, kZStatic);
	showScore();
	setCounter(_sumCounter, 0);
	resetBoxes();
	_vm->setGameRunning(true);   // FUN_0040d8a0(1)
	resetBoxes();
	_state = kStatePlaying;
	_buttons.add(kButtonPump, 0x27f, 0x1fb, nullptr, "ok_button", nullptr, nullptr, "", true);
	_buttons.addTab(1, kButtonPlus1, 0x124, 0xbf, nullptr, "plus_1", "", true);
	_buttons.addTab(1, kButtonPlus1 + 1, 0x124, 0xd7, nullptr, "minus_1", "", true);
	_buttons.addTab(1, kButtonPlus1 + 2, 0x124, 0xee, nullptr, "multiply_1", "", true);
	selectTab(1, kButtonPlus1);
	// Deviation: the original only clears the slots, so the balls the last
	// round left in the pipe (invisible, first frame) keep their elements
	// and structures until the 30 run out; here they are freed
	for (int i = 0; i < kBallPool; i++) {
		_balls[i].anim->remove();
		_balls[i].used = false;
	}
	for (int i = 0; i < kSlots; i++)
		_slots[i] = Slot();
	for (int i = 0; i < kSlots; i++)
		addBall();
	debug(1, "Mathlab: round starts at level %d, %d s", _level + 1, (int)(_deadline - (int32)now()) / 1000);
}

// FUN_00480530: first = the round's first level (FUN_00480530(1)), else the
// next one. From level 3 on (index 2) the third container and its
// operators take part.
void MathlabScene::setupLevel(bool first) {
	if (!first) {
		_level++;
		if (_level > kMaxLevel)
			_level = kMaxLevel - 1;
		openHatch();
		if (!isAnimAdded("lightbulb")) {
			playAnim("lightbulb");
			playSound("light_bulb");
		}
	} else {
		_hatchOpen = false;
	}
	if (_level > 1 && _thirdDisabled) {
		_buttons.addTab(2, kButtonPlus2, 0x1a1, 0xc0, nullptr, "plus_2", "", true);
		_buttons.addTab(2, kButtonPlus2 + 1, 0x1a0, 0xd7, nullptr, "minus_2", "", true);
		_buttons.addTab(2, kButtonPlus2 + 2, 0x19f, 0xee, nullptr, "multiply_2", "", true);
		selectTab(2, kButtonPlus2);
		playSound("3 shows up");
		if (isAnimAdded("disable2"))
			removeAnim("disable2");
		_thirdDisabled = false;
	}
	_pending = 0;
	_pumping = false;
	const Level &l = kLevels[_level];
	_deadline += l.seconds * 1000;
	setMeter(_levelMeter, 0.0f);
	drawNumber("level", _red14, _level + 1);
	for (int t = 0; t < kTanks; t++) {
		_capacity[t] = rnd(l.capacityRange) + l.capacity[t];
		resetTank(t);
	}
	_target = 0;
	for (int t = 0; t < kTanks; t++)
		_target += l.capacity[t] * l.targetFactor;
	_pumped = 0;
	debug(1, "Mathlab: level %d, tanks %d %d %d %d %d, level tank %d, balls 1..%d, +%d s",
	      _level + 1, _capacity[0], _capacity[1], _capacity[2], _capacity[3], _capacity[4], _target, l.ballRange, l.seconds);
}

// FUN_00481450
void MathlabScene::checkLevel() {
	if (_pumped >= _target)
		setupLevel(false);
}

// FUN_004818c0: seconds off the clock (negative: added). The round is over
// once the deadline is below the time since the round's start minus its
// extra time; the hatch opens and the balls fall out.
void MathlabScene::loseTime(int seconds) {
	const int32 t = (int32)now();
	if (_state == kStateOver)
		return;
	_penalty += seconds;
	if (_penalty < 0)
		_penalty = 0;
	_deadline -= seconds * 1000;
	if (_deadline < t - _timeBase) {
		debug(1, "Mathlab: time is up, score %d", _score);
		_state = kStateOver;
		openHatch();
	}
}

// FUN_004820d0: the balls are out, the tap idle: game over
void MathlabScene::gameOver() {
	for (int id = kButtonPlus1; id < kButtonPlus1 + 3; id++)
		_buttons.remove(id);
	if (!_thirdDisabled)
		for (int id = kButtonPlus2; id < kButtonPlus2 + 3; id++)
			_buttons.remove(id);
	_vm->setGameRunning(false);   // FUN_0040d8a0(0)
	debug(1, "Mathlab: game over at level %d, %d points", _level + 1, _shownScore);
	// Not the scene's own mathlab:GAMEOVER
	_vm->messageBox("interfaceh:GAMEOVER", "whackamole:GAMEOVER", MessageBox::kButtonOk);
	// FUN_0041f9d0(0, score, 0x4a03f8): medals without car parts
	registerScoreNoCarParts(_vm, "", _shownScore, kMedals);
	// TODO: tournament mode (FUN_00419490) returns here (the score went to
	// the tournament and its next game starts).
	clearTankLabels();
	clearLabel("score");
	clearLabel("level");
	clearLabel("clock");
	resetBoxes();
	for (int t = 0; t < kTanks; t++)
		setMeter(_tankMeters[t], 0.0f);
	setMeter(_levelMeter, 0.0f);
	_buttons.remove(kButtonPump);
	addAnim("start", Anim::kDefaultPos, Anim::kDefaultPos, kZStatic);
	_state = kStateIdle;
	resetTap();
}

// FUN_00481350 (event 0x116)
void MathlabScene::onButton(int id) {
	if (id == kHotspotHelp)
		_vm->showHelp();
	else if (id == kHotspotExit)
		_vm->endGame();
	if (_state != kStatePlaying)
		return;
	switch (id) {
	case kButtonPlus1:
	case kButtonPlus1 + 1:
	case kButtonPlus1 + 2:
		_op[0] = id - kButtonPlus1;
		break;
	case kButtonPlus2:
	case kButtonPlus2 + 1:
	case kButtonPlus2 + 2:
		_op[1] = id - kButtonPlus2;
		break;
	case kButtonPump:
		pumpPressed();
		break;
	default:
		break;
	}
	checkLevel();
	if (id >= kButtonPlus1 && id <= kButtonPlus2 + 2) {
		showResult();
		playSound("operator_click");
	}
}

// FUN_004817f0 (event 0x10b of "ball_0"): a ball into a container (not the
// covered third one: the ball is simply gone), anywhere else costs 2 s
void MathlabScene::dropBall(int hotspot, int number) {
	_drag.anim->remove();
	if (_state != kStatePlaying)
		return;
	if (hotspot >= kHotspotBox0 && hotspot < kHotspotBox0 + kBoxes) {
		const int b = hotspot - kHotspotBox0;
		if (!_thirdDisabled || b < 2) {
			playSound("click2");
			_box[b] = number;
			setCounter(_boxCounters[b], number);
			showResult();
			debug(1, "Mathlab: %d into container %d, result %d", number, b, result());
		}
		return;
	}
	loseTime(2);
	if (!isAnimAdded("dumpball"))
		playAnim("dumpball");
	debug(1, "Mathlab: ball %d dropped on hotspot %d", number, hotspot);
}

// ---- input --------------------------------------------------------------------------

void MathlabScene::onMouseMove(int hotspot, int x, int y) {
	_buttons.setHover(hotspot);
}

// FUN_004814f0, then the BUTTON module (FUN_00406b50)
void MathlabScene::onMouseDown(int hotspot, int x, int y) {
	_buttons.setHover(hotspot);
	if (_state == kStateIdle) {
		if (hotspot == kHotspotStart) {
			removeAnim("start");
			startRound(0);
		}
	} else if (_state != kStateOver) {
		// (hotspots 50..54 would drag elements 0x512404 + n * 0xbc: dead code)
		if (hotspot >= kHotspotBall0 && hotspot < kHotspotBall0 + kBallPool) {
			Slot *s = findSlot(hotspot);
			if (s && s->frame < 37) {
				// FUN_0040c1f0("ball_0", number): the ball's bitmap follows the
				// mouse 20 pixels up and left of it
				const int number = s->number;
				_dragging = true;
				_dragData = number;
				_dragMouse = Common::Point(x, y);
				if (!_drag.anim->isAdded()) {
					Common::sprintf_s(_drag.name, "ball_%02d", number);
					_drag.anim->add(x - 20, y - 20, kZDrag);
				}
				debug(1, "Mathlab: ball %d picked at frame %d", number, s->frame);
				removeBall(hotspot);
				addBall();
			}
		} else if (hotspot >= kHotspotTank0 && hotspot < kHotspotTank0 + kTanks) {
			int filled = 0;
			for (int b = 0; b < kBoxes; b++)
				if (_box[b] != 0)
					filled++;
			const int r = result();
			if (r > 0 && filled > 1) {
				Pour p;
				p.tank = hotspot - kHotspotTank0;
				p.amount = r;
				_pours.push_back(p);
				debug(1, "Mathlab: %d for tank %d", r, p.tank);
				resetBoxes();
			} else {
				playSound("negative");
			}
		}
	}
	const int id = _buttons.mouseDown(hotspot, x, y);
	if (id)
		onButton(id);
}

// The BUTTON module's release, then the EVENT module's drop (FUN_0040c260:
// the hotspot under the dragged ball, which has none of its own)
void MathlabScene::onMouseUp(int hotspot, int x, int y) {
	_buttons.setHover(hotspot);
	const int id = _buttons.mouseUp();
	if (id)
		onButton(id);
	if (_dragging) {
		_dragging = false;
		dropBall(hotspot, _dragData);
	}
}

// FUN_00481070
void MathlabScene::onAnimFinished(Anim *a) {
	if (a == anim("ballcloser")) {
		removeAnim("ballcloser");
		if (isAnimAdded("ballopener"))
			removeAnim("ballopener");
		addAnim("ballopener", Anim::kDefaultPos, Anim::kDefaultPos, kZStatic);
		return;
	}
	static const char *const kRemove[] = { "overflow", "flush", "new_pump", "dumpball", "lightbulb", "endball1" };
	for (int i = 0; i < ARRAYSIZE(kRemove); i++) {
		if (a == anim(kRemove[i])) {
			a->remove();
			return;
		}
	}
	for (int t = 0; t < kTanks; t++) {
		if (a == anim(kWater[t])) {
			a->remove();
			_tapState = 0;
			return;
		}
	}
	if (a == anim("startballs")) {
		a->remove();
		playAnim("balls");
		if (_pending && !isAnimAdded("newball")) {
			playAnim("newball");
			playSound("newball1");
			_pending--;
		}
	} else if (a == anim("balls")) {
		a->remove();
		if (isAnimAdded("balls1"))
			removeAnim("balls1");
		if (_pending == 0) {
			playAnim("endballs");
			playAnim("endball1");
			anim("endball1")->setVolume((int)(0.65f * 255));
		} else {
			playAnim("balls");
			playAnim("balls1");
		}
	} else if (a == anim("endballs")) {
		a->remove();
		if (_pending) {
			playAnim("startballs");
		} else if (isAnimAdded("fan")) {
			anim("fan")->stop();   // FUN_00412a60
			if (isAnimAdded("new fan long1"))
				removeAnim("new fan long1");
			if (isAnimAdded("fan fade LONG"))
				removeAnim("fan fade LONG");
		}
	} else if (a == anim("newball")) {
		a->remove();
		if (_pending) {
			playAnim("newball");
			playSound("newball1");
			_pending--;
		}
	} else if (a == anim("fan fade LONG")) {
		a->remove();
		playAnim("new fan long1");
		anim("new fan long1")->setVolume((int)(0.4f * 255));
	}
}

// FUN_00481930 (event 0x112), plus the EVENT module's drag (FUN_0040c300:
// the dragged element follows the mouse moves)
void MathlabScene::onUpdate() {
	if (_dragging) {
		const Common::Point m = _vm->mousePos();
		if (m != _dragMouse) {
			Anim *a = _drag.anim;
			a->setPos(a->x() + m.x - _dragMouse.x, a->y() + m.y - _dragMouse.y);
			_dragMouse = m;
		}
	}

	for (int b = 0; b < kBoxes; b++) {
		if (b == 2 && _thirdDisabled) {
			// FUN_00407880: the covered container shows no digits
			clearLabel("box2_0");
			clearLabel("box2_1");
			break;
		}
		updateCounter(_boxCounters[b]);
	}
	updateCounter(_sumCounter);
	if (_state == kStateIdle)
		return;
	if (_state != kStateOver)
		addBall();
	updateTap();
	moveBalls();
	updateBlink();

	// The clock: mm:ss:hundredths (FUN_0041ecb0), not below zero
	const int32 left = _deadline - (int32)now();
	const int ms = left < 1 ? 0 : left;
	drawLabel("clock", _red14, Common::String::format("%02d:%02d:%02d", ms / 1000 / 60, ms / 1000 % 60, ms % 1000 / 10), false);
	if (left < 0)
		loseTime(20);

	if (_tapState == 0 && _pumping && _pours.empty()) {
		pump();
		checkLevel();
		_pumping = false;
	}
	if (_state == kStateOver && _tapState == 0 && _pours.empty() && !_hatchOpen)
		gameOver();
}

} // End of namespace Flaaklypa
