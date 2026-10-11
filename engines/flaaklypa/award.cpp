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
#include "common/config-manager.h"
#include "common/debug.h"
#include "common/system.h"
#include "common/tokenizer.h"
#include "graphics/screen.h"

#include "flaaklypa/award.h"
#include "flaaklypa/button.h"
#include "flaaklypa/dialog.h"
#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/profile.h"
#include "flaaklypa/resources.h"

namespace Flaaklypa {

// ---- tables ----------------------------------------------------------------

// Items per category (0x49ca88).
static const int kItemCount[kAwardCategories] = { 62, 18, 10, 8, 31, 6, 100, 15, 15, 15 };

// Items every player has (0x49ca60): a count for car parts and bonus points,
// a bit mask for the others (film clip 1, car 1, track 1, images 1..8).
static const int32 kFreeItems[kAwardCategories] = { 0, 1, 1, 1, 0xff, 0, 0, 0, 0, 0xf };

// Butterfly (bit) -> language.ini and bitmap number (0x49cab0).
static const int kButterflyIndex[15] = { 1, 0, 4, 3, 2, 5, 9, 8, 7, 10, 12, 11, 6, 13, 14 };

// The names of the 3D viewer scenes (0x4ad7fc).
static const char *const k3DSceneNames[15] = {
	"intro:SCENENAME", "anaglyph:SCENE_YARD", "desk:SCENENAME", "anaglyph:SCENE_PEE",
	"anaglyph:SCENE_HOUSE", "anaglyph:SCENE_TVROOM", "garage:SCENENAME", "morning:SCENENAME",
	"anaglyph:SCENE_OUTTENT", "anaglyph:SCENE_INTENT", "buildacar:SCENENAME", "tvstation:SCENENAME",
	"race:SCENENAME", "goodbye:SCENENAME", "anaglyph:SCENE_OUTRO"
};

// language.ini item names per category (0x4ad89c): "awarddlg:<prefix>_<n + 1>".
// The bonus points have no names; 3D scenes use k3DSceneNames.
static const char *const kNamePrefixes[kAwardCategories] = {
	"PART", "FILM", "CAR", "TRACK", "IMAGE", "CHARACTER", nullptr, "BUTTERFLY", nullptr, "3DIMAGE"
};

// Item bitmaps of the award dialog in common/bitmap/award/ (0x4ad874);
// car parts are part_<n + 1> ("part_%02d"), the others one bitmap per
// category. There is no butterfly.bmp or 3dimage.bmp.
static const char *const kItemBitmaps[kAwardCategories] = {
	"part", "filmclip", "car", "track", "image", "character", "", "butterfly", "3dscene", "3dimage"
};

// Bonus points that award other items (0x4ad8c8, 25 records: bonus point,
// category, count, flags).
static const struct {
	int bonus, type, count, flags;
} kBonusAwards[] = {
	{ 2, kAwardImage, 1, 0 }, { 4, kAwardFilmClip, 1, 0 }, { 6, kAwardCar, 1, 0 },
	{ 8, kAwardImage, 1, 0 }, { 10, kAwardFilmClip, 1, 0 }, { 12, kAwardImage, 1, 0 },
	{ 14, kAwardFilmClip, 1, 0 }, { 16, kAwardCar, 1, 0 }, { 18, kAwardImage, 1, 0 },
	{ 20, kAwardFilmClip, 1, 0 }, { 22, kAwardImage, 1, 0 }, { 24, kAwardFilmClip, 1, 0 },
	{ 26, kAwardCar, 1, 0 }, { 28, kAwardImage, 1, 0 }, { 30, kAwardFilmClip, 1, 0 },
	{ 32, kAwardImage, 1, 0 }, { 34, kAwardFilmClip, 1, 0 }, { 36, kAwardCar, 1, 0 },
	{ 38, kAwardImage, 1, 0 }, { 40, kAwardFilmClip, 1, 0 }, { 42, kAwardImage, 1, 0 },
	{ 44, kAwardFilmClip, 1, 0 }, { 46, kAwardCar, 1, 0 }, { 48, kAwardImage, 1, 0 },
	{ 50, kAwardFilmClip, 1, 0 }
};

// The image gallery (0x4adc04..0x4adc40): five ranges of items, their
// category and bitmap names.
static const struct {
	int type;
	const char *bitmap;
	int start, end;
} kGallery[] = {
	{ kAwardImage, "image", 0, 31 },
	{ kAwardCar, "car", 31, 41 },
	{ kAwardTrack, "track", 41, 49 },
	{ kAwardButterfly, "butterfly", 49, 64 },
	{ kAward3DImage, "3dimage", 64, 79 }
};

// The medals (local table of FUN_0041bce0, 0x4ad590)
static const char *const kMedalBitmaps[5] = { nullptr, "bronze", "silver", "gold", "platinum" };
static const char *const kMedalNames[5] = {
	"awarddlg:NONE", "awarddlg:BRONZE", "awarddlg:SILVER", "awarddlg:GOLD", "awarddlg:PLATINUM"
};

// The "anaglyph" scene's profile value with a bit per 3D scene found.
static const char *const k3DSceneOwner = "anaglyph";
enum {
	k3DSceneKey = 100
};

static bool validType(int type) {
	return type >= 0 && type < kAwardCategories;
}

// ---- counters (FUN_00411b70 / FUN_00411c40) --------------------------------

static void readCounters(FlaaklypaEngine *vm, int32 *counters) {
	const Profile *p = vm->profile();
	for (int i = 0; i < kAwardTypes; i++)
		counters[i] = p ? p->award(i) : 0;
}

static void writeCounters(FlaaklypaEngine *vm, const int32 *counters) {
	Profile *p = vm->profile();
	if (!p)
		return;
	for (int i = 0; i < kAwardTypes; i++)
		p->setAward(i, counters[i]);
	vm->profileChanged();
}

int awardItemCount(int type) {
	return validType(type) ? kItemCount[type] : 0;
}

int32 awardCounter(FlaaklypaEngine *vm, int type) {
	if (type < 0 || type >= kAwardTypes)
		return 0;
	const Profile *p = vm->profile();
	return p ? p->award(type) : 0;
}

// FUN_0041b660
static bool hasBit(int32 mask, int n) {
	return ((uint32)mask & (1u << (n & 0x1f))) != 0;
}

bool awardIsWon(int type, int32 counter, int item) {
	switch (type) {
	case kAwardCarPart:
	case kAwardBonus:
		return item < kFreeItems[type] || item < counter;
	case kAwardFilmClip:
	case kAwardCar:
	case kAwardTrack:
	case kAwardImage:
	case kAwardCharacter:
	case kAwardButterfly:
		return hasBit(kFreeItems[type], item) || hasBit(counter, item);
	default:
		return false;
	}
}

bool awardHasItem(FlaaklypaEngine *vm, int type, int item) {
	return awardIsWon(type, awardCounter(vm, type), item);
}

int awardWonCount(int type, int32 counter) {
	int n = 0;
	for (int i = 0; i < awardItemCount(type); i++)
		if (awardIsWon(type, counter, i))
			n++;
	return n;
}

int32 awardAddItem(int type, int32 counter, int item) {
	if (item < 0 || item >= awardItemCount(type))
		return counter;
	switch (type) {
	case kAwardCarPart:
	case kAwardBonus:
		return counter <= item ? item + 1 : counter;
	case kAwardFilmClip:
	case kAwardCar:
	case kAwardTrack:
	case kAwardImage:
	case kAwardCharacter:
	case kAwardButterfly:
		return (int32)((uint32)counter | (1u << (item & 0x1f)));
	default:
		return counter;
	}
}

// FUN_0041b830: the first count items not won yet, in item order.
static int32 pickItems(int type, int32 counter, int &count, Common::Array<int> &items) {
	if (count < 1) {
		count = 0;
		return counter;
	}
	int got = 0;
	for (int i = 0; got < count && i < kItemCount[type]; i++) {
		if (awardIsWon(type, counter, i))
			continue;
		counter = awardAddItem(type, counter, i);
		items.push_back(i);
		got++;
	}
	count = got;
	return counter;
}

int awardGive(FlaaklypaEngine *vm, int type, int count, int flags, Common::Array<int> *items) {
	// Only the profile's counters (the original reads past them otherwise).
	if (type < 0 || type >= kAwardTypes)
		return 0;
	int32 counters[kAwardTypes];
	readCounters(vm, counters);
	const int32 oldCounter = counters[type], oldImages = counters[kAwardImage];
	Common::Array<int> got;
	if (flags & kAwardSpecificItem) {
		counters[type] = awardAddItem(type, counters[type], count);
		got.push_back(count);
		count = 1;
	} else {
		counters[type] = pickItems(type, counters[type], count, got);
	}
	// A new car also unlocks its picture in the gallery.
	if (type == kAwardCar)
		for (int i = 0; i < count; i++)
			counters[kAwardImage] = awardAddItem(kAwardImage, counters[kAwardImage], got[i] + 0x12);
	if (counters[type] != oldCounter || counters[kAwardImage] != oldImages)
		writeCounters(vm, counters);
	if (items)
		for (int i = 0; i < count; i++)
			items->push_back(got[i]);
	return count;
}

Common::String awardItemName(FlaaklypaEngine *vm, int type, int item) {
	if (!validType(type) || item < 0 || item >= kItemCount[type])
		return vm->getString("awarddlg:NONE");
	if (type == kAward3DScene)
		return vm->getString(k3DSceneNames[item]);
	if (type == kAwardButterfly)
		item = awardButterflyIndex(item);
	if (!kNamePrefixes[type])
		return "";
	return vm->getString(Common::String::format("awarddlg:%s_%d", kNamePrefixes[type], item + 1));
}

int awardButterflyIndex(int butterfly) {
	return butterfly >= 0 && butterfly < 15 ? kButterflyIndex[butterfly] : -1;
}

static uint32 found3DScenes(FlaaklypaEngine *vm) {
	const Profile *p = vm->profile();
	return p ? (uint32)p->getValue(k3DSceneOwner, k3DSceneKey, 0) : 0;
}

bool award3DSceneFound(FlaaklypaEngine *vm, int scene) {
	return hasBit(found3DScenes(vm), scene);
}

// ---- gallery ---------------------------------------------------------------

int galleryItemCount() {
	return kGallery[ARRAYSIZE(kGallery) - 1].end;
}

int galleryRange(int type, int *start, int *end) {
	int s = 0, e = 0;
	for (const auto &g : kGallery) {
		if (g.type == type) {
			s = g.start;
			e = g.end;
			break;
		}
	}
	if (start)
		*start = s;
	if (end)
		*end = e;
	return e - s;
}

// FUN_0041b440: the gallery range of an item
static int galleryPart(int item) {
	if (item < 0)
		return -1;
	for (uint i = 0; i < ARRAYSIZE(kGallery); i++)
		if (item < kGallery[i].end)
			return i;
	return -1;
}

bool galleryItemAvailable(FlaaklypaEngine *vm, int item) {
	int part = galleryPart(item);
	if (part < 0)
		return false;
	int type = kGallery[part].type;
	if (type == kAward3DImage)
		return true;
	return awardHasItem(vm, type, item - kGallery[part].start);
}

Common::String galleryItemName(FlaaklypaEngine *vm, int item) {
	int part = galleryPart(item);
	if (part < 0)
		return "";
	return awardItemName(vm, kGallery[part].type, item - kGallery[part].start);
}

Common::String galleryItemBitmap(int item) {
	int part = galleryPart(item);
	if (part < 0)
		return "";
	return Common::String::format("%s_%02d", kGallery[part].bitmap, item - kGallery[part].start + 1);
}

// ---- deciding what is shown (FUN_0041b900) ---------------------------------

// FUN_0041bab0: a 3D scene is awarded once.
static bool give3DScene(FlaaklypaEngine *vm, int scene) {
	uint32 mask = found3DScenes(vm);
	uint32 newMask = mask | (1u << (scene & 0x1f));
	if (newMask == mask)
		return false;
	Profile *p = vm->profile();
	if (p) {
		p->setValue(k3DSceneOwner, k3DSceneKey, (int32)newMask);
		vm->profileChanged();
	}
	return true;
}

// FUN_0041b9d0 / FUN_0041ba50: count bonus points; those listed in
// kBonusAwards award other items. The last of them is reported through
// type, count and items when given. Returns whether any did.
static bool giveBonus(FlaaklypaEngine *vm, int points, int *type, int *count, Common::Array<int> *items) {
	Common::Array<int> bonus;
	int n = awardGive(vm, kAwardBonus, points, 0, &bonus);
	bool any = false;
	for (int i = 0; i < n; i++) {
		for (const auto &b : kBonusAwards) {
			if (b.bonus != bonus[i])
				continue;
			if (items)
				items->clear();
			int got = awardGive(vm, b.type, b.count, b.flags, items);
			if (type)
				*type = b.type;
			// The original reports the table's count even when the category
			// was full, and the dialog then shows an item from uninitialised
			// memory.
			if (count)
				*count = got;
			any = true;
			break;
		}
	}
	return any;
}

static bool decideAward(FlaaklypaEngine *vm, int &type, int &count, int flags, Common::Array<int> &items) {
	const bool medal = (flags & kAwardMedalMask) != 0;
	if (type == kAward3DScene) {
		int scene = count;
		items.push_back(scene);
		count = 1;
		return give3DScene(vm, scene);
	}
	if (count == 0)
		return medal;
	int got = awardGive(vm, type, count, flags, &items);
	if (got == count || type != kAwardCarPart) {
		count = got;
		return got != 0 || medal;
	}
	// All car parts won: the parts left over become bonus points.
	if (got != 0) {
		giveBonus(vm, count - got, nullptr, nullptr, nullptr);
		count = got;
		return true;
	}
	bool bonus = giveBonus(vm, count, &type, &count, &items);
	// The original keeps the count when no bonus award came and shows that
	// many items from uninitialised memory next to a medal.
	if (!bonus)
		count = 0;
	return bonus || medal;
}

// ---- the award dialog (FUN_0041bc00 / FUN_0041c000 / FUN_0041bce0 / FUN_0041be20)

// Layout relative to the dialog, 317x345, centred on the screen (FUN_0041bb20)
#define kAwardTitleRect Common::Rect(13, 8, 297, 39)    // 0x49caf0
#define kAwardTextRect Common::Rect(16, 55, 294, 221)   // 0x49cb00
#define kAwardItemsRect Common::Rect(10, 236, 297, 286) // 0x49cb10

enum {
	kAwardDialogW = 0x13d,
	kAwardDialogH = 0x159,
	kAwardButtonClose = 500,
	kAwardSlots = 5,              ///< item slots, the last one holds the medal
	kAwardTooltipDelay = 1500     ///< the tooltip module's default (FUN_00409b50)
};

class AwardDialog : public Dialog {
public:
	AwardDialog(FlaaklypaEngine *vm, int type, const Common::Array<int> &items, int count,
	            const Common::String &title, const Common::String &text, int flags);
	~AwardDialog() override;

	void draw(Graphics::ManagedSurface &dst) override;

protected:
	void onButton(int id) override;
	void onKey(const Common::KeyState &key) override;
	void onMouseDown(int x, int y, bool onDialog) override;
	void onRightClick(int x, int y) override;
	void onMouseMove(int x, int y, bool leftHeld) override;
	void drawContents(Graphics::ManagedSurface &dst) override;

private:
	struct Slot {
		Graphics::ManagedSurface *bitmap;
		Common::String name;   ///< the tooltip (0x592630)
		Common::Rect rect;     ///< on the screen
	};

	void setSlot(int i, const Common::String &bitmap, const Common::String &name);
	int slotAt(int x, int y) const;
	void hideTooltip();

	Slot _slots[kAwardSlots];
	Tooltip _tooltip;
	Graphics::ManagedSurface *_tip;
	Common::Point _tipPos;
	int _hover;          ///< slot under the mouse, -1 none, -2 not known yet
	bool _tipPending;    ///< the hover time of the tooltip is running
	uint32 _hoverTime;
	Common::Point _mouse;
};

AwardDialog::AwardDialog(FlaaklypaEngine *vm, int type, const Common::Array<int> &items, int count,
                         const Common::String &title, const Common::String &text, int flags) :
	Dialog(vm, true), _tooltip(nullptr), _tip(nullptr), _hover(-2), _tipPending(false), _hoverTime(0) {
	for (int i = 0; i < kAwardSlots; i++)
		_slots[i].bitmap = nullptr;

	// FUN_0041c000: dialog "awarddlg" (dimmed background, hit mask) at z
	// 500, the CLOSE button 500 at z 501 labelled in Amerigo BT_10_.
	Graphics::ManagedSurface *backdrop = loadBitmap("awarddlg", "backdrop");
	if (!backdrop) {
		close(0);
		return;
	}
	const int dx = (kScreenWidth - kAwardDialogW) / 2, dy = (kScreenHeight - kAwardDialogH) / 2;

	// FUN_0041be20: the title in Amerigo BT_14_ centred (a label at z 501)
	// and the text with awarddlg:AWARDBELOW word wrapped in Amerigo BT_10_
	// onto the dialog bitmap. Missing texts get the defaults of the category,
	// overridden by those of the medal.
	const int medal = flags & kAwardMedalMask;
	Common::String t = title;
	if (t.empty()) {
		t = "awarddlg:TITLE_DEFAULT";
		static const char *const titles[] = { nullptr, "awarddlg:TITLE_FILMCLIP", "awarddlg:TITLE_CAR", "awarddlg:TITLE_TRACK", "awarddlg:TITLE_IMAGE" };
		if (type >= 1 && type <= 4)
			t = titles[type];
		static const char *const medalTitles[] = { nullptr, "awarddlg:TITLE_BRONZE", "awarddlg:TITLE_SILVER", "awarddlg:TITLE_GOLD", "awarddlg:TITLE_PLATINUM" };
		if (medal >= 1 && medal <= 4)
			t = medalTitles[medal];
	}
	Common::String d = text;
	if (d.empty()) {
		static const char *const texts[] = { "awarddlg:DESC_CARPART", "awarddlg:DESC_FILMCLIP", "awarddlg:DESC_CAR", "awarddlg:DESC_TRACK", "awarddlg:DESC_IMAGE" };
		if (type >= 0 && type <= 4)
			d = texts[type];
		static const char *const medalTexts[] = { nullptr, "awarddlg:DESC_BRONZE", "awarddlg:DESC_SILVER", "awarddlg:DESC_GOLD", "awarddlg:DESC_PLATINUM" };
		if (medal >= 1 && medal <= 4)
			d = medalTexts[medal];
	}
	BitmapFont titleFont, textFont;
	titleFont.load("Amerigo BT_14_");
	textFont.load("Amerigo BT_10_");
	titleFont.drawStringCentred(*backdrop, kAwardTitleRect, _vm->getString(t));
	if (!d.empty()) {
		// FUN_00402750: from the top of the box, not clipped to it
		Common::String s = Common::String::format("%s\n\n%s", _vm->getString(d).c_str(), _vm->getString("awarddlg:AWARDBELOW").c_str());
		Common::Array<Common::String> lines = wordWrap(textFont, s, kAwardTextRect.width());
		int y = kAwardTextRect.top;
		for (const auto &line : lines) {
			textFont.drawString(*backdrop, kAwardTextRect.left, y, line);
			y += textFont.height();
		}
	}
	setSurface(backdrop, dx, dy);
	setHitMask(_vm->_resources->loadMask("common", "dialogue/awarddlg/hotspot.bmp"));
	addButton(kAwardButtonClose, 0xd8, 0x129, "awarddlg", "button_0", "button_1", nullptr, _vm->getString("interfaceh:CLOSE"));

	// FUN_0041bc00: five slots across the items box, 1/5 of its width apart
	// (rounded), at the top of the box; transparent, hit tested on their
	// rectangle (mode 2), hotspots 0x226.., z 501.
	for (int i = 0; i < kAwardSlots; i++) {
		int off = (i * kAwardItemsRect.width() * 2 + 5) / 10;
		_slots[i].rect = Common::Rect(dx + kAwardItemsRect.left + off, dy + kAwardItemsRect.top,
		                              dx + kAwardItemsRect.left + off, dy + kAwardItemsRect.top);
	}

	// FUN_0041bce0: a medal takes the last slot; up to five items, car parts
	// with their own bitmaps.
	int maxItems = kAwardSlots;
	if (medal) {
		if (medal <= 4)
			setSlot(kAwardSlots - 1, kMedalBitmaps[medal], _vm->getString(kMedalNames[medal]));
		maxItems = kAwardSlots - 1;
	}
	count = CLIP<int>(count, 0, MIN<int>(maxItems, items.size()));
	for (int i = 0; i < count; i++) {
		Common::String bitmap;
		if (validType(type))
			bitmap = type == kAwardCarPart ? Common::String::format("part_%02d", items[i] + 1) : Common::String(kItemBitmaps[type]);
		setSlot(i, bitmap, awardItemName(_vm, type, items[i]));
	}
}

AwardDialog::~AwardDialog() {
	for (int i = 0; i < kAwardSlots; i++)
		delete _slots[i].bitmap;
	delete _tip;
}

void AwardDialog::setSlot(int i, const Common::String &bitmap, const Common::String &name) {
	Slot &s = _slots[i];
	s.name = name;
	if (bitmap.empty())
		return;
	s.bitmap = _vm->_resources->loadBitmap("common", "bitmap/award/" + bitmap + ".bmp", _vm->_screen->format);
	if (!s.bitmap) {
		debug(1, "Award: no bitmap common/bitmap/award/%s.bmp", bitmap.c_str());
		return;
	}
	s.rect.setWidth(s.bitmap->w);
	s.rect.setHeight(s.bitmap->h);
}

int AwardDialog::slotAt(int x, int y) const {
	// The elements above the dialog in z order; the tooltip has no hotspot.
	for (int i = 0; i < kAwardSlots; i++)
		if (_slots[i].bitmap && _slots[i].rect.contains(x, y))
			return i;
	return -1;
}

void AwardDialog::hideTooltip() {
	// FUN_00409bc0
	delete _tip;
	_tip = nullptr;
}

void AwardDialog::onButton(int id) {
	// FUN_0041c350
	if (id == kAwardButtonClose)
		close(1);
}

void AwardDialog::onKey(const Common::KeyState &key) {
	// FUN_0041c430
	if (key.keycode == Common::KEYCODE_RETURN || key.keycode == Common::KEYCODE_KP_ENTER)
		close(1);
	else if (key.keycode == Common::KEYCODE_ESCAPE)
		close(2);
}

void AwardDialog::onMouseDown(int x, int y, bool onDialog) {
	// FUN_0041c300: clicks hide the tooltip
	hideTooltip();
}

void AwardDialog::onRightClick(int x, int y) {
	hideTooltip();
}

void AwardDialog::onMouseMove(int x, int y, bool leftHeld) {
	// Events 0x113/0x114 -> FUN_0041c330 -> FUN_00409e90: a new hotspot
	// hides the tooltip and restarts the hover time (real time, timeGetTime).
	_mouse = Common::Point(x, y);
	int slot = slotAt(x, y);
	if (slot == _hover)
		return;
	_hover = slot;
	hideTooltip();
	_tipPending = true;
	_hoverTime = g_system->getMillis();
}

void AwardDialog::drawContents(Graphics::ManagedSurface &dst) {
	for (const auto &s : _slots)
		if (s.bitmap)
			dst.transBlitFrom(*s.bitmap, Common::Point(s.rect.left, s.rect.top), s.bitmap->getPixel(0, 0));
}

void AwardDialog::draw(Graphics::ManagedSurface &dst) {
	// The dialog's frame tick (event 0x112 -> FUN_00409ed0) asks for the
	// tooltip (0x11a -> FUN_0041c400): the name of the item or medal under
	// the mouse.
	if (_tipPending && g_system->getMillis() >= _hoverTime + kAwardTooltipDelay) {
		_tipPending = false;
		if (_hover >= 0 && !_slots[_hover].name.empty())
			_tip = _tooltip.render(_slots[_hover].name, _mouse.x, _mouse.y, _tipPos);
	}
	Dialog::draw(dst);
	// The tooltip is at z 1500, above the dialog's buttons.
	if (_tip)
		dst.transBlitFrom(*_tip, _tipPos, _tip->getPixel(0, 0));
}

// ---- entry points ----------------------------------------------------------

bool showAward(FlaaklypaEngine *vm, int type, const Common::String &title, const Common::String &text, int count, int flags) {
	Common::Array<int> items;
	const int origType = type, origCount = count;
	if (!decideAward(vm, type, count, flags, items)) {
		debug(1, "Award: category %d, count %d, flags 0x%x: nothing won", origType, origCount, flags);
		return false;
	}
	Common::String list;
	for (uint i = 0; i < items.size(); i++)
		list += Common::String::format(i ? ", %d" : "%d", items[i]);
	debug(1, "Award: category %d, count %d, flags 0x%x: shows category %d, %d items (%s), medal %d",
	      origType, origCount, flags, type, count, list.c_str(), flags & kAwardMedalMask);
	AwardDialog dlg(vm, type, items, count, title, text, flags);
	dlg.run();
	return true;
}

void awardDifficulty(FlaaklypaEngine *vm, int level, int count) {
	// FUN_0041c0b0: other levels get the defaults
	static const char *const titles[] = { "awarddlg:TITLE_EASY", "awarddlg:TITLE_MEDIUM", "awarddlg:TITLE_HARD" };
	static const char *const texts[] = { "awarddlg:DESC_EASY", "awarddlg:DESC_MEDIUM", "awarddlg:DESC_HARD" };
	bool known = level >= 0 && level <= 2;
	showAward(vm, kAwardCarPart, known ? titles[level] : "", known ? texts[level] : "", count, 0);
}

void awardFactsFound(FlaaklypaEngine *vm) {
	showAward(vm, kAwardCarPart, "awarddlg:TITLE_FACT", "awarddlg:DESC_FACT", 1, 0);
}

void awardHiddenCarPart(FlaaklypaEngine *vm) {
	showAward(vm, kAwardCarPart, "awarddlg:TITLE_HIDDEN", "awarddlg:DESC_HIDDEN", 1, 0);
}

void awardJigsaw(FlaaklypaEngine *vm) {
	showAward(vm, kAwardCarPart, "awarddlg:TITLE_JIGSAW", "awarddlg:DESC_JIGSAW", 1, 0);
}

void award3DScene(FlaaklypaEngine *vm, int scene) {
	showAward(vm, kAward3DScene, "awarddlg:TITLE_3DSCENE", "awarddlg:DESC_3DSCENE", scene, kAwardSpecificItem);
}

void awardPlayTime(FlaaklypaEngine *vm) {
	// FUN_0040d4c0: "timeaward" 0 marks the award as given; without a
	// profile it would come every time, but the original only counts the
	// time from the selection of a profile (FUN_0040d860).
	Profile *p = vm->profile();
	if (p) {
		if (p->getValue("timeaward", 0, 0))
			return;
		p->setValue("timeaward", 0, 1);
		vm->profileChanged();
	}
	showAward(vm, kAwardCarPart, "gamec:TIME_TITLE", "gamec:TIME_DESC", 1, 0);
}

// ---- development aid -------------------------------------------------------

static void logCounters(FlaaklypaEngine *vm, const char *when) {
	int32 c[kAwardTypes];
	readCounters(vm, c);
	debug(1, "Award test %s: counters %d 0x%x 0x%x 0x%x 0x%x 0x%x %d 0x%x, 3D scenes 0x%x", when,
	      c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7], found3DScenes(vm));
}

void testAward(FlaaklypaEngine *vm, const Common::String &spec) {
	Common::StringTokenizer tok(spec, ",");
	Common::String kind = tok.nextToken();
	Common::String countStr = tok.nextToken(), flagsStr = tok.nextToken();
	int count = countStr.empty() ? 1 : (int)strtol(countStr.c_str(), nullptr, 0);
	int flags = flagsStr.empty() ? 0 : (int)strtol(flagsStr.c_str(), nullptr, 0);

	bool temporary = false;
	if (!vm->_profile) {
		vm->_profile = new Profile("Test", 0, 1, 0);
		temporary = true;
		if (ConfMan.hasKey("test_award_counters")) {
			Common::StringTokenizer c(ConfMan.get("test_award_counters"), ",");
			for (int i = 0; i < kAwardTypes && !c.empty(); i++)
				vm->_profile->setAward(i, (int32)strtoul(c.nextToken().c_str(), nullptr, 0));
		}
		if (ConfMan.hasKey("test_award_anaglyph"))
			vm->_profile->setValue(k3DSceneOwner, k3DSceneKey, (int32)strtoul(ConfMan.get("test_award_anaglyph").c_str(), nullptr, 0));
	}
	logCounters(vm, "before");

	if (kind == "easy" || kind == "medium" || kind == "hard")
		awardDifficulty(vm, kind == "easy" ? 0 : kind == "medium" ? 1 : 2, count);
	else if (kind == "fact")
		awardFactsFound(vm);
	else if (kind == "hidden")
		awardHiddenCarPart(vm);
	else if (kind == "jigsaw")
		awardJigsaw(vm);
	else if (kind == "3dscene")
		award3DScene(vm, count);
	else if (kind == "time")
		awardPlayTime(vm);
	else
		showAward(vm, atoi(kind.c_str()), "", "", count, flags);

	logCounters(vm, "after");
	if (temporary) {
		delete vm->_profile;
		vm->_profile = nullptr;
	}
}

} // End of namespace Flaaklypa
