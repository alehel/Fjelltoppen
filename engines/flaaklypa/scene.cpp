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

#include "flaaklypa/cursor.h"
#include "flaaklypa/dialog.h"
#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/music.h"
#include "flaaklypa/resources.h"
#include "flaaklypa/scene.h"

namespace Flaaklypa {

Scene::Scene(FlaaklypaEngine *vm, const SceneDef *def) : _vm(vm), _def(def), _name(def->name),
	_backdrop(nullptr), _mask(nullptr), _cursorTable(def->cursors), _cursorMode(kCursorNormal),
	_lastHotspot(-1), _seqList(nullptr), _factsActive(false), _factsFound(0) {
	_singleList[0] = _singleList[1] = nullptr;
	if (def->anims)
		for (const AnimDef *a = def->anims; a->name; a++)
			_anims.push_back(new Anim(this, a));
}

Scene::~Scene() {
	// FUN_0041df50 (close): the facts found so far go to the profile
	if (_factsActive)
		_vm->setSceneValue(_name, kProfileKeyFacts, _factsFound);
	for (auto *a : _anims)
		delete a;
	for (auto *d : _dynDefs)
		delete d;
	if (_mask) {
		_mask->free();
		delete _mask;
	}
	delete _backdrop;
}

Resources *Scene::resources() const {
	return _vm->_resources;
}

bool Scene::load() {
	_backdrop = resources()->loadBitmap(_name, "backdrop.bmp", _vm->_screen->format);
	if (!_backdrop) {
		warning("Scene %s: no backdrop", _name.c_str());
		return false;
	}
	_mask = resources()->loadMask(_name, "hotspots.bmp");
	return true;
}

// ---- animations ----------------------------------------------------------

Anim *Scene::findAnim(const char *name) {
	for (auto *a : _anims)
		if (!scumm_stricmp(a->name(), name))
			return a;
	return nullptr;
}

Anim *Scene::anim(const char *name) {
	Anim *a = findAnim(name);
	if (!a) {
		// Not in the tables extracted from the executable: define it on the
		// fly as a clip at the origin so the scene logic can carry on.
		warning("Scene %s: animation '%s' is not in the scene tables", _name.c_str(), name);
		a = defineAnim(name, true, true, 0, 0, 0, 0);
	}
	return a;
}

Anim *Scene::defineAnim(const char *name, bool smacker, bool transparent, int hotspot, int x, int y, int z, bool visible, bool loop) {
	Anim *existing = findAnim(name);
	if (existing)
		return existing;
	AnimDef *d = new AnimDef();
	_dynNames.push_back(Common::String(name));
	d->name = _dynNames.back().c_str();
	d->smacker = smacker;
	d->visible = visible;
	d->transparent = transparent;
	d->loop = loop;
	d->hotspot = hotspot;
	d->x = x;
	d->y = y;
	d->group = 0;
	d->zOrder = z;
	d->overlay = 0;
	_dynDefs.push_back(d);
	Anim *a = new Anim(this, d);
	_anims.push_back(a);
	return a;
}

Anim *Scene::defineSurfaceAnim(const char *name, int w, int h, uint32 keyColor, int hotspot) {
	Anim *a = defineAnim(name, false, true, hotspot, 0, 0, 0);
	a->createSurface(w, h, keyColor);
	return a;
}

void Scene::addAnim(const char *name, int x, int y, int z) {
	anim(name)->add(x, y, z);
}

void Scene::removeAnim(const char *name) {
	anim(name)->remove();
}

void Scene::playAnim(const char *name) {
	anim(name)->play();
}

bool Scene::isAnimAdded(const char *name) {
	return anim(name)->isAdded();
}

bool Scene::isAnimPlaying(const char *name) {
	return anim(name)->isPlaying();
}

void Scene::sortAnims() {
	// Stable insertion sort by z order.
	for (uint i = 1; i < _active.size(); i++) {
		Anim *a = _active[i];
		int j = i - 1;
		while (j >= 0 && _active[j]->z() > a->z()) {
			_active[j + 1] = _active[j];
			j--;
		}
		_active[j + 1] = a;
	}
}

void Scene::animAdded(Anim *anim) {
	bool found = false;
	for (auto *a : _active)
		if (a == anim)
			found = true;
	if (!found)
		_active.push_back(anim);
	sortAnims();
}

void Scene::animRemoved(Anim *anim) {
	for (uint i = 0; i < _active.size(); i++)
		if (_active[i] == anim) {
			_active.remove_at(i);
			break;
		}
	for (uint i = 0; i < _finished.size(); i++)
		if (_finished[i] == anim) {
			_finished.remove_at(i);
			break;
		}
}

void Scene::animStarted(Anim *anim) {
	onAnimStarted(anim);
}

// ---- anim queues ---------------------------------------------------------

void Scene::AnimQueue::set(AnimList l) {
	list = l;
	count = 0;
	pos = 0;
	if (list)
		while (list[count])
			count++;
}

const char *Scene::AnimQueue::previous() const {
	if (count < 1)
		return nullptr;
	return list[pos == 0 ? count - 1 : pos - 1];
}

bool Scene::AnimQueue::advance() {
	if (count == 0)
		return true;
	pos = (pos + 1) % count;
	return pos == 0;
}

// ---- characters ----------------------------------------------------------

void Scene::addCharacter(int id, int hotspot) {
	// FUN_0040a2b0 leaves an existing character alone.
	if (findCharacter(id))
		return;
	Character c;
	c.id = id;
	c.hotspot = hotspot;
	_characters.push_back(c);
}

Scene::Character *Scene::findCharacter(int id) {
	for (auto &c : _characters)
		if (c.id == id)
			return &c;
	return nullptr;
}

void Scene::setCharacterList(int id, int which, AnimList list) {
	Character *c = findCharacter(id);
	if (!c || which < kListIdle || which > kListOwn)
		return;
	c->lists[which].set(list);
	for (int i = 0; i < c->lists[which].count; i++) {
		Anim *a = anim(c->lists[which].list[i]);
		a->setHotspot(c->hotspot);
		a->setGroup(a->group() | c->id);
	}
}

void Scene::setCharacterState(int id, int state) {
	Character *c = findCharacter(id);
	if (!c || state < kListIdle || state > kListSequence)
		return;
	c->state = state;
	if (state == kListSequence) {
		characterPlayNext(*c, kListSequence);
		return;
	}
	// FUN_0040a7c0: start the next clip of the list unless the group is busy
	const char *name = c->lists[state].current();
	if (!name)
		return;
	Anim *a = anim(name);
	if (groupBusy(a->group()))
		return;
	if (state == kListReaction)
		setCursorMode(kCursorHidden);
	takeAnim(a, state);
	c->lists[state].advance();
}

void Scene::playCharacterList(int id, AnimList list) {
	Character *c = findCharacter(id);
	if (!c)
		return;
	setCharacterList(id, kListOwn, list);
	if (!c->lists[kListOwn].count)
		return;
	// FUN_0040a4e0: every character of the clip's group switches to state 4
	Anim *a = anim(c->lists[kListOwn].current());
	setGroupState(a->group(), kListOwn);
	if (groupBusy(a->group()))
		return;
	takeAnim(a, kListOwn);
	c->lists[kListOwn].advance();
}

void Scene::stopCharacter(int id) {
	// FUN_0040a310 removes the clip and clears the slot's in-use flag: the
	// character no longer exists (resetCharacters() skips it, a new
	// addCharacter() creates it afresh).
	for (uint i = 0; i < _characters.size(); i++) {
		if (_characters[i].id != id)
			continue;
		Anim *cur = _characters[i].current;
		if (cur && cur->isAdded())
			cur->remove();
		_characters.remove_at(i);
		return;
	}
}

bool Scene::isCharacterActive(int id) {
	Character *c = findCharacter(id);
	return c && c->current && c->current->isAdded();
}

void Scene::setCharacterZ(int id, int z) {
	for (auto &c : _characters)
		if (c.id == id)
			c.z = z;
}

void Scene::setCharacterAnims(int id, AnimList idle, AnimList bored, AnimList reaction) {
	for (auto &c : _characters) {
		if (c.id != id)
			continue;
		c.lists[kListIdle].set(idle);
		c.lists[kListBored].set(bored);
		c.lists[kListReaction].set(reaction);
		for (int l = 1; l <= 3; l++) {
			for (int i = 0; i < c.lists[l].count; i++) {
				Anim *a = anim(c.lists[l].list[i]);
				a->setHotspot(c.hotspot);
				a->setGroup(a->group() | c.id);
			}
		}
	}
}

bool Scene::groupBusy(uint32 group) const {
	for (const auto &c : _characters)
		if ((c.id & group) && c.current && c.current->isPlaying())
			return true;
	return false;
}

void Scene::setGroupState(uint32 group, int state) {
	for (auto &c : _characters)
		if (c.id & group)
			c.state = state;
}

void Scene::takeAnim(Anim *a, int mode) {
	// Every character shown by the clip drops its current clip.
	int maxZ = INT_MIN;
	for (auto &c : _characters) {
		if (!(c.id & a->group()))
			continue;
		if (c.current && c.current != a && c.current->isAdded())
			c.current->remove();
		c.current = a;
		c.mode = mode;
		c.state = kListIdle;
		maxZ = MAX(maxZ, c.z);
	}
	a->play();
	if (a->def()->zOrder == 0 && maxZ != INT_MIN)
		a->setZ(maxZ);
}

void Scene::scheduleBored(Character &c) {
	c.boredTime = g_engine->getGameMillis() + 15000 + _vm->getRandomNumber(9999);
}

void Scene::characterPlayNext(Character &c, int list) {
	if (list == kListSequence) {
		// The sequence waits for this character; play its next clip.
		const char *name = _seqList ? _seq.current() : nullptr;
		if (!name)
			return;
		Anim *a = anim(name);
		if (groupBusy(a->group()))
			return;
		setCursorMode(kCursorHidden);
		takeAnim(a, kListSequence);
		_seq.advance();
		return;
	}
	if (list < kListIdle || list > kListOwn)
		return;
	const char *name = c.lists[list].current();
	if (!name)
		return;
	Anim *a = anim(name);
	if (groupBusy(a->group()))
		return;
	if (list == kListReaction)
		setCursorMode(kCursorHidden);
	takeAnim(a, list);
	c.lists[list].advance();
}

void Scene::characterClicked(int hotspot) {
	for (auto &c : _characters) {
		if (c.hotspot != hotspot || c.state != kListIdle)
			continue;
		const char *name = c.lists[kListReaction].current();
		if (!name)
			continue;
		// The reaction starts once the characters' current clips end.
		setGroupState(anim(name)->group(), kListReaction);
		setCursorMode(kCursorWait);
	}
}

void Scene::characterAnimFinished(Character &c, Anim *a) {
	if (c.current != a)
		return;
	switch (c.mode) {
	case kListReaction:
		setCursorMode(kCursorNormal);
		// fall through
	case kListBored:
	case kListOwn:
	case kListSequence:
		scheduleBored(c);
		break;
	default:
		break;
	}
	if (c.state == kListIdle && c.lists[kListBored].count && g_engine->getGameMillis() >= c.boredTime)
		c.state = kListBored;
	characterPlayNext(c, c.state);
}

void Scene::resetCharacters() {
	for (auto &c : _characters) {
		c.state = kListIdle;
		if (c.current && c.current->isPlaying())
			c.current->stop();
		characterPlayNext(c, kListIdle);
	}
	sequenceClear();
	setCursorMode(kCursorNormal);
}

// ---- sequences -----------------------------------------------------------

void Scene::sequenceClear() {
	_seqList = nullptr;
	_seq.set(nullptr);
}

void Scene::playSingle(const char *name) {
	_singleList[0] = name;
	_singleList[1] = nullptr;
	playSequence(_singleList);
}

void Scene::playSequence(AnimList list) {
	_seqList = list;
	_seq.set(list);
	if (!_seq.count) {
		_seqList = nullptr;
		return;
	}
	Anim *a = anim(_seq.current());
	setGroupState(a->group(), kListSequence);
	if (groupBusy(a->group())) {
		setCursorMode(kCursorWait);
		return;
	}
	takeAnim(a, kListSequence);
	_seq.advance();
	setCursorMode(kCursorHidden);
}

void Scene::sequenceAnimFinished(Anim *a) {
	if (!_seqList)
		return;
	const char *prev = _seq.previous();
	if (!prev || a != findAnim(prev))
		return;
	if (a->group() == 0 && a->isAdded())
		a->remove();
	// _seq.pos is the next clip; pos 0 means the previous one was the last.
	if (_seq.pos == 0) {
		sequenceClear();
		setCursorMode(kCursorNormal);
		onSequenceDone();
		return;
	}
	Anim *next = anim(_seq.current());
	if (next->isPlaying())
		return;
	setGroupState(next->group(), kListSequence);
	if (groupBusy(next->group()))
		return;
	_seq.advance();
	takeAnim(next, kListSequence);
}

void Scene::stopSequence() {
	if (_seqList) {
		for (int i = 0; i < _seq.count; i++) {
			Anim *a = anim(_seq.list[i]);
			if (a->isPlaying())
				a->remove();
		}
		sequenceClear();
	}
	resetCharacters();
}

// ---- cursor --------------------------------------------------------------

void Scene::setCursorTable(const CursorEntry *table) {
	_cursorTable = table;
	_lastHotspot = -1;
	updateCursor();
}

void Scene::setCursorMode(CursorMode mode) {
	_cursorMode = mode;
	updateCursor();
}

void Scene::showWaitCursor() {
	setCursorMode(kCursorWait);
}

void Scene::hideCursor() {
	setCursorMode(kCursorHidden);
}

void Scene::showCursor() {
	setCursorMode(kCursorNormal);
}

void Scene::updateCursor() {
	Common::String name;
	switch (_cursorMode) {
	case kCursorHidden:
		name = "";
		break;
	case kCursorWait:
		name = "wait";
		break;
	case kCursorNormal: {
		Common::Point p = _vm->getEventManager()->getMousePos();
		int hotspot = hotspotAt(p.x, p.y);
		_lastHotspot = hotspot;
		name = "default";
		if (_cursorTable) {
			for (const CursorEntry *e = _cursorTable; e->hotspot >= 0 && e->cursor; e++)
				if (e->hotspot == hotspot) {
					name = e->cursor;
					break;
				}
		}
		break;
	}
	}
	_vm->_cursor->set(name);
}

// ---- hotspots ------------------------------------------------------------

int Scene::hotspotAt(int x, int y) const {
	// Animations first, top most first. Elements without a hotspot do not
	// hide the mask below them (the original skips those too).
	for (int i = (int)_active.size() - 1; i >= 0; i--) {
		int h = _active[i]->hitTest(x, y);
		if (h > 0)
			return h;
	}
	if (_mask && x >= 0 && y >= 0 && x < _mask->w && y < _mask->h)
		return *(const byte *)_mask->getBasePtr(x, y);
	return 0;
}

// ---- timers --------------------------------------------------------------

void Scene::setTimer(int id, uint32 delayMs, int data) {
	killTimer(id);
	Timer t;
	t.id = id;
	t.data = data;
	t.fireTime = g_engine->getGameMillis() + delayMs;
	_timers.push_back(t);
}

void Scene::killTimer(int id) {
	for (Common::List<Timer>::iterator it = _timers.begin(); it != _timers.end();) {
		if (it->id == id)
			it = _timers.erase(it);
		else
			++it;
	}
}

// ---- misc ----------------------------------------------------------------

void Scene::playMusic(const char *name) {
	_vm->_music->play(name);
}

// ---- main loop -----------------------------------------------------------

void Scene::handleEvent(const Common::Event &event) {
	switch (event.type) {
	case Common::EVENT_MOUSEMOVE: {
		if (_cursorMode != kCursorNormal)
			break;
		int h = hotspotAt(event.mouse.x, event.mouse.y);
		if (h != _lastHotspot) {
			updateCursor();
			onMouseMove(h, event.mouse.x, event.mouse.y);
		}
		break;
	}
	case Common::EVENT_LBUTTONDOWN: {
		if (_cursorMode != kCursorNormal)
			break;
		int h = hotspotAt(event.mouse.x, event.mouse.y);
		debug(1, "Scene %s: click on hotspot %d at %d,%d", _name.c_str(), h, event.mouse.x, event.mouse.y);
		characterClicked(h);
		onMouseDown(h, event.mouse.x, event.mouse.y);
		// The fact module's handler runs after the scene's (priority -10)
		if (_factsActive)
			factClicked(h);
		break;
	}
	case Common::EVENT_LBUTTONUP: {
		if (_cursorMode != kCursorNormal)
			break;
		int h = hotspotAt(event.mouse.x, event.mouse.y);
		onMouseUp(h, event.mouse.x, event.mouse.y);
		break;
	}
	case Common::EVENT_RBUTTONDOWN:
		if (_cursorMode != kCursorNormal)
			break;
		onRightClick(event.mouse.x, event.mouse.y);
		break;
	case Common::EVENT_KEYDOWN:
		if (event.kbd.keycode == Common::KEYCODE_SPACE && !handlesKey(event.kbd))
			stopSequence();
		onKey(event.kbd);
		// The global keys (FUN_0040d300) run after the scene's handler,
		// unless it consumed the key.
		if (!handlesKey(event.kbd))
			_vm->globalKey(event.kbd);
		break;
	case Common::EVENT_KEYUP:
		onKeyUp(event.kbd);
		break;
	default:
		break;
	}
}

void Scene::pauseAnims(bool pause) {
	for (auto *a : _active)
		a->pause(pause);
}

void Scene::dispatchAnimFinished(Anim *a) {
	if (a->removeWhenDone()) {
		a->remove();
		return;
	}
	// Order as in the original: scene callback, sequence player, characters.
	onAnimFinished(a);
	sequenceAnimFinished(a);
	for (auto &c : _characters)
		characterAnimFinished(c, a);
}

int Scene::pickIdleAnim(AnimList list, int count, int randomRange) {
	for (int i = 0; i < 100; i++) {
		int n = _vm->getRandomNumber(randomRange - 1);
		if (!isAnimPlaying(list[n]))
			return n;
	}
	for (int i = 0; i < count; i++)
		if (!isAnimPlaying(list[i]))
			return i;
	return -1;
}

// ---- fact pages ------------------------------------------------------------

void Scene::initFacts() {
	// FUN_0041dbf0
	_facts.clear();
	Common::INIFile ini;
	if (resources()->loadIni(_name, "fact.ini", ini)) {
		for (const auto &section : ini.getSections()) {
			Fact f;
			f.title = section.name;
			const Common::INIFile::KeyValue *kv = section.getKey("hotspot");
			f.hotspot = kv ? atoi(Resources::iniValue(kv->value).c_str()) : -1;
			// FUN_0041dde0: text0, text1, ... up to the first missing key,
			// separated by an empty line
			for (int i = 0; (kv = section.getKey(Common::String::format("text%d", i))) != nullptr; i++) {
				if (i)
					f.text += "\n\n";
				f.text += Resources::iniValue(kv->value);
			}
			_facts.push_back(f);
		}
	}
	_factsFound = _vm->getSceneValue(_name, kProfileKeyFacts);
	_factsActive = true;
	debug(1, "Scene %s: %d fact pages, found mask 0x%x", _name.c_str(), _facts.size(), _factsFound);
}

void Scene::factClicked(int hotspot) {
	// FUN_0041dcb0 / FUN_0041dd40: the first section with that hotspot
	for (uint i = 0; i < _facts.size(); i++) {
		if (_facts[i].hotspot != hotspot)
			continue;
		_vm->messageBox(_facts[i].title, _facts[i].text, MessageBox::kButtonOk);
		// FUN_0041dd20 / FUN_0041dd30: one bit per section; finding the last
		// one is rewarded once
		const uint32 all = (1u << (_facts.size() & 0x1f)) - 1;
		if (_factsFound != all) {
			_factsFound |= 1u << (i & 0x1f);
			if (_factsFound == all) {
				// TODO: the award dialog (FUN_0041c120: awarddlg:TITLE_FACT /
				// DESC_FACT, FUN_0041bb20 with a car part)
				debug(1, "Scene %s: all fact pages found (TODO: award dialog)", _name.c_str());
			}
		}
		return;
	}
}

void Scene::update() {
	uint32 now = g_engine->getGameMillis();

	// Take the due timers out first: a callback may open a modal dialog,
	// which runs nested frames that update this scene again.
	Common::Array<Timer> due;
	for (Common::List<Timer>::iterator it = _timers.begin(); it != _timers.end();) {
		if (now >= it->fireTime) {
			due.push_back(*it);
			it = _timers.erase(it);
		} else {
			++it;
		}
	}
	for (const auto &t : due)
		onTimer(t.id, t.data);

	// Iterate over a copy: callbacks add and remove elements.
	Common::Array<Anim *> active = _active;
	for (auto *a : active)
		if (a->isAdded() && a->update())
			_finished.push_back(a);
	while (!_finished.empty()) {
		Anim *a = _finished[0];
		_finished.remove_at(0);
		dispatchAnimFinished(a);
	}

	onUpdate();
	_vm->_cursor->update();
}

void Scene::draw(Graphics::ManagedSurface &dst) {
	if (_backdrop)
		dst.blitFrom(*_backdrop);
	for (auto *a : _active)
		a->draw(dst);
}

} // End of namespace Flaaklypa
