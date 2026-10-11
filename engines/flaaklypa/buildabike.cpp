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

#include "flaaklypa/buildabike.h"
#include "flaaklypa/cheats.h"
#include "flaaklypa/dialog.h"
#include "flaaklypa/hiscore.h"
#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/resources.h"
#include "flaaklypa/tournament.h"

namespace Flaaklypa {

// Part types in the order of the name tables at 0x4d7648 (broken_*),
// 0x4d7674 (fixed_*), 0x4d76a0 (line_*) and 0x4d76cc (wall_*).
const char *const BuildabikeScene::kPartNames[kTypes] = {
	"blackseat", "blueframe", "bottle", "brownseat", "frontwheel", "handlebar",
	"headlight", "pedals", "rack", "rearwheel", "redframe"
};

// Draw order of the nine part slots (0x49dbcc): bottle, frame, front wheel,
// handlebar, headlight, pedals, rack, rear wheel, seat.
const int BuildabikeScene::kSlotZ[kSlots] = { 4, 3, 2, 4, 4, 1, 4, 2, 4 };

// Position of a part inside the bike area, by type (0x49dcd0 fixed, 0x49dc20 broken).
const int BuildabikeScene::kFixedOffset[kTypes][2] = {
	{ 131, 20 }, { 35, 35 }, { 126, 47 }, { 134, 25 }, { 0, 58 }, { 68, 8 },
	{ 47, 36 }, { 113, 81 }, { 159, 50 }, { 141, 58 }, { 35, 35 }
};
const int BuildabikeScene::kBrokenOffset[kTypes][2] = {
	{ 132, 15 }, { 35, 31 }, { 126, 47 }, { 134, 28 }, { 2, 60 }, { 66, 0 },
	{ 44, 36 }, { 109, 83 }, { 158, 42 }, { 142, 65 }, { 35, 36 }
};

// Points for a repaired slot (0x49dfc0) and health lost for a broken or
// missing one (0x49dfe4), by slot.
const int BuildabikeScene::kBonus[kSlots] = { 50, 110, 100, 90, 50, 90, 60, 100, 90 };
const int BuildabikeScene::kPenalty[kSlots] = { 5, 20, 15, 10, 5, 10, 5, 15, 10 };

// The store on the wall: bitmap positions (0x49dd80), z (0x49dba0) and the
// 11x14 count text areas (0x49de30).
const int BuildabikeScene::kWallPos[kTypes][2] = {
	{ 511, 232 }, { 102, 120 }, { 476, 232 }, { 553, 228 }, { 293, 112 }, { 407, 120 },
	{ 589, 247 }, { 405, 270 }, { 352, 255 }, { 29, 241 }, { 169, 244 }
};
const int BuildabikeScene::kWallZ[kTypes] = { 4, 3, 4, 4, 2, 4, 4, 1, 4, 2, 3 };
const int BuildabikeScene::kWallTextPos[kTypes][2] = {
	{ 521, 278 }, { 178, 191 }, { 481, 278 }, { 564, 278 }, { 312, 206 }, { 448, 155 },
	{ 603, 278 }, { 405, 364 }, { 350, 317 }, { 85, 331 }, { 259, 313 }
};

// Bike areas (0x49dee0, 237x170), platform bitmaps (0x49df40) and the 49x14
// time text areas (0x49df70).
const int BuildabikeScene::kBikeOrigin[kBikes][2] = { { 20, 425 }, { 288, 425 }, { 556, 425 } };
const int BuildabikeScene::kPlatformPos[kBikes][2] = { { 10, 552 }, { 281, 552 }, { 538, 551 } };
const int BuildabikeScene::kBikeTextPos[kBikes][2] = { { 177, 377 }, { 248, 377 }, { 316, 377 } };

// Per difficulty (0x49e014): bikes counted as done at the start, health.
const int BuildabikeScene::kLevelTable[kLevels][2] = { { 0, 100 }, { 5, 75 }, { 10, 50 } };
// Bikes done before a second and a third platform come into play (0x49e008).
const int BuildabikeScene::kBikeThresholds[kBikes] = { 0, 15, 40 };

// Frames and the rack have "<name>_hs.bmp" hit masks (0x4d75f0 / 0x4d761c).
const bool BuildabikeScene::kHasHitMask[kTypes] = {
	false, true, false, false, false, false, false, false, true, false, true
};

enum {
	kBikeHeight = 170,
	kBeltY = 60,
	kBeltX0 = -100,
	kBeltLength = 900,
	kZDrag = 21,
	kZStart = 22,
	kZCover = 17,
	kZWallText = 18,
	kZBikeText = 19,
	kZScore = 5,
	kHealthBarWidth = 119
};

static const float kBeltTime = 15.0f;        ///< seconds for a part to cross the screen (0x49e044)
static const float kRecycleTime = 25.0f;     ///< seconds a part spends in the machine (0x664340)
static const float kLiftTime = 2.0f;         ///< seconds a platform takes to rise or sink (0x49e040)
static const float kScoreSpeed = 250.0f;     ///< points per second the score display counts (0x49e048)
static const float kHealthSpeed = 50.0f;     ///< health bar units per second (0x49e04c)
static const float kTimeBonus = 5.0f;        ///< points per second left when a bike is finished (0x49d9fc)

BuildabikeScene::BuildabikeScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def),
	_solidMask(nullptr), _energy(nullptr), _queueWrite(0), _scoreText(nullptr), _healthBar(nullptr), _drag(nullptr),
	_dragKind(kKindNone), _dragType(0), _dragOrigin(0), _dragData(0), _running(false), _level(1),
	_bikesDone(0), _score(0), _scoreDisplay(0), _health(0), _maxHealth(100), _healthDisplay(0),
	_healthWidth(-1), _lastTick(0), _startTime(0), _highlighted(0) {
	// One animation definition per bitmap / clip; the parts on screen are
	// Anim objects of these owned by the scene (the original copies the file
	// name into its per slot animation structures instead).
	static const char *const prefixes[4] = { "broken_", "fixed_", "wall_", "line_" };
	for (int k = 0; k < 4; k++)
		for (int t = 0; t < kTypes; t++) {
			_defNames[k][t] = Common::String(prefixes[k]) + kPartNames[t];
			AnimDef &d = _partDefs[k][t];
			d.name = _defNames[k][t].c_str();
			d.smacker = k == kKindLine;
			d.visible = 1;
			d.transparent = 1;
			d.loop = k == kKindLine;
			d.hotspot = 0;
			d.x = d.y = 0;
			d.group = 0;
			d.zOrder = 0;
			d.overlay = 0;
		}
	for (int k = 0; k < 2; k++)
		for (int t = 0; t < kTypes; t++)
			_hitMasks[k][t] = nullptr;
	for (int b = 0; b < kBikes; b++) {
		Bike &bike = _bikes[b];
		bike.platform = nullptr;
		for (int s = 0; s < kSlots; s++) {
			bike.parts[s] = nullptr;
			bike.partKind[s] = kKindNone;
			bike.partType[s] = 0;
		}
		bike.state = kStateTop;
		bike.lift = 0;
		bike.timeLeft = 0;
		bike.active = bike.pending = false;
		bike.generation = 0;
		bike.brokenMask = 0;
		_bikeTexts[b] = nullptr;
	}
	for (int i = 0; i < kBeltItems; i++) {
		_belt[i].anim = nullptr;
		_belt[i].type = 0;
		_belt[i].generation = 0;
		_belt[i].pos = 0;
	}
	for (int i = 0; i < kQueueSize; i++) {
		_queue[i].type = 0;
		_queue[i].timer = 0;
		_queue[i].used = false;
	}
	for (int t = 0; t < kTypes; t++) {
		_wallCount[t] = 0;
		_wallParts[t] = nullptr;
		_wallTexts[t] = nullptr;
	}
}

BuildabikeScene::~BuildabikeScene() {
	// The Anim objects owned here must leave the scene's active list before
	// Scene's destructor runs.
	for (int b = 0; b < kBikes; b++)
		for (int s = 0; s < kSlots; s++)
			deleteAnim(_bikes[b].parts[s]);
	for (int i = 0; i < kBeltItems; i++)
		deleteAnim(_belt[i].anim);
	for (int t = 0; t < kTypes; t++)
		deleteAnim(_wallParts[t]);
	deleteAnim(_drag);
	for (int k = 0; k < 2; k++)
		for (int t = 0; t < kTypes; t++)
			if (_hitMasks[k][t]) {
				_hitMasks[k][t]->free();
				delete _hitMasks[k][t];
			}
	if (_solidMask) {
		_solidMask->free();
		delete _solidMask;
	}
	delete _energy;
}

bool BuildabikeScene::load() {
	if (!Scene::load())
		return false;
	_energy = resources()->loadBitmap(_name, "bitmap/energy.bmp", _vm->_screen->format);
	if (!_energy)
		warning("Buildabike: bitmap/energy.bmp missing");
	for (int t = 0; t < kTypes; t++)
		if (kHasHitMask[t]) {
			_hitMasks[kKindBroken][t] = resources()->loadMask(_name, Common::String::format("bitmap/broken_%s_hs.bmp", kPartNames[t]));
			_hitMasks[kKindFixed][t] = resources()->loadMask(_name, Common::String::format("bitmap/fixed_%s_hs.bmp", kPartNames[t]));
		}
	_font.load("Counter");
	// Parts are hit tested by their rectangle (hotspot mode 2); the largest
	// bitmap is 200x100.
	_solidMask = new Graphics::Surface();
	_solidMask->create(256, 256, Graphics::PixelFormat::createFormatCLUT8());
	memset(_solidMask->getPixels(), 1, 256 * 256);

	// Sound effects, played as clones by the original (FUN_00438250).
	static const char *const sounds[] = { "click", "correct", "wrong", "down", "up", "drop1", "drop2", nullptr };
	for (int i = 0; sounds[i]; i++)
		defineAnim(sounds[i], true, true, 0, 0, 0, 0);
	return true;
}

// ---- screen (FUN_004391a0) -----------------------------------------------

void BuildabikeScene::onInit(int arg) {
	const uint32 green = _vm->_screen->format.RGBToColor(0, 255, 0);

	playMusic("subgame8");
	// FUN_0040ad50: typing MAJKA fills the store (the close removes it,
	// FUN_0040ae10; here the engine)
	_vm->_cheats->add("MAJKA", new Common::Functor0Mem<void, BuildabikeScene>(this, &BuildabikeScene::cheatMajka), this);
	defineAnim("help_0", false, true, kHotspotHelp, 0, 0, 0)->add(0, 0, 0);
	defineAnim("help_1", false, true, kHotspotHelp, 0, 0, 0);
	defineAnim("exit_0", false, true, kHotspotExit, 728, 0, 0)->add(728, 0, 0);
	defineAnim("exit_1", false, true, kHotspotExit, 728, 0, 0);
	_highlighted = 0;
	addAnim("points", Anim::kDefaultPos, Anim::kDefaultPos, kZScore);

	// Text areas drawn with the "Counter" font (FUN_004392f0, FUN_00439320, FUN_00439360).
	for (int b = 0; b < kBikes; b++) {
		_bikeTexts[b] = defineSurfaceAnim(Common::String::format("biketext%d", b).c_str(), 49, 14, green);
		_bikeTexts[b]->add(kBikeTextPos[b][0], kBikeTextPos[b][1], kZBikeText);
	}
	_scoreText = defineSurfaceAnim("scoretext", 73, 14, green);
	_scoreText->add(555, 133, kZScore);
	// The health bar is a PROGRESS object of the original (energy.bmp at 512,162).
	_healthBar = defineSurfaceAnim("healthbar", kHealthBarWidth, 4, green);
	_healthBar->add(512, 162, kZScore);
	_healthWidth = -1;
	for (int t = 0; t < kTypes; t++) {
		_wallTexts[t] = defineSurfaceAnim(Common::String::format("walltext%d", t).c_str(), 11, 14, green);
		_wallTexts[t]->add(kWallTextPos[t][0], kWallTextPos[t][1], kZWallText);
	}

	// Platforms (FUN_00439450) and the cover hiding the bikes below the floor.
	static const char *const platforms[kBikes] = { "platform_left", "platform_middle", "platform_right" };
	for (int b = 0; b < kBikes; b++) {
		_bikes[b].platform = defineAnim(platforms[b], false, true, 0, kPlatformPos[b][0], kPlatformPos[b][1], 0);
		_bikes[b].platform->add(kPlatformPos[b][0], kPlatformPos[b][1], 0);
	}
	addAnim("platform_cover", Anim::kDefaultPos, Anim::kDefaultPos, kZCover);

	// Belt slots evenly spaced (FUN_004395b0) and an empty machine (FUN_004395f0).
	for (int i = 0; i < kBeltItems; i++)
		_belt[i].pos = (float)i / kBeltItems;

	addAnim("lcogwheel");
	addAnim("rcogwheel");
	addAnim("wheel");
	addAnim("lever");

	_running = false;
	// In tournament mode (FUN_00419490) there is no start button: the
	// "player ready" box (FUN_00419660), then the game starts at once
	// (FUN_00439620) at the player's difficulty.
	if (!_vm->tournament()->isActive()) {
		addAnim("start", Anim::kDefaultPos, Anim::kDefaultPos, kZStart);
	} else {
		_vm->tournament()->showPlayerReady();
		startGame();
	}
}

void BuildabikeScene::onClose() {
	// FUN_004398c0
	_running = false;
}

void BuildabikeScene::highlightButton(int hotspot) {
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

void BuildabikeScene::onMouseMove(int hotspot, int x, int y) {
	highlightButton(hotspot);
}


void BuildabikeScene::onAnimFinished(Anim *a) {
	const Common::String n(a->name());
	if (n == "click" || n == "correct" || n == "wrong" || n == "down" || n == "up" || n == "drop1" || n == "drop2")
		a->remove();
}

void BuildabikeScene::playSound(const char *name) {
	Anim *a = anim(name);
	if (a->isPlaying())
		a->stop();
	a->play();
}

// ---- texts ---------------------------------------------------------------

// FUN_004079c0: flags 0x201 right aligned, 0x101 centred, vertically centred.
void BuildabikeScene::drawText(Anim *a, const Common::String &text, bool right) {
	Graphics::ManagedSurface *s = a->surface();
	s->fillRect(Common::Rect(0, 0, s->w, s->h), s->format.RGBToColor(0, 255, 0));
	int w = _font.stringWidth(text);
	int x = right ? s->w - w : (s->w - w) / 2;
	int y = (s->h - _font.height()) / 2;
	_font.drawString(*s, x, y, text);
}

void BuildabikeScene::clearText(Anim *a) {
	Graphics::ManagedSurface *s = a->surface();
	s->fillRect(Common::Rect(0, 0, s->w, s->h), s->format.RGBToColor(0, 255, 0));
}

// ---- parts ---------------------------------------------------------------

Anim *BuildabikeScene::newPart(int kind, int type, int hotspot) {
	Anim *a = new Anim(this, &_partDefs[kind][type]);
	a->setHotspot(hotspot);
	const Graphics::Surface *mask = (kind == kKindBroken || kind == kKindFixed) ? _hitMasks[kind][type] : nullptr;
	a->setHitMask(mask ? mask : _solidMask);
	return a;
}

void BuildabikeScene::deleteAnim(Anim *&a) {
	if (!a)
		return;
	a->remove();
	delete a;
	a = nullptr;
}

// FUN_00439ec0: the slot a part type goes into (both frames share slot 1, both seats slot 8).
int BuildabikeScene::slotForType(int type) {
	static const int slots[kTypes] = { 8, 1, 0, 8, 2, 3, 4, 5, 6, 7, 1 };
	return slots[type];
}

// FUN_00438f10: the part type of a slot on a new bike (random frame colour and seat).
int BuildabikeScene::typeForSlot(int slot) {
	switch (slot) {
	case 0: return 2;
	case 1: return _vm->getRandomNumber(1) ? 1 : 10;
	case 2: return 4;
	case 3: return 5;
	case 4: return 6;
	case 5: return 7;
	case 6: return 8;
	case 7: return 9;
	case 8: return _vm->getRandomNumber(1) ? 0 : 3;
	default: return -1;
	}
}

// FUN_00438680: puts a part bitmap into a slot of a bike.
void BuildabikeScene::placePart(int bike, int slot, int kind, int type) {
	Bike &b = _bikes[bike];
	deleteAnim(b.parts[slot]);
	b.partKind[slot] = kind;
	b.partType[slot] = type;
	const int *off = kind == kKindBroken ? kBrokenOffset[type] : kFixedOffset[type];
	int x = kBikeOrigin[bike][0] + off[0];
	int y = kBikeOrigin[bike][1] + off[1];
	Anim *a = newPart(kind, type, kHotspotSlotFirst + bike * kSlots + slot);
	// The original adds at the resting position; the lift offset is applied
	// by the next platform move (always in the same frame for new bikes).
	a->add(x, y + (b.platform->y() - kPlatformPos[bike][1]), kSlotZ[slot]);
	b.parts[slot] = a;
	debug(1, "Buildabike: bike %d slot %d gets %s", bike, slot, a->name());
}

void BuildabikeScene::removePart(int bike, int slot) {
	deleteAnim(_bikes[bike].parts[slot]);
}

// FUN_00438640
void BuildabikeScene::clearParts(int bike) {
	for (int s = 0; s < kSlots; s++)
		removePart(bike, s);
}

// FUN_00438510: a new bike with 1..4 broken parts, more as the game goes on.
void BuildabikeScene::spawnParts(int bike) {
	int broken = _bikesDone / 3 - (int)_vm->getRandomNumber(2) + 1;
	broken = CLIP(broken, 1, 4);
	int start = _vm->getRandomNumber(kSlots - 1);
	Bike &b = _bikes[bike];
	for (int i = 0; i < kSlots; i++) {
		int s = (start + i) % kSlots;
		int type = typeForSlot(s);
		placePart(bike, s, broken > 0 ? kKindBroken : kKindFixed, type);
		if (broken > 0)
			b.brokenMask |= 1 << s;
		else
			b.brokenMask &= ~(1 << s);
		broken--;
	}
	for (int s = 0; s < kSlots; s++)
		debug(1, "Buildabike: bike %d slot %d: %s %s at %d,%d", bike, s, b.partKind[s] == kKindBroken ? "broken" : "fixed",
		      kPartNames[b.partType[s]], b.parts[s]->x(), b.parts[s]->y() - (b.platform->y() - kPlatformPos[bike][1]));
}

// FUN_00438840: points for repaired slots, health lost for broken or missing
// ones. Returns true when nothing is wrong with the bike.
bool BuildabikeScene::scoreBike(int bike, bool apply) {
	Bike &b = _bikes[bike];
	int bonus = 0, penalty = 0;
	for (int s = 0; s < kSlots; s++) {
		if (b.partKind[s] != kKindBroken && b.parts[s]) {
			if (b.brokenMask & (1 << s))
				bonus += kBonus[s];
		} else {
			penalty -= kPenalty[s];
		}
	}
	if (apply) {
		_score += bonus;
		_health += penalty;
		debug(1, "Buildabike: bike %d scored %d, health %d -> score %d", bike, bonus, penalty, _score);
	}
	return penalty >= 0;
}

// FUN_00439d70's completion test: no slot remembers a broken part. Empty
// slots count by their last part, like the original's name strings.
bool BuildabikeScene::bikeComplete(int bike) const {
	for (int s = 0; s < kSlots; s++)
		if (_bikes[bike].partKind[s] == kKindBroken)
			return false;
	return true;
}

void BuildabikeScene::cheatMajka() {
	for (int i = 0; i < kTypes; i++)
		setWallCount(i, 9);
}

// FUN_00438e50: the store holds up to nine of each part.
void BuildabikeScene::setWallCount(int type, int count) {
	count = CLIP(count, 0, 9);
	_wallCount[type] = count;
	drawText(_wallTexts[type], Common::String::format("%d", count), false);
	if (count > 0) {
		if (!_wallParts[type]) {
			_wallParts[type] = newPart(kKindWall, type, kHotspotWallFirst + type);
			_wallParts[type]->add(kWallPos[type][0], kWallPos[type][1], kWallZ[type]);
		}
	} else {
		deleteAnim(_wallParts[type]);
	}
}

// FUN_00438170 + FUN_004387d0: moves a platform and the parts on it.
void BuildabikeScene::setLift(int bike, float lift) {
	Bike &b = _bikes[bike];
	b.lift = lift;
	int y = kPlatformPos[bike][1] + (int)(lift * kBikeHeight);
	b.platform->setPos(kPlatformPos[bike][0], y);
	int dy = y - kPlatformPos[bike][1];
	for (int s = 0; s < kSlots; s++)
		if (b.parts[s]) {
			const int *off = b.partKind[s] == kKindBroken ? kBrokenOffset[b.partType[s]] : kFixedOffset[b.partType[s]];
			b.parts[s]->setPos(kBikeOrigin[bike][0] + off[0], kBikeOrigin[bike][1] + off[1] + dy);
		}
}

// FUN_004381e0
void BuildabikeScene::setBikeState(int bike, int state) {
	Bike &b = _bikes[bike];
	b.state = state;
	if (state == kStateDown) {
		playSound("down");
		if (b.active)
			playSound(scoreBike(bike, false) ? "correct" : "wrong");
	} else if (state == kStateUp) {
		playSound("up");
	}
}

// FUN_004384f0
int BuildabikeScene::allowedBikes() const {
	int n = 0;
	for (int i = 0; i < kBikes; i++)
		if (kBikeThresholds[i] <= _bikesDone)
			n++;
	return n;
}

// FUN_00438480: seconds for a bike, shrinking as bikes get done and growing
// (by 0.8 per extra platform) when several bikes are in play.
float BuildabikeScene::bikeTime() const {
	float base = (4.0f / (float)(_bikesDone * 2 + 10) + 0.6f) * 42.0f;
	float total = base, f = base;
	int n = allowedBikes();
	for (int i = 2; i <= n; i++) {
		f *= 0.8f;
		total += f;
	}
	return total;
}

// ---- game flow -----------------------------------------------------------

// FUN_00439620
void BuildabikeScene::startGame() {
	_vm->setGameRunning(true);
	_running = true;
	// The difficulty: the tournament player's level (FUN_004194a0: 0 easy,
	// 1 medium, 2 hard) in tournament mode (FUN_00419490), else 1.
	_level = _vm->tournament()->playerLevel(1);
	_bikesDone = kLevelTable[_level][0];
	_lastTick = g_engine->getGameMillis();
	_startTime = _lastTick;
	_score = 0;
	updateScoreText(0);
	_maxHealth = kLevelTable[_level][1];
	_health = _maxHealth;
	// FUN_004396d0(0): the bar starts empty and fills up.
	_healthDisplay = 0;
	_healthWidth = -1;
	updateHealthBar(0);

	// FUN_00439730
	for (int b = 0; b < kBikes; b++) {
		Bike &bike = _bikes[b];
		bike.active = bike.pending = false;
		bike.state = kStateTop;
		bike.generation = 0;
		bike.timeLeft = 0;
		bike.brokenMask = 0;
		setLift(b, 0);
	}
	// FUN_00439710
	for (int t = 0; t < kTypes; t++)
		setWallCount(t, 3);
	// FUN_00439770
	for (int b = 0; b < kBikes; b++)
		for (int s = 0; s < kSlots; s++) {
			removePart(b, s);
			_bikes[b].partKind[s] = kKindNone;
		}
	// FUN_004397d0
	for (int i = 0; i < kBeltItems; i++) {
		deleteAnim(_belt[i].anim);
		_belt[i].generation = 0;
		_belt[i].pos = (float)i / kBeltItems;
	}
	// FUN_00439850
	for (int i = 0; i < kQueueSize; i++)
		_queue[i].used = false;
	_queueWrite = 0;
	debug(1, "Buildabike: game started at difficulty %d (%d bikes done, health %d)", _level, _bikesDone, _health);
}

// FUN_00438c50
void BuildabikeScene::gameOver() {
	uint32 elapsed = g_engine->getGameMillis() - _startTime;
	_vm->setGameRunning(false);
	_running = false;
	_scoreDisplay = (float)_score;
	drawText(_scoreText, Common::String::format("%03d %03d", _score / 1000, _score % 1000), true);
	_healthDisplay = (float)_health;
	updateHealthBar(0);
	anim("lcogwheel")->stop();
	anim("rcogwheel")->stop();
	anim("lever")->stop();
	anim("wheel")->stop();
	// The looping sounds are not stopped by the original (it leaves the
	// scene through the high score module); do it here.
	removeAnim("string");
	removeAnim("machine");
	deleteAnim(_drag);

	Common::String msg = Common::String::format(_vm->getString("buildabike:ENDMESSAGE").c_str(),
	                                            elapsed / 60000, (elapsed / 1000) % 60, _score);
	debug(1, "Buildabike: game over after %u ms, score %d", elapsed, _score);
	// Modal: _running is already false, so the nested frames leave the game
	// alone (onUpdate() and tickBike()'s caller check it).
	_vm->messageBox("interfaceh:GAMEOVER", msg, MessageBox::kButtonOk);
	// FUN_0041f9a0(0, score, 0x49e02c, 0); in tournament mode the score
	// goes to the tournament (the next player's round or the results screen).
	static const int kMedals[4] = { 2500, 5000, 10000, 20000 };
	registerScore(_vm, "", _score, kMedals);

	// Outside a tournament (FUN_00419490) the screen is cleared and the
	// start button comes back.
	if (_vm->tournament()->isActive())
		return;
	for (int i = 0; i < kBeltItems; i++)
		deleteAnim(_belt[i].anim);
	for (int i = 0; i < kQueueSize; i++)
		_queue[i].used = false;
	for (int t = 0; t < kTypes; t++) {
		setWallCount(t, 0);
		clearText(_wallTexts[t]);
	}
	for (int b = 0; b < kBikes; b++) {
		clearParts(b);
		setLift(b, 0);
		clearText(_bikeTexts[b]);
	}
	_score = 0;
	clearText(_scoreText);
	addAnim("start", Anim::kDefaultPos, Anim::kDefaultPos, kZStart);
}

// ---- input ---------------------------------------------------------------

// FUN_00439950 / FUN_004399a0
void BuildabikeScene::onMouseDown(int hotspot, int x, int y) {
	if (hotspot == kHotspotExit) {
		_vm->endGame();
		return;
	}
	if (hotspot == kHotspotHelp) {
		_vm->showHelp();
		return;
	}
	if (!_running) {
		if (hotspot == kHotspotStart) {
			removeAnim("start");
			startGame();
		}
		return;
	}
	// A part left hanging by a failed drop is dropped for good.
	deleteAnim(_drag);
	if (hotspot >= kHotspotSlotFirst && hotspot < kHotspotSlotFirst + kBikes * kSlots) {
		pickFromBike(hotspot, x, y);
		playSound("click");
	} else if (hotspot >= kHotspotBeltFirst && hotspot < kHotspotBeltFirst + kBeltItems) {
		pickFromBelt(hotspot, x, y);
		playSound("click");
	} else if (hotspot >= kHotspotWallFirst && hotspot < kHotspotWallFirst + kTypes) {
		pickFromWall(hotspot, x, y);
		playSound("click");
	}
}

// The dragged bitmap is centred on the mouse (FUN_00412cf0) and follows it
// (the EVENT module's drag handling, FUN_0040c1f0 / 0x109). It has no
// hotspot of its own so the elements below it stay hit testable, like the
// original's exclusion of the dragged element in FUN_0040c260.
void BuildabikeScene::startDrag(int kind, int type, int origin, int data, int x, int y) {
	deleteAnim(_drag);
	_drag = newPart(kind, type, 0);
	_drag->setHitMask(nullptr);
	_drag->add(x, y, kZDrag);
	Common::Rect r = _drag->rect();
	_dragOffset = Common::Point(-r.width() / 2, -r.height() / 2);
	_drag->setPos(x + _dragOffset.x, y + _dragOffset.y);
	_dragKind = kind;
	_dragType = type;
	_dragOrigin = origin;
	_dragData = data;
	debug(1, "Buildabike: dragging %s from hotspot %d", _drag->name(), origin);
}

// FUN_00439a40
void BuildabikeScene::pickFromBike(int hotspot, int x, int y) {
	int bike = (hotspot - kHotspotSlotFirst) / kSlots;
	int slot = (hotspot - kHotspotSlotFirst) % kSlots;
	Bike &b = _bikes[bike];
	if (!b.parts[slot] || !b.active || b.state != kStateTop)
		return;
	int kind = b.partKind[slot], type = b.partType[slot];
	removePart(bike, slot);
	startDrag(kind, type, hotspot, b.generation, x, y);
}

// FUN_00439b20
void BuildabikeScene::pickFromBelt(int hotspot, int x, int y) {
	int i = hotspot - kHotspotBeltFirst;
	if (!_belt[i].anim)
		return;
	deleteAnim(_belt[i].anim);
	startDrag(kKindWall, _belt[i].type, hotspot, _belt[i].generation, x, y);
}

// FUN_00439bc0
void BuildabikeScene::pickFromWall(int hotspot, int x, int y) {
	int type = hotspot - kHotspotWallFirst;
	if (_wallCount[type] == 0)
		return;
	setWallCount(type, _wallCount[type] - 1);
	startDrag(kKindWall, type, hotspot, 0, x, y);
}

void BuildabikeScene::endDrag() {
	deleteAnim(_drag);
}

// FUN_00439c40 / FUN_00439c70: the 0x10b drop event.
void BuildabikeScene::onMouseUp(int hotspot, int x, int y) {
	if (!_running || !_drag)
		return;
	debug(1, "Buildabike: drop %s on hotspot %d at %d,%d", _drag->name(), hotspot, x, y);
	bool ok = false;
	if ((hotspot >= kHotspotPlatformFirst && hotspot < kHotspotPlatformFirst + kBikes) ||
	    (hotspot >= kHotspotSlotFirst && hotspot < kHotspotSlotFirst + kBikes * kSlots))
		ok = dropOnBike(hotspot);
	else if (hotspot == kHotspotMachine)
		ok = dropOnMachine();
	else if (hotspot >= kHotspotWallFirst && hotspot < kHotspotWallFirst + kTypes)
		ok = dropOnWall(hotspot);
	if (ok) {
		playSound("drop1");
		return;
	}
	// Back to where it came from.
	if (_dragOrigin >= kHotspotSlotFirst && _dragOrigin < kHotspotSlotFirst + kBikes * kSlots)
		returnToBike();
	else if (_dragOrigin >= kHotspotBeltFirst && _dragOrigin < kHotspotBeltFirst + kBeltItems)
		returnToBelt();
	else if (_dragOrigin >= kHotspotWallFirst && _dragOrigin < kHotspotWallFirst + kTypes)
		returnToWall();
	playSound("drop2");
}

// FUN_00439d70: a part goes into its slot of the bike under the mouse. A
// bike with no broken part left is finished: time left counts as points and
// the platform goes down.
bool BuildabikeScene::dropOnBike(int hotspot) {
	int slot = slotForType(_dragType);
	int bike = hotspot >= kHotspotSlotFirst ? (hotspot - kHotspotSlotFirst) / kSlots : hotspot - kHotspotPlatformFirst;
	Bike &b = _bikes[bike];
	if (!b.active || b.state != kStateTop || b.parts[slot])
		return false;
	int kind = _dragKind == kKindBroken ? kKindBroken : kKindFixed;
	endDrag();
	placePart(bike, slot, kind, _dragType);
	if (!bikeComplete(bike))
		return true;
	_score += (int)(b.timeLeft * kTimeBonus);
	debug(1, "Buildabike: bike %d finished with %.1f s left, score %d", bike, b.timeLeft, _score);
	setBikeState(bike, kStateDown);
	return true;
}

// FUN_00439f40
bool BuildabikeScene::dropOnMachine() {
	if (!queuePart(_dragType))
		return false;
	endDrag();
	playAnim("lever");
	return true;
}

// FUN_0043a050: only good parts go back on the wall, at their own place.
bool BuildabikeScene::dropOnWall(int hotspot) {
	int type = hotspot - kHotspotWallFirst;
	if (_dragKind == kKindBroken || type != _dragType)
		return false;
	endDrag();
	setWallCount(type, _wallCount[type] + 1);
	return true;
}

// FUN_0043a0c0: the part is lost when its slot got filled meanwhile or the
// bike has gone (the generation changed).
void BuildabikeScene::returnToBike() {
	int bike = (_dragOrigin - kHotspotSlotFirst) / kSlots;
	int slot = (_dragOrigin - kHotspotSlotFirst) % kSlots;
	Bike &b = _bikes[bike];
	if (b.parts[slot])
		return; // the original leaves the bitmap hanging until the next click
	endDrag();
	if (b.active && b.state == kStateTop && b.generation == _dragData && b.partKind[slot] != kKindNone)
		placePart(bike, slot, b.partKind[slot], b.partType[slot]);
}

// FUN_0043a170
void BuildabikeScene::returnToBelt() {
	int i = _dragOrigin - kHotspotBeltFirst;
	endDrag();
	if (_belt[i].generation == _dragData)
		showBeltItem(i);
}

// FUN_0043a1d0
void BuildabikeScene::returnToWall() {
	endDrag();
	setWallCount(_dragType, _wallCount[_dragType] + 1);
}

// ---- machine and belt ----------------------------------------------------

// FUN_00439f90: the machine is a ring of 22 slots.
bool BuildabikeScene::queuePart(int type) {
	int i = _queueWrite;
	while (_queue[i].used) {
		i = (i + 1) % kQueueSize;
		if (i == _queueWrite)
			return false;
	}
	_queue[i].type = type;
	_queue[i].timer = kRecycleTime;
	_queue[i].used = true;
	_queueWrite = (_queueWrite + 1) % kQueueSize;
	debug(1, "Buildabike: %s into the machine (slot %d)", kPartNames[type], i);
	return true;
}

// FUN_00438bf0: the most recently queued repaired part (the ring is
// scanned backwards from the write index), or -1.
int BuildabikeScene::machineOutput() {
	int i = _queueWrite;
	while (true) {
		i--;
		if (i < 0)
			i = kQueueSize - 1;
		if (_queue[i].used && _queue[i].timer <= 0)
			break;
		if (i == _queueWrite)
			return -1;
	}
	_queue[i].used = false;
	return _queue[i].type;
}

void BuildabikeScene::showBeltItem(int i) {
	BeltItem &it = _belt[i];
	deleteAnim(it.anim);
	it.anim = newPart(kKindLine, it.type, kHotspotBeltFirst + i);
	// The original expands the hit rectangle by 0..10 pixels per type
	// (0x49dbf0); the pixel exact test is used here instead.
	it.anim->add(kBeltX0 + (int)(kBeltLength * it.pos), kBeltY, 6 + i);
	it.anim->play();
}

void BuildabikeScene::removeBeltItem(int i) {
	deleteAnim(_belt[i].anim);
}

// ---- per frame -----------------------------------------------------------

// FUN_00437d20
void BuildabikeScene::onUpdate() {
	if (_drag) {
		Common::Point m = _vm->getEventManager()->getMousePos();
		_drag->setPos(m.x + _dragOffset.x, m.y + _dragOffset.y);
	}
	if (!_running)
		return;
	uint32 now = g_engine->getGameMillis();
	float dt = (float)(now - _lastTick) * 0.001f;
	updateBikes(dt);
	if (!_running)
		return;
	updateBikeTexts();
	updateBelt(dt);
	updateMachine(dt);
	updateScoreText(dt);
	updateHealthBar(dt);
	_lastTick = now;
}

// FUN_00438030: keeps as many bikes in play as the level allows, then runs
// the platforms.
void BuildabikeScene::updateBikes(float dt) {
	int allowed = allowedBikes();
	int shown = 0;
	for (int b = 0; b < kBikes; b++)
		if (_bikes[b].active || _bikes[b].pending)
			shown++;
	if (shown < allowed) {
		int start = _vm->getRandomNumber(kBikes - 1);
		int k = start;
		do {
			Bike &b = _bikes[k];
			if (!b.active && !b.pending) {
				if (b.state == kStateBottom) {
					b.active = true;
					b.generation++;
					spawnParts(k);
					setLift(k, 1.0f);
				} else if (b.state == kStateTop) {
					setBikeState(k, kStateDown);
					b.pending = true;
					b.generation++;
				}
				shown++;
			}
			k = (k + 1) % kBikes;
		} while (k != start && shown < allowed);
	}
	for (int b = 0; b < kBikes; b++)
		if (_bikes[b].active || _bikes[b].state != kStateTop) {
			tickBike(b, dt);
			if (!_running)
				return;
		}
}

// FUN_004382d0: the platform state machine.
void BuildabikeScene::tickBike(int bike, float dt) {
	Bike &b = _bikes[bike];
	switch (b.state) {
	case kStateTop:
		b.timeLeft -= dt;
		if (b.timeLeft <= 0) {
			setLift(bike, -b.timeLeft);
			setBikeState(bike, kStateDown);
			b.timeLeft = 0;
		}
		break;
	case kStateUp: {
		float lift = b.lift - dt / kLiftTime;
		b.lift = lift;
		if (lift <= 0) {
			b.state = kStateTop;
			b.lift = 0;
			b.timeLeft = kLiftTime * lift + bikeTime();
			debug(1, "Buildabike: bike %d up, %.1f s", bike, b.timeLeft);
		}
		setLift(bike, b.lift);
		break;
	}
	case kStateBottom:
		setBikeState(bike, kStateUp);
		setLift(bike, 1.0f);
		break;
	case kStateDown: {
		float lift = b.lift + dt / kLiftTime;
		b.lift = lift;
		if (lift >= 1.0f) {
			if (!b.active) {
				if (b.pending) {
					spawnParts(bike);
					b.active = true;
					b.pending = false;
				}
			} else {
				scoreBike(bike, true);
				clearParts(bike);
				if (_health < 1) {
					gameOver();
					return;
				}
				b.active = false;
				_bikesDone++;
			}
			b.state = kStateBottom;
		}
		setLift(bike, b.lift);
		break;
	}
	default:
		break;
	}
}

// FUN_00437da0 / FUN_00437df0: time left per bike, "mm ss" from a minute up.
void BuildabikeScene::updateBikeTexts() {
	for (int b = 0; b < kBikes; b++) {
		Bike &bike = _bikes[b];
		if (!bike.active || bike.state != kStateTop) {
			clearText(_bikeTexts[b]);
			continue;
		}
		Common::String text;
		if (bike.timeLeft < 60.0f)
			text = Common::String::format("%.2f", bike.timeLeft);
		else {
			int t = (int)bike.timeLeft;
			text = Common::String::format("%02d %02d", t / 60, t % 60);
		}
		drawText(_bikeTexts[b], text, true);
	}
}

// FUN_00438920: eleven evenly spaced belt positions cross the screen in 15
// seconds; each time one re-enters on the left it takes a repaired part out
// of the machine, if there is one. Parts that reach the right edge are lost.
void BuildabikeScene::updateBelt(float dt) {
	bool any = false;
	for (int i = 0; i < kBeltItems; i++) {
		BeltItem &it = _belt[i];
		it.pos += dt / kBeltTime;
		if (it.pos > 1.0f) {
			int type = machineOutput();
			removeBeltItem(i);
			it.pos = fmod(it.pos, 1.0f);
			it.generation++;
			if (type >= 0) {
				it.type = type;
				showBeltItem(i);
				debug(1, "Buildabike: %s on the belt (slot %d)", kPartNames[type], i);
			}
		}
		if (it.anim) {
			it.anim->setPos(kBeltX0 + (int)(kBeltLength * it.pos), kBeltY);
			any = true;
		}
	}
	// The belt (cog wheels and the "string" sound) runs while a part is on
	// it or being dragged off it (the original tests origins 49..58).
	if (any || (_drag && _dragOrigin > kHotspotBeltFirst && _dragOrigin < kHotspotBeltFirst + kBeltItems)) {
		if (!isAnimPlaying("lcogwheel"))
			playAnim("lcogwheel");
		if (!isAnimPlaying("rcogwheel"))
			playAnim("rcogwheel");
		if (!isAnimPlaying("string"))
			playAnim("string");
	} else {
		if (isAnimPlaying("lcogwheel"))
			anim("lcogwheel")->stop();
		if (isAnimPlaying("rcogwheel"))
			anim("rcogwheel")->stop();
		if (isAnimPlaying("string"))
			removeAnim("string");
	}
}

// FUN_00438b40: the machine works while a part is inside.
void BuildabikeScene::updateMachine(float dt) {
	bool any = false;
	for (int i = 0; i < kQueueSize; i++)
		if (_queue[i].used && _queue[i].timer > 0) {
			any = true;
			_queue[i].timer -= dt;
		}
	if (any) {
		if (!isAnimPlaying("wheel"))
			playAnim("wheel");
		if (!isAnimPlaying("machine"))
			playAnim("machine");
	} else {
		if (isAnimPlaying("wheel"))
			anim("wheel")->stop();
		if (isAnimPlaying("machine"))
			removeAnim("machine");
	}
}

// FUN_00437e90: the score display counts up to the score at 250 per second.
// Nothing is drawn while they agree, so the area stays blank until the
// first points, like the original.
void BuildabikeScene::updateScoreText(float dt) {
	float score = (float)_score;
	if (score < _scoreDisplay)
		_scoreDisplay = 0;
	if (_scoreDisplay == score)
		return;
	if (_scoreDisplay < score)
		_scoreDisplay += kScoreSpeed * dt;
	if (score < _scoreDisplay)
		_scoreDisplay = score;
	int v = (int)_scoreDisplay;
	drawText(_scoreText, Common::String::format("%03d %03d", v / 1000, v % 1000), true);
}

// FUN_00437f60 + the PROGRESS module: the bar follows the health at 50 per second.
void BuildabikeScene::updateHealthBar(float dt) {
	float target = (float)_health;
	if (_healthDisplay != target) {
		if (target <= _healthDisplay)
			_healthDisplay = MAX(target, _healthDisplay - kHealthSpeed * dt);
		else
			_healthDisplay = MIN(target, _healthDisplay + kHealthSpeed * dt);
	}
	float value = _healthDisplay <= 0 ? 0 : _healthDisplay / (float)_maxHealth;
	int width = (int)(kHealthBarWidth * CLIP(value, 0.0f, 1.0f));
	if (width == _healthWidth)
		return;
	_healthWidth = width;
	clearText(_healthBar);
	if (_energy && width > 0)
		_healthBar->surface()->blitFrom(*_energy, Common::Rect(0, 0, width, _energy->h), Common::Point(0, 0));
}

} // End of namespace Flaaklypa
