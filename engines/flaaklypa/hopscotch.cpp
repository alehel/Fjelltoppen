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
#include "flaaklypa/hopscotch.h"
#include "flaaklypa/music.h"
#include "flaaklypa/resources.h"

namespace Flaaklypa {

// The level table at 0x49e788: sequence length of the first round, rounds,
// grid, squares in use, time per turn, numbers (0) or symbols (1).
const HopscotchScene::Level HopscotchScene::kLevels[kLevelCount] = {
	{ 3, 3, 0, 4, 25000, 0 },
	{ 3, 4, 0, 5, 24000, 1 },
	{ 3, 5, 0, 6, 24000, 0 },
	{ 4, 4, 1, 7, 23000, 0 },
	{ 4, 5, 1, 8, 23000, 1 },
	{ 4, 6, 1, 9, 22000, 0 },
	{ 5, 5, 2, 7, 23000, 0 },
	{ 5, 6, 2, 8, 24000, 1 },
	{ 5, 7, 2, 9, 25000, 0 },
	{ 6, 8, 3, 10, 26000, 0 },
	{ 6, 10, 3, 11, 27000, 1 },
	{ 6, 12, 3, 12, 28000, 0 },
	{ 8, 3, 0, 12, 28000, 0 },
	{ 10, 3, 1, 12, 27000, 1 },
	{ 12, 3, 2, 12, 26000, 0 },
	{ 14, 3, 3, 12, 25000, 1 }
};

// Square positions relative to the grid origin (0x49e648 .. 0x49e708).
static const int kGrid1Cells[] = { 14, 102, 97, 54, 96, 133, 180, 10, 179, 88, 177, 169 };
static const int kGrid2Cells[] = { 11, 14, 94, 12, 175, 10, 11, 93, 95, 89, 176, 89, 12, 171, 95, 170, 177, 169 };
static const int kGrid3Cells[] = { 11, 78, 82, 77, 154, 78, 228, 24, 230, 126, 306, 77, 385, 20, 385, 125, 463, 75 };
static const int kGrid4Cells[] = { 10, 66, 11, 145, 92, 11, 95, 90, 95, 170, 176, 54, 176, 134, 261, 23, 259, 104, 257, 184, 344, 62, 341, 143 };

const HopscotchScene::Grid HopscotchScene::kGrids[kGridCount] = {
	{ 320, 192, 6, kGrid1Cells },
	{ 373, 200, 9, kGrid2Cells },
	{ 206, 220, 9, kGrid3Cells },
	{ 251, 195, 12, kGrid4Cells }
};

// Where Ludvig (0x49e5e8) and Solan (0x49e608) stand next to each grid.
const int HopscotchScene::kStartPos[2][kGridCount][2] = {
	{ { 410, 432 }, { 287, 372 }, { 137, 381 }, { 422, 456 } },
	{ { 413, 151 }, { 283, 216 }, { 135, 219 }, { 427, 134 } }
};

// Tone clips per grid: "<prefix><square>.smk" (FUN_0043cac0). The plain
// set is in data/hopscotch.bin, the others in data1/hopscotch.bin.
const char *const HopscotchScene::kTonePrefix[kGridCount] = { "", "B", "C", "D" };

static const float kJumpSpeed = 0.002f;   ///< 0x49e92c: fraction of the distance per ms
static const float kDirectionThreshold = 0.5f;

HopscotchScene::HopscotchScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def),
	_grid(0), _gridAnim(nullptr), _scoreBox(nullptr), _score(0), _level(0), _round(0),
	_turnStart(0), _state(kStateIdle), _timerFrame(-1), _messageBox(false) {
	for (int i = 0; i < kMaxCells; i++)
		_cells[i] = nullptr;
	for (int i = 0; i < kMaxSequence; i++)
		_sequence[i] = 0;
	initJumper(_lud, 0);
	initJumper(_sol, 1);
}

HopscotchScene::~HopscotchScene() {
}

bool HopscotchScene::load() {
	if (!Scene::load())
		return false;
	_font14.load("Amerigo BT_14_");
	_font10.load("Amerigo BT_10_");
	return true;
}

// FUN_0043bc90, the init handler.
void HopscotchScene::onInit(int arg) {
	// The original stops the music (FUN_0040b2f0); the scene has none.
	_vm->_music->stop();

	// Score box: a text area at (315, 2)-(506, 31), z -10, showing the
	// scene title in the small font until a game starts.
	_scoreBox = defineSurfaceAnim("scorebox", 191, 29, _vm->_screen->format.RGBToColor(0, 255, 0));
	_scoreBox->add(315, 2, kZTimer);
	drawBoxText(_font10, _vm->getString("hopscotch:SCENENAME"));

	defineButton(kButtonHelp, 0, 0, "help_0", "help_1", "", true);
	defineButton(kButtonExit, 728, 0, "exit_0", "exit_1", "", true);
	defineButton(kButtonStart, 13, 497, "", "start_1", "start_2", false);
	setButtonText(kButtonStart, _vm->getString("hopscotch:START"));

	initJumper(_lud, 0);
	initJumper(_sol, 1);
	resetTimer();
	_score = 0;
	_level = 0;
	_round = 0;
	_turnStart = 0;
	_state = kStateIdle;
	setGrid(0, false);

	// TODO: with a player profile the original shows the tournament
	// "PLAYERREADY" message box (FUN_00419660) and starts the game at once.
}

// ---- buttons (BUTTON module) ---------------------------------------------

// FUN_00406ed0 / FUN_00406540: a push button with up to three bitmaps.
void HopscotchScene::defineButton(int id, int x, int y, const char *bmp0, const char *bmp1, const char *bmp2, bool transparent) {
	PushButton b;
	b.id = id;
	b.bitmaps[0] = bmp0;
	b.bitmaps[1] = bmp1;
	b.bitmaps[2] = bmp2;
	b.state = 0;
	b.shown = nullptr;
	b.text = nullptr;
	b.rect = Common::Rect(x, y, x, y);
	// The button's pixels report its id as hotspot (the clicks on them are
	// ignored by the scene handler, FUN_0043c7d0).
	for (int i = 0; i < 3; i++)
		if (!b.bitmaps[i].empty())
			defineAnim(b.bitmaps[i].c_str(), false, transparent, id, x, y, kZButton);
	// FUN_004069b0: the rectangle comes from the first bitmap there is.
	for (int i = 0; i < 3; i++) {
		if (b.bitmaps[i].empty())
			continue;
		Anim *a = anim(b.bitmaps[i].c_str());
		a->add(x, y, kZButton);
		b.rect = a->rect();
		a->remove();
		break;
	}
	_buttons.push_back(b);
	showButton(_buttons.back());
}

HopscotchScene::PushButton *HopscotchScene::button(int id) {
	for (uint i = 0; i < _buttons.size(); i++)
		if (_buttons[i].id == id)
			return &_buttons[i];
	return nullptr;
}

// FUN_00406760: shows the bitmap of the current state, if any.
void HopscotchScene::showButton(PushButton &b) {
	Anim *want = b.bitmaps[b.state].empty() ? nullptr : anim(b.bitmaps[b.state].c_str());
	if (want == b.shown)
		return;
	if (b.shown)
		b.shown->remove();
	b.shown = want;
	if (want)
		want->add(b.rect.left, b.rect.top, kZButton);
}

// FUN_00406720
void HopscotchScene::setButtonState(int id, int state) {
	PushButton *b = button(id);
	if (b && b->state != state) {
		b->state = state;
		showButton(*b);
	}
}

// FUN_00406950: the label is a text area over the button in the button
// font, centred, one z level above the bitmap.
void HopscotchScene::setButtonText(int id, const Common::String &text) {
	PushButton *b = button(id);
	if (!b)
		return;
	if (!b->text) {
		b->text = defineSurfaceAnim(Common::String::format("button%dtext", id).c_str(), b->rect.width(), b->rect.height(),
		                            _vm->_screen->format.RGBToColor(0, 255, 0));
		b->text->add(b->rect.left, b->rect.top, kZButton + 1);
	}
	Graphics::ManagedSurface *s = b->text->surface();
	s->fillRect(Common::Rect(0, 0, s->w, s->h), s->format.RGBToColor(0, 255, 0));
	_font14.drawStringCentred(*s, Common::Rect(0, 0, s->w, s->h), text);
}

// FUN_00406b50 / FUN_00406c50: a press on an enabled button shows its pressed bitmap.
void HopscotchScene::buttonDown(int x, int y) {
	for (uint i = 0; i < _buttons.size(); i++) {
		PushButton &b = _buttons[i];
		if (b.rect.contains(x, y) && b.state == 0) {
			b.state = 1;
			showButton(b);
			return;
		}
	}
}

// FUN_00406cf0 / FUN_00406d40: releasing anywhere fires the pressed button (event 0x116).
void HopscotchScene::buttonUp(int x, int y) {
	for (uint i = 0; i < _buttons.size(); i++) {
		PushButton &b = _buttons[i];
		if (b.state == 1) {
			b.state = 0;
			showButton(b);
			onButton(b.id);
			return;
		}
	}
}

// FUN_0043c910, the 0x116 handler.
void HopscotchScene::onButton(int id) {
	switch (id) {
	case kButtonHelp:
		_vm->showHelp();
		break;
	case kButtonExit:
		// FUN_0040cc30 (asks "gamec:SUBGAMEABORT" while a game runs)
		_vm->endGame();
		break;
	case kButtonStart:
		if (_state == kStateIdle) {
			startGame();
		} else if (_state == kStatePlayer) {
			// Hint: Solan repeats the sequence, the hourglass keeps running.
			_lud.step = 0;
			resetJumper(_lud);
			setState(kStateHint);
			_sol.step = 0;
			solanNext();
		}
		break;
	default:
		break;
	}
}

// ---- grid and squares ----------------------------------------------------

// FUN_0043c430
const HopscotchScene::Level &HopscotchScene::level() const {
	if (_level < 0)
		return kLevels[0];
	return kLevels[MIN(_level, kLevelCount - 1)];
}

// FUN_0043be60: puts up a grid with its squares and both characters.
void HopscotchScene::setGrid(int grid, bool symbols) {
	_grid = grid;
	if (_gridAnim && _gridAnim->isAdded())
		_gridAnim->remove();
	const Grid &g = kGrids[grid];
	_gridAnim = defineAnim(Common::String::format("grid%d", grid + 1).c_str(), false, true, 0, g.x, g.y, 0);
	_gridAnim->add(g.x, g.y, 0);
	setupCells(symbols);
	resetJumper(_lud);
	resetJumper(_sol);
}

// FUN_0043c1c0 / FUN_0043c270: number01.. or symbol01.. on the squares, z 2.
void HopscotchScene::setupCells(bool symbols) {
	removeCells();
	const Grid &g = kGrids[_grid];
	for (int i = 0; i < g.count; i++) {
		Common::Point p = cellPos(i);
		_cells[i] = defineAnim(Common::String::format("%s%02d", symbols ? "symbol" : "number", i + 1).c_str(), false, true, 0, p.x, p.y, kZCells);
		_cells[i]->add(p.x, p.y, kZCells);
	}
}

// FUN_0043c2e0
void HopscotchScene::removeCells() {
	for (int i = 0; i < kMaxCells; i++) {
		if (_cells[i] && _cells[i]->isAdded())
			_cells[i]->remove();
		_cells[i] = nullptr;
	}
}

// FUN_0043c230
Common::Point HopscotchScene::cellPos(int cell) const {
	const Grid &g = kGrids[_grid];
	return Common::Point(g.cells[cell * 2] + g.x, g.cells[cell * 2 + 1] + g.y);
}

// FUN_0043c890: the square whose 75x75 rectangle contains the point, or -1.
int HopscotchScene::cellAt(int x, int y) const {
	const Grid &g = kGrids[_grid];
	for (int i = 0; i < g.count; i++) {
		Common::Point p = cellPos(i);
		if (x >= p.x && x < p.x + kCellSize && y >= p.y && y < p.y + kCellSize)
			return i;
	}
	return -1;
}

// FUN_0043c0a0: the sprite position that centres a jump clip on a square.
Common::Point HopscotchScene::spritePos(int x, int y) const {
	return Common::Point(x + (kCellSize - kSpriteSize) / 2, y + (kCellSize - kSpriteSize) / 2);
}

// FUN_0043cac0 / FUN_0043cb20: the tone of a square (animation/<n>.smk).
// The original plays a clone of the audio element so tones can overlap.
void HopscotchScene::playTone(int cell) {
	Common::String name = Common::String::format("%s%d", kTonePrefix[level().grid], cell + 1);
	if (!resources()->exists(_name, Common::String::format("animation/%s.smk", name.c_str()))) {
		debug(2, "Hopscotch: tone %s missing", name.c_str());
		return;
	}
	// Audio only clips of the original: not visible. Keep the 4x4 frames
	// off screen since defineAnim() makes visible elements.
	Anim *a = defineAnim(name.c_str(), true, false, 0, -16, -16, 0);
	if (a->isPlaying())
		a->stop();
	a->play();
	a->setRemoveWhenDone(true);
}

// ---- jumpers -------------------------------------------------------------

// FUN_0043be30
void HopscotchScene::initJumper(Jumper &j, int id) {
	j.id = id;
	j.x = j.y = 0;
	j.targetX = j.targetY = 0;
	j.dir = 0;
	j.step = 0;
	j.startTime = 0;
	j.dirX = j.dirY = j.dist = 0;
	j.anim = nullptr;
}

// FUN_0043bee0: back to the start position next to the grid, facing it.
void HopscotchScene::resetJumper(Jumper &j) {
	const int *start = kStartPos[j.id][_grid];
	if (j.anim && j.anim->isPlaying())
		j.anim->stop();
	Common::Point p = spritePos(start[0], start[1]);
	int dir = directionToGrid(start[0], start[1]);
	setClip(j, kClipStart, p.x, p.y, dir);
}

// FUN_0043c0f0
void HopscotchScene::setClip(Jumper &j, int kind, int x, int y, int dir) {
	setClipAnim(j, kind, x, y, dir);
	j.dir = dir;
	j.x = x;
	j.y = y;
}

// FUN_0043c130: "<lud|sol><jump|fall|start><angle>" added at (x, y), z 5.
// Clips that do not exist (Solan only has start poses 090 and 180, in
// data1/hopscotch.bin, and no fall) are not shown, like the original's
// element without a player handle.
void HopscotchScene::setClipAnim(Jumper &j, int kind, int x, int y, int dir) {
	static const char *const kinds[3] = { "jump", "fall", "start" };
	if (j.anim && j.anim->isAdded())
		j.anim->remove();
	Common::String name = Common::String::format("%s%s%03d", j.id == 0 ? "lud" : "sol", kinds[kind], dir);
	if (!resources()->exists(_name, Common::String::format("animation/%s.smk", name.c_str()))) {
		debug(2, "Hopscotch: clip %s missing", name.c_str());
		j.anim = nullptr;
		return;
	}
	j.anim = defineAnim(name.c_str(), true, true, 0, x, y, kZJumper);
	j.anim->add(x, y, kZJumper);
}

// FUN_0043c6b0: direction and distance of the hop from the current position.
void HopscotchScene::setTarget(Jumper &j, int x, int y) {
	float dx = (float)x - (float)j.x;
	float dy = (float)y - (float)j.y;
	j.targetX = x;
	j.targetY = y;
	j.dist = sqrt(dx * dx + dy * dy);
	if (j.dist < 0.001f) {
		j.dirX = j.dirY = 0;
	} else {
		j.dirX = dx / j.dist;
		j.dirY = dy / j.dist;
	}
	j.startTime = g_engine->getGameMillis();
}

// FUN_0043c610: hops to a square (its top left corner is given).
void HopscotchScene::jumpTo(Jumper &j, int kind, int cellX, int cellY) {
	Common::Point p = spritePos(cellX, cellY);
	setTarget(j, p.x, p.y);
	int dir = (j.dirX == 0 && j.dirY == 0) ? j.dir : deltaDirection(j.dirX, j.dirY);
	setClip(j, kind, j.x, j.y, dir);
	if (j.anim)
		j.anim->play();
}

// FUN_0043cce0: the sprite moves linearly over the first 500 ms of the clip.
void HopscotchScene::moveJumper(Jumper &j, uint32 now) {
	int t = (int)(now - j.startTime);
	t = CLIP(t, 0, (int)kJumpTime);
	float d = j.dist * kJumpSpeed * t;
	float fx = d * j.dirX, fy = d * j.dirY;
	int x = j.x + (int)(fx + (fx > 0 ? 0.5f : -0.5f));
	int y = j.y + (int)(fy + (fy > 0 ? 0.5f : -0.5f));
	if (j.anim)
		j.anim->setPos(x, y);
}

// FUN_0043ca70: the clip ended; stand on the target (first frame of the
// jump clip) and play the tone of the square under the sprite centre.
int HopscotchScene::land(Jumper &j) {
	setClip(j, kClipJump, j.targetX, j.targetY, j.dir);
	int cell = cellAt(j.targetX + kSpriteSize / 2, j.targetY + 76);
	if (cell >= 0)
		playTone(cell);
	return cell;
}

// FUN_0043bf50: facing from a point towards the centre of the grid bitmap.
int HopscotchScene::directionToGrid(int x, int y) {
	Common::Rect r = _gridAnim ? _gridAnim->rect() : Common::Rect(0, 0, 0, 0);
	int cx = r.left + r.width() / 2;
	int cy = r.top + r.height() / 2;
	return deltaDirection((float)(cx - x), (float)(cy - y));
}

// FUN_0043bfc0 ("hs_GetDeltaDirection"): one of eight facings, 0 = up,
// 90 = right, clockwise.
int HopscotchScene::deltaDirection(float dx, float dy) {
	float len = sqrt(dx * dx + dy * dy);
	dx /= len;
	dy /= len;
	if (dy >= -kDirectionThreshold) {
		if (dy > kDirectionThreshold) {
			if (dx < -kDirectionThreshold)
				return 225;
			if (dx > kDirectionThreshold)
				return 135;
			return 180;
		}
		if (dx < -kDirectionThreshold)
			return 270;
		if (dx > kDirectionThreshold)
			return 90;
		warning("Hopscotch: hs_GetDeltaDirection()");
		return 0;
	}
	if (dx < -kDirectionThreshold)
		return 315;
	if (dx > kDirectionThreshold)
		return 45;
	return 0;
}

// ---- game flow -----------------------------------------------------------

// FUN_0043c310, the start button.
void HopscotchScene::startGame() {
	if (_state != kStateIdle)
		return;
	_vm->setGameRunning(true);
	resetScore();
	int level = 0;
	// TODO: with a profile the start level is three times the profile's
	// difficulty setting (FUN_004194a0, value at +0x24).
	startLevel(level);
}

// FUN_0043c3c0: new grid when it changes, new sequence, Solan shows it.
void HopscotchScene::startLevel(int level) {
	_level = level < 0 ? 0 : MIN(level, kLevelCount - 1);
	_round = 0;
	const Level &l = this->level();
	setGrid(l.grid, l.symbols != 0);
	generateSequence(CLIP(l.baseLength + l.rounds, 1, (int)kMaxSequence));
	startDemo();
}

// FUN_0043c460 with FUN_0040a150: random squares, every square once before
// any repeats.
void HopscotchScene::generateSequence(int n) {
	int maxCell = MIN(kGrids[_grid].count, level().maxCells);
	uint32 used = 0;
	for (int i = 0; i < n; i++) {
		if (used == (1u << maxCell) - 1)
			used = 0;
		int free = 0;
		for (int c = 0; c < maxCell; c++)
			if (!(used & (1u << c)))
				free++;
		int pick = free ? (int)_vm->getRandomNumber(free - 1) : 0;
		int cell = 0;
		for (int c = 0; c < maxCell; c++) {
			if (used & (1u << c))
				continue;
			if (pick < 1) {
				cell = c;
				break;
			}
			pick--;
		}
		used |= 1u << cell;
		_sequence[i] = cell;
	}
	Common::String s;
	for (int i = 0; i < n; i++)
		s += Common::String::format("%d ", _sequence[i] + 1);
	debug(1, "Hopscotch: level %d sequence %s", _level, s.c_str());
}

// FUN_0043c5e0: squares shown in the current round.
int HopscotchScene::visibleLength() const {
	const Level &l = level();
	int n = _round + l.baseLength;
	if (n < l.baseLength)
		return l.baseLength;
	return MIN(n, l.baseLength + l.rounds - 1);
}

// FUN_0043c4c0
void HopscotchScene::startDemo() {
	_lud.step = 0;
	resetJumper(_lud);
	resetJumper(_sol);
	setState(kStateDemo);
	_sol.step = 0;
	solanNext();
}

// FUN_0043c580: Solan's next hop, or the end of his demonstration.
void HopscotchScene::solanNext() {
	if (visibleLength() <= _sol.step) {
		demoDone();
		return;
	}
	Common::Point p = cellPos(_sequence[_sol.step]);
	jumpTo(_sol, kClipJump, p.x, p.y);
	_sol.step++;
}

// FUN_0043c740: Solan steps aside; the hourglass is turned before the
// first turn of a round, a hint just hands the turn back.
void HopscotchScene::demoDone() {
	resetJumper(_sol);
	if (_state == kStateDemo) {
		if (isAnimAdded("timer"))
			removeAnim("timer");
		playAnim("rotate");
		return;
	}
	setState(kStatePlayer);
}

// FUN_0043cb60, after the bird: one more square, or the next level.
void HopscotchScene::nextRound() {
	_round++;
	if (_round < level().rounds)
		startDemo();
	else
		startLevel(_level + 1);
}

// FUN_0043c4f0: the start button doubles as the hint button.
void HopscotchScene::setState(int state) {
	_state = state;
	switch (state) {
	case kStateIdle:
		setButtonText(kButtonStart, _vm->getString("hopscotch:START"));
		setButtonState(kButtonStart, 0);
		break;
	case kStatePlayer:
		setButtonState(kButtonStart, 0);
		break;
	case kStateDemo:
		setButtonText(kButtonStart, _vm->getString("hopscotch:HINT"));
		setButtonState(kButtonStart, 2);
		break;
	case kStateHint:
	case 4:
		setButtonState(kButtonStart, 2);
		break;
	default:
		break;
	}
}

// FUN_0043c800: a click on a square during the player's turn. Ludvig hops
// there, falling if it is the wrong one.
void HopscotchScene::clickCell(int x, int y) {
	if (_state != kStatePlayer || isAnimPlaying("bird") || (_lud.anim && _lud.anim->isPlaying()))
		return;
	int cell = cellAt(x, y);
	if (cell < 0)
		return;
	int expected = _sequence[_lud.step];
	Common::Point p = cellPos(cell);
	jumpTo(_lud, expected != cell ? kClipFall : kClipJump, p.x, p.y);
}

// FUN_0043cd90: time ran out.
void HopscotchScene::gameOver() {
	_vm->setGameRunning(false);
	debug(1, "Hopscotch: game over, %d points", _score);
	// The state is still the player's turn with the time up: the original's
	// box swallows the frame events, here onUpdate() skips while it is open
	// (else every nested frame would end the game again).
	_messageBox = true;
	_vm->messageBox("interfaceh:GAMEOVER", "hopscotch:GAMEOVER", MessageBox::kButtonOk);
	_messageBox = false;
	resetTimer();
	setState(kStateIdle);
	resetJumper(_lud);
	resetJumper(_sol);
	// TODO: high score registration FUN_0041f9a0(0, score, medal table
	// 0x49e918 = 3000/5000/7000/10000, 0).
	// Without a profile the score box shows the title again.
	drawBoxText(_font10, _vm->getString("hopscotch:SCENENAME"));
}

// ---- score and hourglass -------------------------------------------------

// FUN_0043c370
void HopscotchScene::resetScore() {
	_score = 0;
	drawScore();
}

// FUN_0043c380
void HopscotchScene::drawScore() {
	drawBoxText(_font14, Common::String::format("%d", _score));
}

void HopscotchScene::drawBoxText(const BitmapFont &font, const Common::String &text) {
	Graphics::ManagedSurface *s = _scoreBox->surface();
	s->fillRect(Common::Rect(0, 0, s->w, s->h), s->format.RGBToColor(0, 255, 0));
	font.drawStringCentred(*s, Common::Rect(0, 0, s->w, s->h), text);
}

// FUN_0043cb90: five points per full second left on the hourglass.
int HopscotchScene::timeBonus() const {
	int left = (int)(level().timeMs + _turnStart - g_engine->getGameMillis());
	return (left / 1000) * 5;
}

// FUN_0043bde0: the hourglass at its default place showing its last frame.
void HopscotchScene::resetTimer() {
	if (!isAnimAdded("timer"))
		addAnim("timer", Anim::kDefaultPos, Anim::kDefaultPos, kZTimer);
	Anim *t = anim("timer");
	_timerFrame = t->frameCount() - 1;
	t->showFrame(_timerFrame);
}

// FUN_0043cc40: the hourglass runs through its frames over the turn time.
// Returns false when the time is up.
bool HopscotchScene::updateTimer(uint32 now) {
	Anim *t = anim("timer");
	int last = t->frameCount() - 1;
	float v = (float)last / (float)level().timeMs * (float)(int)(now - _turnStart);
	int frame = (int)(v + (v > 0 ? 0.5f : -0.5f));
	if (last < frame)
		return false;
	if (frame != _timerFrame) {
		_timerFrame = frame;
		t->showFrame(frame);
	}
	return true;
}

// ---- events --------------------------------------------------------------

// FUN_0043c7d0 (0x105), with the BUTTON module's own mouse handling first.
void HopscotchScene::onMouseDown(int hotspot, int x, int y) {
	buttonDown(x, y);
	if (hotspot != kButtonHelp && hotspot != kButtonExit && hotspot != kButtonStart)
		clickCell(x, y);
}

void HopscotchScene::onMouseUp(int hotspot, int x, int y) {
	buttonUp(x, y);
}

void HopscotchScene::onKey(const Common::KeyState &key) {
	if (key.keycode == Common::KEYCODE_ESCAPE)
		_vm->endGame();
}

// FUN_0043c980 (0x110).
void HopscotchScene::onAnimFinished(Anim *a) {
	const Common::String n(a->name());
	if (n == "rotate") {
		// The hourglass has been turned: the player's time starts.
		a->remove();
		addAnim("timer", Anim::kDefaultPos, Anim::kDefaultPos, kZTimer);
		_timerFrame = -1;
		_turnStart = g_engine->getGameMillis();
		setState(kStatePlayer);
	} else if (a == _sol.anim) {
		land(_sol);
		solanNext();
	} else if (a == _lud.anim) {
		int cell = land(_lud);
		if (cell == _sequence[_lud.step]) {
			_lud.step++;
			if (visibleLength() <= _lud.step) {
				// Round done: 100 points plus the time bonus, then the bird.
				_score += 100;
				_score += timeBonus();
				drawScore();
				playAnim("bird");
			} else {
				_score += 5;
				drawScore();
			}
		} else {
			// Wrong square: start the sequence over.
			_lud.step = 0;
			resetJumper(_lud);
		}
	} else if (n == "bird") {
		a->remove();
		nextRound();
	}
}

// FUN_0043cbc0 (0x112).
void HopscotchScene::onUpdate() {
	if (_messageBox)
		return;
	uint32 now = g_engine->getGameMillis();
	if (_sol.anim && _sol.anim->isPlaying())
		moveJumper(_sol, now);
	if (_lud.anim && _lud.anim->isPlaying())
		moveJumper(_lud, now);
	if ((_state == kStateHint || _state == kStatePlayer) && !isAnimPlaying("bird")) {
		if (!updateTimer(now))
			gameOver();
	}
}

} // End of namespace Flaaklypa
