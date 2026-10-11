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
#include "common/endian.h"
#include "graphics/managed_surface.h"

#include "flaaklypa/award.h"
#include "flaaklypa/bouquet.h"
#include "flaaklypa/dialog.h"
#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/profile.h"
#include "flaaklypa/resources.h"

namespace Flaaklypa {

// Activity "Ludvigs blomsterbinderi" (handler FUN_0045e0f0).

enum {
	kHotspotCarPart = 4,
	kHotspotFirstKind = 100,     ///< the flowers on the shelf, 100..121 in the mask
	kHotspotFirstFlower = 1000,  ///< the flowers in the vase, one element each
	kHotspotVase = 2000,

	kButtonHelp = 1,
	kButtonExit = 2,
	kButtonPrint = 3,
	kButtonPrev = 0x1e,
	kButtonNext = 0x1f,
	kButtonClear = 0x20,

	kZVase = 300,
	kZHeld = 0x1c2,
	kTimerBoat = 0,
	kBoatPeriod = 180000,

	kValueBouquet = 1,           ///< the page's profile value 1: the bouquet
	kBouquetSize = 0xf8          ///< {vase, count, kind[30], place[30]}
};

// bitmap/flowers/<kind>_<1..5>, bitmap/small/<kind> (0x5031b0)
static const char *const kKindNames[] = {
	"red_rose", "red_rosebuds", "violet_tulip", "orange_rose", "orange_lupin", "orange_marigold",
	"yellow_rose", "yellow_daffodil", "yellow_sunflower", "green_fern", "green_evergreen",
	"green_ivy", "blue_rose", "blue_lupin", "blue_bluebells", "violet_violet", "violet_orchid",
	"violet_lupin", "white_rose", "white_daisy", "blue_tulip", "yellow_tulip"
};

// The five zones over the vase, left to right; each is one place of every
// kind (0x49f6c0).
static const int kPlaceZones[5][4] = {
	{ 442, 104, 513, 460 }, { 513, 104, 568, 460 }, { 568, 104, 621, 460 },
	{ 621, 104, 679, 460 }, { 679, 104, 756, 460 }
};

// The vases (bitmap/vases/big_<n>) at (499, 360) + offset (0x49f710)
static const int kVaseBase[2] = { 499, 360 };
static const int kVaseOffsets[18][2] = {
	{ 30, 12 }, { 28, 5 }, { 27, 8 }, { 27, 17 }, { 25, -13 }, { 27, -1 }, { 23, -17 }, { 26, -11 },
	{ 26, -13 }, { 26, -13 }, { 30, -1 }, { 30, -1 }, { 30, -1 }, { 26, -3 }, { 29, -19 }, { 28, 8 },
	{ 23, 5 }, { 24, -4 }
};

// A flower at (376, 137) + the vase's offset + its place's offset (0x49f7a8)
static const int kFlowerBase[2] = { 376, 137 };
static const int kFlowerOffsets[22][5][2] = {
	{ { 48, 67 }, { 87, 34 }, { 132, 22 }, { 150, 26 }, { 172, 50 } },
	{ { 72, 154 }, { 87, 130 }, { 150, 74 }, { 189, 90 }, { 190, 138 } },
	{ { 95, 106 }, { 121, 90 }, { 153, 82 }, { 187, 90 }, { 189, 150 } },
	{ { 35, 78 }, { 54, 26 }, { 106, 22 }, { 143, 2 }, { 205, 78 } },
	{ { 77, 162 }, { 51, 102 }, { 170, 6 }, { 183, 50 }, { 185, 154 } },
	{ { 72, 146 }, { 74, 102 }, { 138, 70 }, { 156, 106 }, { 178, 154 } },
	{ { 61, 146 }, { 89, 94 }, { 142, 62 }, { 159, 70 }, { 194, 138 } },
	{ { 89, 186 }, { 138, 138 }, { 170, 98 }, { 193, 122 }, { 193, 162 } },
	{ { 2, 46 }, { 74, 42 }, { 142, 2 }, { 199, 11 }, { 200, 50 } },
	{ { 24, 204 }, { 73, 210 }, { 147, 208 }, { 184, 206 }, { 213, 211 } },
	{ { 29, 218 }, { 89, 216 }, { 177, 217 }, { 193, 214 }, { 205, 219 } },
	{ { 153, 102 }, { 36, 102 }, { 179, 70 }, { 188, 78 }, { 196, 110 } },
	{ { 108, 89 }, { 75, 38 }, { 128, 14 }, { 158, 26 }, { 196, 114 } },
	{ { 1, 102 }, { 82, 2 }, { 129, 2 }, { 144, 30 }, { 147, 118 } },
	{ { 120, 43 }, { 94, 39 }, { 169, 38 }, { 163, 51 }, { 145, 58 } },
	{ { 28, 115 }, { 90, 64 }, { 161, 45 }, { 192, 46 }, { 200, 53 } },
	{ { 94, 98 }, { 113, 58 }, { 167, 34 }, { 196, 90 }, { 201, 127 } },
	{ { 120, 150 }, { 73, 150 }, { 147, 150 }, { 180, 142 }, { 180, 142 } },
	{ { 103, 158 }, { 135, 119 }, { 162, 115 }, { 185, 143 }, { 203, 175 } },
	{ { 80, 206 }, { 110, 154 }, { 174, 138 }, { 195, 158 }, { 196, 207 } },
	{ { 111, 126 }, { 128, 118 }, { 189, 90 }, { 198, 134 }, { 201, 178 } },
	{ { 91, 146 }, { 102, 118 }, { 179, 74 }, { 196, 82 }, { 196, 122 } },
};

BouquetScene::BouquetScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def), _buttons(this),
	_held(nullptr), _nextOrder(0), _vase(0), _boat(0) {
	for (int i = 0; i < kMaxFlowers; i++) {
		_flowers[i].used = false;
		_flowers[i].kind = 0;
		_flowers[i].place = -1;
		_flowers[i].order = 0;
		_flowers[i].elem = Common::String::format("flower%d", i);
	}
}

// ---- init / close (FUN_0045e200 / FUN_0045e800) -------------------------

void BouquetScene::onInit(int arg) {
	_held = nullptr;
	_nextOrder = 0;
	showVase(0);

	// FUN_0045e530: the bouquet kept in the profile
	byte buf[kBouquetSize];
	if (_vm->profile() && _vm->profile()->getRecord(_name, Profile::kRecordValue, kValueBouquet, buf, sizeof(buf))) {
		int count = MIN<int>((int32)READ_LE_UINT32(buf + 4), kMaxFlowers);
		for (int i = 0; i < count; i++) {
			int kind = (int32)READ_LE_UINT32(buf + 8 + 4 * i);
			int place = (int32)READ_LE_UINT32(buf + 8 + 4 * kMaxFlowers + 4 * i);
			if (kind < 0 || kind >= kKinds || place < 0 || place >= kPlaces)
				continue;
			Flower *f = newFlower(kind, place, 0, 0);
			if (f)
				toFront(*f);
		}
		int vase = (int32)READ_LE_UINT32(buf);
		showVase(vase >= 0 && vase < kVases ? vase : 0);
	}

	// A boat sails by every three minutes, the first one after one to three
	setTimer(kTimerBoat, 60000 + _vm->getRandomNumber(120000 - 1));

	// The title in Amerigo BT_14_, centred in (295,3)-(503,36)
	_titleFont.load("Amerigo BT_14_");
	Anim *title = defineSurfaceAnim("title", 503 - 295, 36 - 3, _vm->_screen->format.RGBToColor(0, 0xff, 0));
	_titleFont.drawStringCentred(*title->surface(), Common::Rect(title->surface()->w, title->surface()->h), _vm->getString(_def->title));
	title->add(295, 3, 0);

	_buttons.add(kButtonHelp, 0, 0, "help_0", "help_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonExit, 0x2d8, 0, "exit_0", "exit_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonPrint, 0, 0x210, "print_0", "print_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonPrev, 0x208, 0x21b, "prev_0", "prev_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonNext, 0x280, 0x21b, "next_0", "next_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonClear, 0x2eb, 0x208, "clear_0", "clear_1", nullptr, nullptr, "", true);

	if (!carPartFound())
		addAnim("carpart", Anim::kDefaultPos, Anim::kDefaultPos, -10);
	playMusic("activity2");
}

void BouquetScene::onClose() {
	// FUN_0045e830: the flowers in the vase, back to front
	Common::Array<Flower *> flowers = placedFlowers();
	byte buf[kBouquetSize];
	memset(buf, 0, sizeof(buf));
	WRITE_LE_UINT32(buf, (uint32)_vase);
	WRITE_LE_UINT32(buf + 4, flowers.size());
	for (uint i = 0; i < flowers.size(); i++) {
		WRITE_LE_UINT32(buf + 8 + 4 * i, (uint32)flowers[i]->kind);
		WRITE_LE_UINT32(buf + 8 + 4 * kMaxFlowers + 4 * i, (uint32)flowers[i]->place);
	}
	if (_vm->profile()) {
		_vm->profile()->setRecord(_name, Profile::kRecordValue, kValueBouquet, buf, sizeof(buf));
		_vm->profileChanged();
	}
}

// ---- the vase and the flowers -----------------------------------------------

// FUN_0045e400: the vase, and the flowers move with it
void BouquetScene::showVase(int vase) {
	_vase = vase;
	_buttons.showBitmap("vase", Common::String::format("vases/big_%d", vase + 1), true, kHotspotVase,
		kVaseBase[0] + kVaseOffsets[vase][0], kVaseBase[1] + kVaseOffsets[vase][1], kZVase);
	for (auto &f : _flowers) {
		if (f.used && f.place >= 0) {
			Common::Point p = flowerPos(f);
			anim(f.elem.c_str())->setPos(p.x, p.y);
		}
	}
}

// FUN_0045e4c0
Common::Point BouquetScene::flowerPos(const Flower &f) const {
	const int *o = kFlowerOffsets[f.kind][f.place];
	return Common::Point(o[0] + kVaseOffsets[_vase][0] + kFlowerBase[0], o[1] + kVaseOffsets[_vase][1] + kFlowerBase[1]);
}

// FUN_0045e650: behind the vase, except the middle places of the fern and
// the evergreen, which hang over its front
int BouquetScene::flowerZ(const Flower &f) const {
	if ((f.kind == 9 || f.kind == 10) && f.place >= 1 && f.place <= 3)
		return f.order + 400;
	return f.order + 50;
}

// FUN_0045e700: a flower in the vase shows its place's picture, one held
// outside the small picture of its kind
void BouquetScene::placeFlower(Flower &f, int place, int x, int y) {
	f.place = place;
	if (place < 0) {
		_buttons.showBitmap(f.elem, Common::String::format("small/%s", kKindNames[f.kind]), true,
			kHotspotFirstFlower + (&f - _flowers), x, y, kZHeld);
	} else {
		Common::Point p = flowerPos(f);
		_buttons.showBitmap(f.elem, Common::String::format("flowers/%s_%d", kKindNames[f.kind], place + 1), true,
			kHotspotFirstFlower + (&f - _flowers), p.x, p.y, flowerZ(f));
	}
}

// FUN_0045e690
BouquetScene::Flower *BouquetScene::newFlower(int kind, int place, int x, int y) {
	for (auto &f : _flowers) {
		if (f.used)
			continue;
		f.used = true;
		f.kind = kind;
		placeFlower(f, place, x, y);
		return &f;
	}
	return nullptr;
}

// FUN_0045ea80
void BouquetScene::removeFlower(Flower &f) {
	if (isAnimAdded(f.elem.c_str()))
		removeAnim(f.elem.c_str());
	f.used = false;
}

// FUN_0045e5c0: the stacking order is renumbered when it passes 30
void BouquetScene::toFront(Flower &f) {
	if (_nextOrder > kMaxFlowers) {
		for (auto &o : _flowers) {
			if (o.used && &o != &f) {
				o.order -= kMaxFlowers;
				if (isAnimAdded(o.elem.c_str()))
					anim(o.elem.c_str())->setZ(o.place < 0 ? kZHeld : flowerZ(o));
			}
		}
		_nextOrder -= kMaxFlowers;
	}
	f.order = _nextOrder++;
	if (isAnimAdded(f.elem.c_str()))
		anim(f.elem.c_str())->setZ(f.place < 0 ? kZHeld : flowerZ(f));
}

// FUN_0045e9e0
int BouquetScene::placeAt(int x, int y) const {
	for (int i = 0; i < kPlaces; i++)
		if (x >= kPlaceZones[i][0] && x < kPlaceZones[i][2] && y >= kPlaceZones[i][1] && y < kPlaceZones[i][3])
			return i;
	return -1;
}

// FUN_0045e960 / FUN_0045e9a0 (mouse moves with the button held): over a
// zone the flower jumps to its place, outside it follows the mouse small
void BouquetScene::dragTo(int x, int y) {
	if (!_held)
		return;
	int place = placeAt(x, y);
	if (place != _held->place)
		placeFlower(*_held, place, x, y);
	if (place < 0) {
		Anim *a = anim(_held->elem.c_str());
		Common::Rect r = a->rect();
		a->setPos(x - r.width() / 2, y - r.height() / 2);
	}
}

// FUN_0045e8b0: the flowers in the vase sorted by z
Common::Array<BouquetScene::Flower *> BouquetScene::placedFlowers() {
	Common::Array<Flower *> list;
	for (auto &f : _flowers)
		if (f.used && f.place >= 0)
			list.push_back(&f);
	Common::sort(list.begin(), list.end(), [this](const Flower *a, const Flower *b) {
		return flowerZ(*a) < flowerZ(*b);
	});
	return list;
}

// ---- input --------------------------------------------------------------------

void BouquetScene::onMouseDown(int hotspot, int x, int y) {
	// FUN_0045eb00
	_lastMouse = Common::Point(x, y);
	if (hotspot == kHotspotCarPart) {
		takeCarPart();
	} else if (hotspot >= kHotspotFirstKind && hotspot < kHotspotFirstKind + kKinds) {
		// FUN_0045ebc0: a new flower in the hand, centred on the mouse
		Flower *f = newFlower(hotspot - kHotspotFirstKind, -1, x, y);
		if (!f) {
			_vm->messageBox("interfaceh:WARNING", "bouquet:FLOWERSMAX", MessageBox::kButtonOk);
		} else {
			dragTo(x, y);
			toFront(*f);
			_held = f;
		}
	} else if (hotspot >= kHotspotFirstFlower && hotspot < kHotspotFirstFlower + kMaxFlowers) {
		// FUN_0045eba0: picks up a flower of the vase
		Flower &f = _flowers[hotspot - kHotspotFirstFlower];
		if (f.used) {
			toFront(f);
			_held = &f;
		}
	}
	int id = _buttons.mouseDown(hotspot, x, y);
	(void)id;
}

void BouquetScene::onMouseUp(int hotspot, int x, int y) {
	// FUN_0045ea20: dropped over the vase it takes its place (and pushes out
	// the flower of the same kind there), elsewhere it is thrown away
	if (_held) {
		dragTo(x, y);
		if (placeAt(x, y) >= 0) {
			for (auto &f : _flowers)
				if (f.used && &f != _held && f.kind == _held->kind && f.place == _held->place)
					removeFlower(f);
		} else {
			removeFlower(*_held);
		}
		_held = nullptr;
	}

	// FUN_0045ec10 (event 0x116)
	switch (_buttons.mouseUp()) {
	case kButtonHelp:
		_vm->showHelp();
		break;
	case kButtonExit:
		_vm->endGame();
		break;
	case kButtonPrint:
		print();
		break;
	case kButtonPrev:
		showVase((_vase + kVases - 1) % kVases);
		break;
	case kButtonNext:
		showVase((_vase + 1) % kVases);
		break;
	case kButtonClear:
		for (auto &f : _flowers)
			if (f.used)
				removeFlower(f);
		break;
	default:
		break;
	}
}

void BouquetScene::onMouseMove(int hotspot, int x, int y) {
	_buttons.setHover(hotspot);
}

void BouquetScene::onUpdate() {
	// The drag follows every mouse move (event 0x109)
	Common::Point m = _vm->mousePos();
	if (_held && m != _lastMouse) {
		_lastMouse = m;
		dragTo(m.x, m.y);
	}
}

void BouquetScene::onAnimFinished(Anim *a) {
	// FUN_0045ee30
	Common::String n(a->name());
	if (n == "sailboat" || n == "raceboat")
		a->remove();
}

void BouquetScene::onTimer(int id, int data) {
	// FUN_0045ee50: the sailboat and the raceboat take turns
	if (id != kTimerBoat)
		return;
	setTimer(kTimerBoat, kBoatPeriod);
	playAnim(_boat == 0 ? "sailboat" : "raceboat");
	_boat ^= 1;
}

// FUN_0045ec10 case 3 / FUN_0045ed10: the bouquet (flowers by z, the vase
// on top) on white goes to the activity centre with the request
// {"bouquet", 0, picture} (DAT_006a1970).
void BouquetScene::print() {
	Common::Array<Flower *> flowers = placedFlowers();
	Anim *vase = anim("vase");
	Common::Rect bounds = vase->rect();
	for (Flower *f : flowers)
		bounds.extend(anim(f->elem.c_str())->rect());
	Graphics::ManagedSurface img(bounds.width(), bounds.height(), _vm->_screen->format);
	img.fillRect(Common::Rect(img.w, img.h), img.format.RGBToColor(0xff, 0xff, 0xff));
	for (Flower *f : flowers) {
		Graphics::ManagedSurface *s = anim(f->elem.c_str())->surface();
		Common::Rect r = anim(f->elem.c_str())->rect();
		img.transBlitFrom(*s, Common::Point(r.left - bounds.left, r.top - bounds.top), s->getPixel(0, 0));
	}
	Graphics::ManagedSurface *s = vase->surface();
	img.transBlitFrom(*s, Common::Point(vase->rect().left - bounds.left, vase->rect().top - bounds.top), s->getPixel(0, 0));
	_vm->showActivity("bouquet", 0, img);
}

} // End of namespace Flaaklypa
