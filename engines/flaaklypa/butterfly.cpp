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
#include "common/util.h"
#include "common/system.h"
#include "common/textconsole.h"

#include "flaaklypa/award.h"
#include "flaaklypa/butterfly.h"
#include "flaaklypa/dialog.h"
#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/resources.h"

namespace Flaaklypa {

// The species table at 0x49e078: class, clip prefix, points, spawn weight,
// then for the normal and the scared mode: ms between turns, speed in
// pixels per second and turn angle (min, max each).
const ButterflyScene::SpeciesDef ButterflyScene::kSpecies[kSpeciesCount] = {
	{ 0, "largewhite",   4,  10, { { 1000, 1500, 100, 150,  0, 25 }, {  750, 1000, 100, 200, 10, 45 } } },
	{ 0, "smallwhite",   4,  10, { {  750, 1000, 150, 200, 10, 45 }, {  500, 1000, 150, 250, 20, 75 } } },
	{ 0, "redadmiral",   4,  10, { {  200,  400, 150, 175,  5, 10 }, {  500,  500, 160, 200,  5, 10 } } },
	{ 0, "peacock",      4,  10, { { 1200, 1400, 120, 130, 30, 45 }, {  600,  800, 150, 160, 10, 20 } } },
	{ 0, "tortoishell",  4,  10, { { 1000, 1200, 180, 210, 20, 40 }, {  500,  700, 100, 110,  0, 10 } } },
	{ 1, "painted",      6,   7, { { 1200, 1400, 160, 180, 40, 55 }, { 1500, 1700, 190, 200, 45, 60 } } },
	{ 1, "monarch",      8,   7, { {  800, 1000, 200, 220,  5, 25 }, {  350,  500, 200, 230, 30, 45 } } },
	{ 1, "bluemorph",   10,   7, { {  250,  400, 200, 220, 20, 40 }, {  300,  500, 250, 260, 50, 75 } } },
	{ 1, "heliconius",  12,   7, { {  200, 1500, 100, 300,  0, 70 }, {  500,  600, 240, 260, 30, 35 } } },
	{ 1, "leaf",        14,   6, { {  800, 1000, 160, 180, 25, 45 }, {  200,  300, 250, 260, 10, 15 } } },
	{ 1, "owl",         16,   6, { {  900, 1300, 150, 175, 30, 35 }, { 1500, 1750, 230, 250, 40, 45 } } },
	{ 2, "clearwing",   30,   3, { {  250,  500, 125, 175, 30, 55 }, { 1000, 1500, 280, 340, 10, 20 } } },
	{ 2, "largeblue",   40,   3, { { 1200, 1600, 180, 300,  0, 30 }, {  500,  800, 240, 260, 65, 75 } } },
	{ 2, "african",     50,   2, { { 1000, 1500, 180, 220, 35, 50 }, { 2000, 2000, 270, 320, 50, 80 } } },
	{ 2, "queen",      100,   2, { { 2000, 2500,  75, 100,  0, 10 }, { 2500, 3000, 360, 380,  0,  0 } } }
};

// The insects at 0x49e438. Catching one costs points.
const ButterflyScene::SpeciesDef ButterflyScene::kEnemies[kEnemyCount] = {
	{ 3, "wasp", -10, 50, { {  100,  550, 120, 250, 20, 75 }, {  500,  750,  80, 100, 30, 75 } } },
	{ 3, "bee",  -20, 50, { {  600, 1200,  75, 100, 10, 35 }, {  600,  800,  75,  75, 30, 45 } } }
};

// Level table at 0x49e4b8: time limit in ms, points needed, speed factor.
const int ButterflyScene::kLevelTable[kLevels][2] = {
	{ 120000, 250 }, { 110000, 500 }, { 100000, 750 }, { 90000, 1000 }, { 95000, 1500 }, { 110000, 2000 },
	{ 125000, 2500 }, { 140000, 3250 }, { 155000, 4000 }, { 170000, 5000 }, { 190000, 7500 }
};
const float ButterflyScene::kLevelSpeed[kLevels] = { 0.7f, 0.75f, 0.8f, 0.85f, 0.9f, 0.95f, 1.0f, 1.05f, 1.1f, 1.15f, 1.2f };

// The meadow (0x49e550); an entity whose 32x32 rectangle leaves it is removed.
enum { kAreaLeft = 24, kAreaTop = 88, kAreaRight = 702, kAreaBottom = 532 };


static const int kTimerX = 728;
static const int kTimerBottom = 425;        ///< timermove at the start of a level
static const int kTimerRange = 322;         ///< ... and this much higher when the time is up (0x49e5e4)
static const int kCatcherOffset = 35;       ///< the net clip is drawn at the mouse minus this

ButterflyScene::ButterflyScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def),
	_inGame(false), _levelPoints(0), _score(0), _level(0), _levelStart(0), _tick(0), _lastTick(0), _highlighted(0) {
	memset(_butterflies, 0, sizeof(_butterflies));
	memset(_enemies, 0, sizeof(_enemies));
	memset(_caught, 0, sizeof(_caught));

	// One animation definition per clip: "<species><angle>.smk". The
	// original sprintf's the name into the entity's animation struct.
	int n = 0;
	for (int s = 0; s < kSpeciesCount + kEnemyCount; s++) {
		const SpeciesDef &sd = s < kSpeciesCount ? kSpecies[s] : kEnemies[s - kSpeciesCount];
		int clips = s < kSpeciesCount ? kClipsPerSpecies : kClipsPerEnemy;
		for (int c = 0; c < clips; c++, n++) {
			_clipNames[n] = Common::String::format("%s%d", sd.name, c * (360 / clips));
			AnimDef &d = _clipDefs[n];
			d.name = _clipNames[n].c_str();
			d.smacker = 1;
			d.visible = 1;
			d.transparent = 1;
			d.loop = 1;
			d.hotspot = 0;
			d.x = d.y = 0;
			d.group = 0;
			d.zOrder = 0;
			d.overlay = 0;
		}
	}

	// While the net is the pointer the original shows no cursor at all
	// (cursor mode 3) but keeps delivering mouse events; a table mapping
	// every hotspot to the empty cursor does the same here.
	for (int i = 0; i < 256; i++) {
		_blankCursors[i].hotspot = i;
		_blankCursors[i].cursor = "";
	}
	_blankCursors[256].hotspot = -1;
	_blankCursors[256].cursor = nullptr;
}

ButterflyScene::~ButterflyScene() {
	for (int i = 0; i < kSlots; i++) {
		delete _butterflies[i].anim;
		delete _enemies[i].anim;
	}
}

bool ButterflyScene::isCollected(int species) {
	// FUN_0041b5d0(7, profile, species)
	return species >= 0 && species < kSpeciesCount && awardHasItem(g_engine, kAwardButterfly, species);
}

bool ButterflyScene::load() {
	if (!Scene::load())
		return false;
	_font10.load("Amerigo BT_10_");
	_font18.load("Amerigo BT_18_");
	return true;
}

// ---- helpers --------------------------------------------------------------

// FUN_0040c550: lo + rand() % (hi - lo); FUN_0040c530(0) returns 0.
int ButterflyScene::randRange(int lo, int hi) {
	if (hi - lo <= 0)
		return lo;
	return lo + (int)_vm->getRandomNumber(hi - lo - 1);
}

// FUN_0043ae60: butterfly clip (0..7, every 45 degrees) for a heading.
int ButterflyScene::octantClip(int heading) {
	if (heading < 0)
		heading += ((359 - heading) / 360) * 360;
	return ((heading + 22) % 360) / 45;
}

// FUN_0043ae10: insect clip (0, 2, 4, 6: every 90 degrees) for a heading.
int ButterflyScene::quadrantClip(int heading) {
	if (heading < 0)
		heading += ((359 - heading) / 360) * 360;
	return (((heading + 45) % 360) / 90) * 2;
}

const ButterflyScene::SpeciesDef &ButterflyScene::speciesOf(const Entity &e) const {
	return e.enemy ? kEnemies[e.species] : kSpecies[e.species];
}

const AnimDef *ButterflyScene::clipDef(const Entity &e, int clip) const {
	if (e.enemy)
		return &_clipDefs[kSpeciesCount * kClipsPerSpecies + e.species * kClipsPerEnemy + clip / 2];
	return &_clipDefs[e.species * kClipsPerSpecies + clip];
}

// ---- screen ---------------------------------------------------------------

void ButterflyScene::onInit(int arg) {
	initScreen();
}

// FUN_0043a4f0, the init handler.
void ButterflyScene::initScreen() {
	const uint32 green = _vm->_screen->format.RGBToColor(0, 255, 0);

	// Labels (Amerigo BT 10, centred in their rectangles at 0x49e560..) and
	// the three number areas (Amerigo BT 18), all at z 110.
	defineSurfaceAnim("commontext", 267, 16, green)->add(190, 63, kZText);
	drawLabel("commontext", _vm->getString("butterfly:COMMON"));
	defineSurfaceAnim("raretext", 103, 16, green)->add(598, 63, kZText);
	drawLabel("raretext", _vm->getString("butterfly:RARE"));
	defineSurfaceAnim("leveltext", 50, 17, green)->add(616, 577, kZText);
	drawLabel("leveltext", _vm->getString("butterfly:LEVEL"));
	defineSurfaceAnim("nexttext", 60, 17, green)->add(553, 577, kZText);
	drawLabel("nexttext", _vm->getString("butterfly:NEXTLEVEL"));
	defineSurfaceAnim("scorenum", 178, 30, green)->add(268, 554, kZText);
	defineSurfaceAnim("nextnum", 60, 32, green)->add(553, 545, kZText);
	defineSurfaceAnim("levelnum", 50, 32, green)->add(616, 545, kZText);

	resetEntities();
	for (int i = 0; i < kSpeciesCount; i++)
		defineAnim(kSpecies[i].name, false, true, 0, 0, 0, kZText);
	for (int i = 0; i < 4; i++)
		defineAnim(Common::String::format("level_%d", i).c_str(), false, false, 0, 20, 82, kZLevel);
	defineAnim("catcher", true, true, 0, 0, 0, kZCatcher);

	// Buttons (BUTTON module, z 150): help at (0, 0), exit at (728, 0) and
	// the album at (24, 541). No album in tournament mode.
	defineAnim("help_0", false, true, kHotspotHelp, 0, 0, kZButton)->add(0, 0, kZButton);
	defineAnim("help_1", false, true, kHotspotHelp, 0, 0, kZButton);
	defineAnim("exit_0", false, true, kHotspotExit, 728, 0, kZButton)->add(728, 0, kZButton);
	defineAnim("exit_1", false, true, kHotspotExit, 728, 0, kZButton);
	defineAnim("album_0", false, true, kHotspotAlbum, 24, 541, kZButton)->add(24, 541, kZButton);
	defineAnim("album_1", false, true, kHotspotAlbum, 24, 541, kZButton);
	_highlighted = 0;

	static const char *const borders[] = {
		"borderleft", "borderright", "bordertop", "borderbottom",
		"bordertopleft", "bordertopright", "borderbottomleft", "borderbottomright"
	};
	for (uint i = 0; i < ARRAYSIZE(borders); i++)
		addAnim(borders[i], Anim::kDefaultPos, Anim::kDefaultPos, kZBorder);
	addAnim("timerstart", Anim::kDefaultPos, Anim::kDefaultPos, 0);
	addAnim("startanim", Anim::kDefaultPos, Anim::kDefaultPos, kZButton);
	playAnim("startanim");
	playMusic("subgame5");
	_inGame = false;
}

// FUN_0043a8c0: the close handler frees the fonts and text areas.
void ButterflyScene::onClose() {
	removeAll(_butterflies);
	removeAll(_enemies);
}

void ButterflyScene::drawLabel(const char *area, const Common::String &text) {
	Graphics::ManagedSurface *s = anim(area)->surface();
	s->fillRect(Common::Rect(0, 0, s->w, s->h), s->format.RGBToColor(0, 255, 0));
	_font10.drawStringCentred(*s, Common::Rect(0, 0, s->w, s->h), text);
}

void ButterflyScene::drawNumber(const char *area, int value) {
	Graphics::ManagedSurface *s = anim(area)->surface();
	s->fillRect(Common::Rect(0, 0, s->w, s->h), s->format.RGBToColor(0, 255, 0));
	_font18.drawStringCentred(*s, Common::Rect(0, 0, s->w, s->h), Common::String::format("%d", value));
}

void ButterflyScene::highlightButton(int hotspot) {
	if (hotspot != kHotspotHelp && hotspot != kHotspotExit && hotspot != kHotspotAlbum)
		hotspot = 0;
	if (_inGame && hotspot == kHotspotAlbum)
		hotspot = 0; // disabled while playing (button state 2)
	if (hotspot == _highlighted)
		return;
	static const struct { int hotspot; const char *normal, *lit; int x, y; } buttons[] = {
		{ kHotspotHelp, "help_0", "help_1", 0, 0 },
		{ kHotspotExit, "exit_0", "exit_1", 728, 0 },
		{ kHotspotAlbum, "album_0", "album_1", 24, 541 }
	};
	for (uint i = 0; i < ARRAYSIZE(buttons); i++) {
		if (buttons[i].hotspot == _highlighted) {
			removeAnim(buttons[i].lit);
			addAnim(buttons[i].normal, buttons[i].x, buttons[i].y, kZButton);
		}
		if (buttons[i].hotspot == hotspot) {
			removeAnim(buttons[i].normal);
			addAnim(buttons[i].lit, buttons[i].x, buttons[i].y, kZButton);
		}
	}
	_highlighted = hotspot;
}

void ButterflyScene::onMouseMove(int hotspot, int x, int y) {
	highlightButton(hotspot);
}


// FUN_0043a820: the start clip was clicked.
void ButterflyScene::startGame() {
	debug(1, "Butterfly: start clicked at %u ms", g_engine->getGameMillis());
	removeAnim("startanim");
	playAnim("timerstart");
	showCatcher(true);
}

// FUN_0043a840: the net replaces the cursor while the game runs.
void ButterflyScene::showCatcher(bool show) {
	if (show) {
		Common::Point m = _vm->getEventManager()->getMousePos();
		setCursorTable(_blankCursors);
		anim("catcher")->add(m.x - kCatcherOffset, m.y - kCatcherOffset, kZCatcher);
		_lastMouse = m;
	} else {
		if (isAnimAdded("catcher"))
			removeAnim("catcher");
		setCursorTable(_def->cursors);
	}
}

// FUN_0043b220: enters a level: new background, texts, timer restart.
void ButterflyScene::setLevel(int level) {
	_levelPoints = 0;
	_level = level;
	for (int i = 0; i < 4; i++) {
		Common::String name = Common::String::format("level_%d", i);
		if (isAnimAdded(name.c_str()))
			removeAnim(name.c_str());
	}
	addAnim(Common::String::format("level_%d", level % 4).c_str(), Anim::kDefaultPos, Anim::kDefaultPos, kZLevel);
	drawNumber("levelnum", level + 1);
	drawNumber("nextnum", kLevelTable[level][1] - _levelPoints + _score);
	drawNumber("scorenum", _score);
	restartTimer();
}

// FUN_0043b320
void ButterflyScene::restartTimer() {
	if (isAnimAdded("timermove"))
		removeAnim("timermove");
	addAnim("timermove", Anim::kDefaultPos, Anim::kDefaultPos, 0);
	playAnim("timermove");
	_levelStart = g_engine->getGameMillis();
	_inGame = true;
}

// FUN_0043b920: moves the caterpillar up; false once the level time is over.
bool ButterflyScene::updateTimer(uint32 now) {
	int elapsed = (int)(now - _levelStart);
	int duration = kLevelTable[_level][0];
	if (duration < elapsed)
		return false;
	anim("timermove")->setPos(kTimerX, kTimerBottom - (int)(elapsed * ((float)kTimerRange / duration)));
	return true;
}

// FUN_0043b8e0
void ButterflyScene::timeUp() {
	if (isAnimAdded("timermove"))
		removeAnim("timermove");
	_inGame = false;
	addAnim("timerend", Anim::kDefaultPos, Anim::kDefaultPos, 0);
	playAnim("timerend");
}

// FUN_0043b990, the "timerend" branch.
void ButterflyScene::gameOver() {
	_vm->setGameRunning(false);
	showCatcher(false);
	debug(1, "Butterfly: game over, %d points, level %d", _score, _level + 1);
	// Modal: timeUp() has stopped the game (_inGame), so the nested frames
	// only move the net; "timerend" has finished and does not fire again.
	_vm->messageBox("interfaceh:GAMEOVER", "butterfly:ENDMESSAGE", MessageBox::kButtonOk);
	removeAll(_butterflies);
	removeAll(_enemies);
	// FUN_0041b740(7, species, 0x10, 0): the species caught go to the
	// profile's butterfly collection, silently (not in tournament mode)
	for (int i = 0; i < kSpeciesCount; i++)
		if (_caught[i] > 0)
			awardGive(_vm, kAwardButterfly, i, kAwardSpecificItem);
	// TODO: high score registration FUN_0041f9a0(0, score, medals {1500, 3000, 5000}).
	for (int i = 0; i < 4; i++) {
		Common::String name = Common::String::format("level_%d", i);
		if (isAnimAdded(name.c_str()))
			removeAnim(name.c_str());
	}
	// Not in the original (its dialogs follow here): show the start clip
	// again so another game can be played.
	removeAnim("timerend");
	addAnim("timerstart", Anim::kDefaultPos, Anim::kDefaultPos, 0);
	addAnim("startanim", Anim::kDefaultPos, Anim::kDefaultPos, kZButton);
	playAnim("startanim");
}

void ButterflyScene::onAnimFinished(Anim *a) {
	const Common::String n(a->name());
	if (n == "timerstart") {
		// FUN_0043b990: the game starts.
		a->remove();
		_vm->setGameRunning(true);
		// TODO: album button disabled (FUN_00406720(4, 2)).
		_levelPoints = _score = _level = 0;
		_levelStart = 0;
		memset(_caught, 0, sizeof(_caught));
		_tick = 0;
		setLevel(0);
		_lastTick = g_engine->getGameMillis();
		debug(1, "Butterfly: game starts at %u ms", _lastTick);
	} else if (n == "timerend") {
		gameOver();
	}
}

// ---- input ----------------------------------------------------------------

// FUN_0043a920
void ButterflyScene::onMouseDown(int hotspot, int x, int y) {
	if (hotspot == kHotspotExit) {
		_vm->endGame();
		return;
	}
	if (hotspot == kHotspotHelp) {
		_vm->showHelp();
		return;
	}
	if (hotspot == kHotspotAlbum) {
		if (!_inGame && !isAnimAdded("timerend"))
			_vm->changeScene("buttercol", 0);
		return;
	}
	if (hotspot == kHotspotStart) {
		startGame();
		return;
	}
	if (_inGame)
		catchAt(x, y);
}

// FUN_0043a950: a click of the net.
void ButterflyScene::catchAt(int x, int y) {
	anim("catcher")->stop();
	playAnim("catcher");

	// Insects first: hitting one costs points and nothing else happens.
	int stung = 0;
	for (int i = 0; i < kSlots; i++) {
		Entity &e = _enemies[i];
		if (e.active && within(e, x, y, kCatchRadius)) {
			addPoints(e);
			stung++;
		}
	}
	if (stung > 0) {
		anim("scream")->stop();
		playAnim("scream");
		drawNumber("scorenum", _score);
		return;
	}

	int caught = 0, scared = 0;
	for (int i = 0; i < kSlots; i++) {
		Entity &e = _butterflies[i];
		if (!e.active)
			continue;
		if (within(e, x, y, kCatchRadius)) {
			addPoints(e);
			_caught[e.species]++;
			debug(1, "Butterfly: caught %s, score %d", speciesOf(e).name, _score);
			removeEntity(e);
			caught++;
		} else if (within(e, x, y, kScareRadius)) {
			setMode(e, 1);
			scared++;
		}
	}
	if (caught > 0) {
		anim("catch")->stop();
		playAnim("catch");
		drawNumber("scorenum", _score);
		if (kLevelTable[_level][1] < _levelPoints) {
			// Time bonus: the remaining fraction of the level time times 100 * (level + 1).
			int duration = kLevelTable[_level][0];
			int remaining = duration + (int)_levelStart - (int)g_engine->getGameMillis();
			_score += (int)((float)remaining / duration * (100 * (_level + 1)));
			debug(1, "Butterfly: level %d done, score %d", _level + 1, _score);
			setLevel(MIN(_level + 1, kLevels - 1)); // FUN_0043b200
		}
	} else if (scared > 0) {
		anim("miss")->stop();
		playAnim("miss");
	}
}

// FUN_0043ab40 / FUN_0043ab80 / FUN_0043abc0: distance from the sprite centre, rounded.
bool ButterflyScene::within(const Entity &e, int x, int y, int radius) const {
	int cx = (int)e.x + kSpriteSize / 2, cy = (int)e.y + kSpriteSize / 2;
	int dx = cx - x, dy = cy - y;
	int dist = (int)(sqrt((double)(dx * dx + dy * dy)) + 0.5);
	return dist <= radius;
}

// FUN_0043b370
void ButterflyScene::addPoints(const Entity &e) {
	_levelPoints += speciesOf(e).points;
	_score += speciesOf(e).points;
}

// ---- entities -------------------------------------------------------------

// FUN_0043a790 / FUN_0043a7f0 / FUN_0043a760
void ButterflyScene::resetEntities() {
	removeAll(_butterflies);
	removeAll(_enemies);
	for (int i = 0; i < kSlots; i++) {
		_butterflies[i].enemy = false;
		_enemies[i].enemy = true;
	}
}

// FUN_0043acd0: new heading; swaps the clip when the direction octant changes.
void ButterflyScene::setClip(Entity &e, int heading) {
	int newClip = e.enemy ? quadrantClip(heading) : octantClip(heading);
	int oldClip = e.enemy ? quadrantClip(e.heading) : octantClip(e.heading);
	float rad = heading * 0.0174533f;
	e.dirX = (float)sin(rad);
	e.dirY = -(float)cos(rad);
	e.heading = heading;
	if (newClip == oldClip && e.anim && e.anim->isAdded())
		return;
	if (e.anim) {
		e.anim->remove();
		delete e.anim;
	}
	e.anim = new Anim(this, clipDef(e, newClip));
	e.anim->add((int)e.x, (int)e.y, e.enemy ? kZEnemy : kZButterfly);
}

// FUN_0043af30: rest = freeze the wings (the clip stops).
void ButterflyScene::setPlaying(Entity &e, bool rest) {
	if (!e.anim)
		return;
	if (!rest) {
		if (!e.anim->isPlaying())
			e.anim->play();
	} else if (e.anim->isPlaying()) {
		e.anim->stop();
	}
	e.resting = rest;
}

// FUN_0043aef0: a calm butterfly heading downwards rests one time in four.
bool ButterflyScene::shouldRest(const Entity &e) {
	if (e.enemy || e.mode == 1)
		return false;
	int q = quadrantClip(e.heading);
	if (q == 0 || q == 2 || q == 6)
		return false;
	return _vm->getRandomNumber(3) == 0;
}

// FUN_0043ac40: turns by a random angle and schedules the next turn.
void ButterflyScene::turn(Entity &e) {
	const int *p = speciesOf(e).params[e.mode];
	int t = randRange(p[4], p[5]);
	if (_vm->getRandomNumber(1) == 0)
		t = -t;
	setClip(e, e.heading + t);
	setPlaying(e, shouldRest(e));
	e.nextChange = g_engine->getGameMillis() + randRange(p[0], p[1]);
}

// FUN_0043abf0: mode 0 normal, 1 scared by a missed net.
void ButterflyScene::setMode(Entity &e, int mode) {
	e.mode = mode;
	const int *p = speciesOf(e).params[mode];
	e.speed = randRange(p[2], p[3]);
	turn(e);
}

// FUN_0043b500 / FUN_0043b550: enters from the left or the right edge.
void ButterflyScene::spawn(Entity &e, bool enemy, int species) {
	e.enemy = enemy;
	e.species = species;
	e.active = true;
	int side = _vm->getRandomNumber(1) ? 2 : 6;
	e.x = (float)(side == 2 ? kAreaLeft - kSpriteSize : kAreaRight);
	e.y = (float)randRange(kAreaTop, kAreaBottom - kSpriteSize);
	e.heading = 0;
	e.resting = false;
	setClip(e, side * 45);
	setMode(e, 0);
	setPlaying(e, false);
}

// FUN_0043af90
void ButterflyScene::removeEntity(Entity &e) {
	if (e.anim) {
		e.anim->remove();
		delete e.anim;
		e.anim = nullptr;
	}
	e.active = false;
	if (!e.enemy)
		updateIcons();
}

void ButterflyScene::removeAll(Entity *list) {
	for (int i = 0; i < kSlots; i++)
		if (list[i].active)
			removeEntity(list[i]);
}

// FUN_0043b650 / FUN_0043b860
int ButterflyScene::freeSlot(const Entity *list) const {
	for (int i = 0; i < kSlots; i++)
		if (!list[i].active)
			return i;
	return -1;
}

// FUN_0043b5d0: weighted random species.
int ButterflyScene::pickSpecies() {
	int total = 0;
	for (int i = 0; i < kSpeciesCount; i++)
		total += kSpecies[i].weight;
	int r = (int)_vm->getRandomNumber(total - 1);
	int sum = 0;
	for (int i = 0; i < kSpeciesCount; i++) {
		sum += kSpecies[i].weight;
		if (r < sum)
			return i;
	}
	return -1;
}

// FUN_0043b4c0
void ButterflyScene::spawnButterfly() {
	int slot = freeSlot(_butterflies);
	if (slot < 0)
		return;
	spawn(_butterflies[slot], false, pickSpecies());
	updateIcons();
}

// FUN_0043b820
void ButterflyScene::spawnEnemy() {
	int slot = freeSlot(_enemies);
	if (slot < 0)
		return;
	spawn(_enemies[slot], true, (int)_vm->getRandomNumber(1));
}

// FUN_0043b6d0 / FUN_0043b790: turns when due, moves, leaves the meadow.
void ButterflyScene::moveEntity(Entity &e, uint32 now, uint32 last) {
	if (e.nextChange <= now)
		turn(e);
	float step = (float)(int)(now - last) * 0.001f * kLevelSpeed[_level];
	e.x += (float)e.speed * e.dirX * step;
	e.y += (float)e.speed * e.dirY * step;
	if (e.anim)
		e.anim->setPos((int)e.x, (int)e.y);
	// FUN_0043ee30: gone once the sprite rectangle no longer touches the area.
	int x0 = (int)e.x, y0 = (int)e.y, x1 = x0 + kSpriteSize, y1 = y0 + kSpriteSize;
	bool touches = kAreaTop <= y1 && kAreaLeft <= x1 && y0 <= kAreaBottom && x0 <= kAreaRight;
	if (!touches)
		removeEntity(e);
}

// FUN_0043afc0: the window at the top shows the species in the air, common
// ones to the left and rare ones to the right, in table order.
void ButterflyScene::updateIcons() {
	for (int i = 0; i < kSpeciesCount; i++)
		if (isAnimAdded(kSpecies[i].name))
			removeAnim(kSpecies[i].name);
	bool present[kSpeciesCount];
	memset(present, 0, sizeof(present));
	for (int i = 0; i < kSlots; i++)
		if (_butterflies[i].active)
			present[_butterflies[i].species] = true;
	int xCommon = 77, xRare = 581;
	for (int i = 0; i < kSpeciesCount; i++) {
		if (!present[i])
			continue;
		int x;
		if (kSpecies[i].cls < 2) {
			x = xCommon;
			xCommon += 52;
		} else {
			x = xRare;
			xRare += 61;
		}
		addAnim(kSpecies[i].name, x, 6, kZText);
	}
}

// ---- per frame ------------------------------------------------------------

// FUN_0043b410, the frame tick. Spawning counts frames like the original
// (one butterfly every 55 frames, one insect every 90; 50 and 81 on the
// last level).
void ButterflyScene::onUpdate() {
	// FUN_0043b3e0: the net follows the mouse.
	Common::Point m = _vm->getEventManager()->getMousePos();
	if (m != _lastMouse) {
		_lastMouse = m;
		if (isAnimAdded("catcher"))
			anim("catcher")->setPos(m.x - kCatcherOffset, m.y - kCatcherOffset);
	}

	if (!_inGame)
		return;
	uint32 now = g_engine->getGameMillis();
	if (!updateTimer(now))
		timeUp();
	_tick++;
	int divisor = _level + 1 < 11 ? 10 : _level + 1;
	if (_tick % (550 / divisor) == 0)
		spawnButterfly();
	if (_tick % (900 / divisor) == 0)
		spawnEnemy();
	for (int i = 0; i < kSlots; i++)
		if (_butterflies[i].active)
			moveEntity(_butterflies[i], now, _lastTick);
	for (int i = 0; i < kSlots; i++)
		if (_enemies[i].active)
			moveEntity(_enemies[i], now, _lastTick);
	_lastTick = now;

	if (_tick % 60 == 0 && gDebugLevel >= 3) {
		for (int i = 0; i < 2 * kSlots; i++) {
			const Entity &e = i < kSlots ? _butterflies[i] : _enemies[i - kSlots];
			if (e.active)
				debug(3, "Butterfly: tick %d (%u ms) %s at %d,%d heading %d speed %d mode %d%s", _tick, now, speciesOf(e).name,
				      (int)e.x, (int)e.y, e.heading, e.speed, e.mode, e.resting ? " resting" : "");
		}
	}
}

// ============================================================================
// The collection screen
// ============================================================================

// The anim list at 0x4d8bd0, indexed by the butterfly species number of the
// profile flag. Note that the first two are swapped relative to the species
// table (largewhite = 0 shows small_white_coll), as in the original.
const char *const ButtercolScene::kCollection[] = {
	"small_white_coll", "large_white_coll", "red_admiiral_coll", "peacock_coll", "small_tortoshell_coll",
	"paint_coll", "monarch_coll", "blue_morphus_coll", "heliconius_coll", "leaf_coll", "owl_coll",
	"clear_wing_coll", "big_blue_coll", "african_coll", "queen_coll"
};

ButtercolScene::ButtercolScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def), _highlighted(0) {
}

// FUN_0043a2a0
void ButtercolScene::onInit(int arg) {
	defineAnim("help_0", false, true, kHotspotHelp, 0, 0, 150)->add(0, 0, 150);
	defineAnim("help_1", false, true, kHotspotHelp, 0, 0, 150);
	defineAnim("exit_0", false, true, kHotspotExit, 728, 0, 150)->add(728, 0, 150);
	defineAnim("exit_1", false, true, kHotspotExit, 728, 0, 150);
	_highlighted = 0;
	placeCollection();
	// The original also registers the "LOVELYFLIES" cheat (FUN_0040ad50)
	// which marks every species as collected.
}

// FUN_0043a340
void ButtercolScene::placeCollection() {
	for (int i = 0; i < ButterflyScene::kSpeciesCount; i++) {
		Anim *a = anim(kCollection[i]);
		a->setHotspot(kHotspotFirst + i);
		if (a->isAdded())
			a->remove();
		if (ButterflyScene::isCollected(i))
			a->add(Anim::kDefaultPos, Anim::kDefaultPos, -10);
	}
}

void ButtercolScene::highlightButton(int hotspot) {
	if (hotspot != kHotspotHelp && hotspot != kHotspotExit)
		hotspot = 0;
	if (hotspot == _highlighted)
		return;
	if (_highlighted == kHotspotHelp) {
		removeAnim("help_1");
		addAnim("help_0", 0, 0, 150);
	} else if (_highlighted == kHotspotExit) {
		removeAnim("exit_1");
		addAnim("exit_0", 728, 0, 150);
	}
	_highlighted = hotspot;
	if (hotspot == kHotspotHelp) {
		removeAnim("help_0");
		addAnim("help_1", 0, 0, 150);
	} else if (hotspot == kHotspotExit) {
		removeAnim("exit_0");
		addAnim("exit_1", 728, 0, 150);
	}
}

void ButtercolScene::onMouseMove(int hotspot, int x, int y) {
	highlightButton(hotspot);
}

// FUN_0043a3b0 and the button handler FUN_00432d10.
void ButtercolScene::onMouseDown(int hotspot, int x, int y) {
	if (hotspot == kHotspotExit) {
		// FUN_0040cc30: back to the parent scene, the game.
		_vm->changeScene("butterfly", 0);
		return;
	}
	if (hotspot == kHotspotHelp) {
		_vm->showHelp();
		return;
	}
	if (hotspot >= kHotspotFirst && hotspot < kHotspotFirst + ButterflyScene::kSpeciesCount) {
		// TODO: the "info" fact page about species hotspot - 20 (FUN_0040cd70("info")
		// with the descriptor at 0x666c48: title "butterfly", list 0x4d8c50).
		debug(1, "Buttercol: info page for species %d not implemented", hotspot - kHotspotFirst);
	}
}


} // End of namespace Flaaklypa
