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

#include "flaaklypa/award.h"
#include "flaaklypa/dialog.h"
#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/music.h"
#include "flaaklypa/profile.h"
#include "flaaklypa/scenes.h"
#include "flaaklypa/scenedata.h"

namespace Flaaklypa {

// The car building workshop (handler FUN_00423210, functions 0x423210..
// 0x424640). The parts the player has won lie in the tray at the bottom
// ("menu_part_NN"); dragged onto their place on Il Tempo Gigante they
// become "part_NN". Five stages show the car from different angles; the
// next one opens when every part of a stage is fitted.

// Animation lists, as in the original's data segment.
static const char *const kNarrations[] = { "S10AN-NAR-001", "S10AN-NAR-002", "S10AN-NAR-003", "S10AN-NAR-004", "S10AN-NAR-005", nullptr }; // 0x4b1568
static const char *const kStage1Anims[] = { "stage1snd", "stage1b", "stage1c", "stage1d", "stage1e", nullptr };
static const char *const kStage3Anims[] = { "stage3snd", "stage3a", nullptr };
static const char *const kStage4Anims[] = { "stage4snd", "stage4a", "stage4c", "stage4d", nullptr };
static const char *const kStage5Anims[] = { "stage5snd", "stage5b", "stage5c", nullptr };
// 0x4b1ef8: the clips of each stage (the second stage has none)
static const char *const *const kStageAnims[BuildacarScene::kStages] = { kStage1Anims, nullptr, kStage3Anims, kStage4Anims, kStage5Anims };

enum {
	kTotalParts = 62,
	kVisibleStages = 5,      ///< "Stage %d / %d"
	kHotspotMenuPart = 100,  ///< + slot: the parts in the tray (tooltips)
	kButtonHint = 5,
	kButtonNext = 6,
	kButtonPrev = 7,
	kButtonDemolish = 8,
	kTimerHint = 1,
	kHintInterval = 200,     ///< ms between the blinks of a hint
	kZStage = -10,
	kZPlaced = 5,            ///< + slot
	kZDrag = 100,
	kZMesh = 0x95,
	kZFrame = 0x96,
	kZCar = 0x97,
	kRaceGame = 20,          ///< award table types that are message boxes
	kCarUpgrade = 21,
	kMinFittedForNext = 13   ///< FUN_004245c0: the navigator's next page
};

static const int kPartCount[BuildacarScene::kStages] = { 13, 13, 14, 12, 10 }; // 0x49cc70
static const int kFirstPart[BuildacarScene::kStages] = { 1, 14, 27, 41, 53 };  // 0x49cc84, 1 based

struct PartRect {
	int16 left, top, right, bottom;
	bool contains(int x, int y) const { return x >= left && x < right && y >= top && y < bottom; } // FUN_00412310
};

// 0x49cc98.. (via 0x4b0d30): the parts' places in the tray, also their
// hit rectangles there
static const PartRect kMenuRects[BuildacarScene::kStages][BuildacarScene::kMaxParts] = {
	{ { 550, 487, 786, 531 }, { 330, 570, 506, 586 }, { 241, 490, 269, 574 }, { 390, 484, 438, 552 }, { 338, 487, 382, 547 },
	  { 548, 548, 788, 596 }, { 170, 504, 222, 580 }, { 276, 488, 328, 576 }, { 447, 496, 487, 556 }, { 498, 497, 542, 561 },
	  { 28, 536, 96, 580 }, { 115, 533, 159, 573 }, { 8, 490, 164, 522 }, { 0, 0, 0, 0 } },
	{ { 341, 508, 417, 584 }, { 24, 483, 160, 579 }, { 590, 482, 634, 582 }, { 654, 523, 682, 583 }, { 508, 531, 576, 575 },
	  { 690, 507, 778, 587 }, { 183, 483, 207, 547 }, { 233, 478, 309, 554 }, { 318, 486, 470, 506 }, { 189, 558, 237, 590 },
	  { 463, 516, 487, 580 }, { 643, 477, 691, 509 }, { 499, 478, 571, 522 }, { 0, 0, 0, 0 } },
	{ { 18, 480, 134, 552 }, { 715, 477, 783, 545 }, { 542, 518, 586, 590 }, { 126, 569, 206, 585 }, { 593, 476, 705, 528 },
	  { 697, 552, 781, 592 }, { 452, 515, 520, 591 }, { 233, 491, 305, 587 }, { 53, 556, 97, 588 }, { 155, 479, 223, 555 },
	  { 461, 479, 553, 519 }, { 606, 548, 686, 588 }, { 312, 475, 380, 547 }, { 372, 529, 440, 597 } },
	{ { 387, 492, 515, 536 }, { 55, 491, 147, 535 }, { 299, 546, 391, 586 }, { 645, 491, 725, 543 }, { 239, 489, 367, 553 },
	  { 163, 501, 211, 549 }, { 542, 493, 574, 525 }, { 68, 553, 144, 577 }, { 648, 558, 724, 582 }, { 516, 523, 628, 583 },
	  { 408, 542, 504, 582 }, { 207, 545, 247, 589 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 } },
	{ { 29, 489, 229, 561 }, { 617, 491, 649, 583 }, { 251, 486, 295, 586 }, { 513, 546, 605, 590 }, { 660, 487, 768, 531 },
	  { 539, 489, 579, 533 }, { 328, 483, 344, 587 }, { 379, 486, 491, 530 }, { 374, 543, 486, 591 }, { 705, 536, 753, 588 },
	  { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 } }
};

// 0x49d078.. (via 0x4b0d44): where the parts go on the car: the bitmap's
// position and the rectangle the part must be dropped in
static const PartRect kPlaceRects[BuildacarScene::kStages][BuildacarScene::kMaxParts] = {
	{ { 268, 244, 560, 328 }, { 412, 286, 516, 334 }, { 358, 293, 442, 357 }, { 396, 285, 500, 353 }, { 398, 320, 462, 364 },
	  { 415, 259, 627, 371 }, { 535, 182, 627, 262 }, { 476, 206, 620, 294 }, { 442, 203, 502, 259 }, { 423, 228, 475, 304 },
	  { 581, 230, 657, 282 }, { 291, 306, 319, 334 }, { 130, 321, 338, 425 }, { 0, 0, 0, 0 } },
	{ { 567, 157, 703, 297 }, { 352, 139, 564, 307 }, { 286, 138, 502, 294 }, { 587, 169, 631, 269 }, { 509, 273, 677, 361 },
	  { 502, 147, 622, 347 }, { 461, 238, 509, 346 }, { 379, 254, 523, 418 }, { 481, 242, 733, 438 }, { 580, 238, 704, 302 },
	  { 570, 296, 682, 392 }, { 535, 283, 627, 355 }, { 233, 109, 289, 193 }, { 0, 0, 0, 0 } },
	{ { 239, 131, 391, 275 }, { 495, 148, 639, 304 }, { 493, 109, 741, 313 }, { 536, 161, 760, 205 }, { 233, 125, 589, 381 },
	  { 540, 162, 688, 322 }, { 561, 211, 765, 383 }, { 420, 29, 584, 265 }, { 518, 222, 594, 298 }, { 483, 250, 691, 426 },
	  { 465, 200, 561, 352 }, { 382, 239, 602, 291 }, { 384, 176, 520, 412 }, { 258, 249, 410, 433 } },
	{ { 481, 87, 633, 195 }, { 445, 128, 577, 264 }, { 369, 193, 501, 305 }, { 381, 125, 581, 269 }, { 147, 179, 483, 347 },
	  { 500, 223, 616, 319 }, { 429, 253, 473, 301 }, { 265, 270, 397, 330 }, { 380, 214, 512, 270 }, { 129, 292, 397, 380 },
	  { 289, 286, 441, 362 }, { 154, 242, 238, 306 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 } },
	{ { 233, 175, 545, 347 }, { 296, 248, 416, 312 }, { 228, 173, 536, 289 }, { 132, 224, 244, 300 }, { 244, 126, 412, 214 },
	  { 226, 207, 266, 287 }, { 274, 178, 430, 302 }, { 249, 265, 381, 349 }, { 263, 248, 499, 360 }, { 393, 177, 473, 261 },
	  { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 } }
};

// 0x49d458.. (via 0x4b0d58): the part that must be fitted before, -1 none
static const int8 kDepends[BuildacarScene::kStages][BuildacarScene::kMaxParts] = {
	{ -1, -1, -1, -1, -1, -1, -1, -1, 7, -1, 7, -1, -1, -1 },
	{ -1, -1, 1, -1, -1, -1, -1, -1, 4, 4, 4, 4, -1, -1 },
	{ -1, -1, 4, 4, -1, 4, -1, 4, 4, -1, 4, 4, 4, -1 },
	{ -1, -1, -1, 1, -1, -1, 4, 4, 4, -1, 7, 9, -1, -1 },
	{ -1, 0, 0, -1, -1, 0, 0, 1, -1, 8, -1, -1, -1, -1 }
};

// 0x49d550 / 0x49d588 (via 0x4b0d6c): the part that covers this one; while
// it is not fitted the part shows its "b" bitmap (only part 43, "part_43b")
static const int8 kCovers[BuildacarScene::kStages][BuildacarScene::kMaxParts] = {
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }
};

// 0x4b0d80: what fitting the n-th part brings (FUN_00424420): an award
// (FUN_0041bb20(type, 0, 0, count, flags)) or, for types 20 and 21, a
// message box (buildacar:RACEGAME / CARUPGRADE).
static const struct {
	int16 parts, type, count, flags;
} kAwards[] = {
	{ 4, kAwardImage, 1, 0 }, { 9, kAwardFilmClip, 1, 0 }, { 13, kRaceGame, 0, 0 }, { 17, kAwardImage, 1, 0 },
	{ 20, kAwardCar, 1, 0 }, { 23, kAwardFilmClip, 1, 0 }, { 25, kAwardCar, 1, 0 }, { 26, kCarUpgrade, 0, 0 },
	{ 28, kAwardImage, 1, 0 }, { 31, kAwardFilmClip, 1, 0 }, { 33, kAwardCar, 1, 0 }, { 35, kAwardImage, 1, 0 },
	{ 37, kAwardFilmClip, 1, 0 }, { 39, kAwardCar, 1, 0 }, { 40, kCarUpgrade, 0, 0 }, { 42, kAwardImage, 1, 0 },
	{ 44, kAwardFilmClip, 1, 0 }, { 46, kAwardCar, 1, 0 }, { 48, kAwardImage, 1, 0 }, { 50, kAwardFilmClip, 1, 0 },
	{ 51, kAwardCar, 1, 0 }, { 52, kCarUpgrade, 0, 0 }, { 53, kAwardImage, 1, 0 }, { 54, kAwardFilmClip, 1, 0 },
	{ 55, kAwardImage, 1, 0 }, { 56, kAwardFilmClip, 1, 0 }, { 57, kAwardCar, 1, 0 }, { 58, kAwardImage, 1, 0 },
	{ 59, kAwardFilmClip, 1, 0 }, { 60, kAwardImage, 1, 0 }, { 61, kAwardFilmClip, 1, 0 }, { 62, kCarUpgrade, 0, 0 }
};

// The four text labels (LABEL module, FUN_00407930; rectangles 0x49d5b8..),
// Amerigo BT_10_ centred, z 0.
static const struct {
	const char *name;
	int16 left, top, right, bottom;
} kLabels[] = {
	{ "stagelabel", 0, 455, 114, 472 },    // DAT_005b72b8 buildacar:STAGE
	{ "stagepercent", 115, 455, 332, 472 },// DAT_005b7d60 buildacar:STAGEPRCNT
	{ "totalpercent", 333, 455, 617, 472 },// DAT_005b61d8 buildacar:TOTALPRCNT
	{ "missing", 618, 455, 799, 472 }      // DAT_005b7368 buildacar:MISSING
};

enum {
	kLabelStage,
	kLabelStagePercent,
	kLabelTotalPercent,
	kLabelMissing
};

// The cheat code registered by the init (FUN_0040ad50, no message).
static const char kCheatCode[] = "SHOWMETHECAR";

BuildacarScene::BuildacarScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def),
	_won(0), _fitted(0), _stage(0), _buildStage(0), _fittedMask(0), _lastPart(-1), _hintRunning(false), _blinks(0),
	_drag(nullptr), _dragPart(0), _cheatPos(0), _mesh(nullptr), _solidMask(nullptr), _buttons(this), _tooltip(this) {
}

BuildacarScene::~BuildacarScene() {
	if (_solidMask) {
		_solidMask->free();
		delete _solidMask;
	}
}

// ---- element slots ---------------------------------------------------------

void BuildacarScene::removeSlot(const Slot &s) {
	if (slotAdded(s))
		s.anim->remove();
}

void BuildacarScene::nameSlot(Slot &s, const Common::String &name, int hotspot) {
	s.anim = defineAnim(name.c_str(), false, true, hotspot, 0, 0);
	s.anim->setHotspot(hotspot);
}

// FUN_00412840(slot, -1, -1, z): at the slot's position
void BuildacarScene::addSlot(const Slot &s, int z) {
	if (s.anim)
		s.anim->add(s.x, s.y, z);
}

// ---- init / close ------------------------------------------------------------

// FUN_00423350
void BuildacarScene::onInit(int arg) {
	// FUN_0040ad50("SHOWMETHECAR", FUN_00423d50, 0): typing the code shows
	// the finished car (see onKey()).
	_cheatPos = 0;
	loadState();
	// FUN_0041b540(0) / FUN_0041b690: the stored count is replaced by the
	// parts won so far
	_won = awardWonCount(kAwardCarPart, awardCounter(_vm, kAwardCarPart));
	debug(1, "Buildacar: %d parts won, %d fitted, stage %d, building stage %d (mask 0x%x)",
	      _won, _fitted, _stage, _buildStage, _fittedMask);
	// FUN_004094d0 (subtitles) is not ported.
	_tooltip.start(0);  // FUN_00409b50(0)
	_font.load("Amerigo BT_10_");
	_hintRunning = false;

	// The tray parts are hit tested on their rectangle (FUN_00413670 mode
	// 2); the largest bitmap is 240x48.
	_solidMask = new Graphics::Surface();
	_solidMask->create(256, 64, Graphics::PixelFormat::createFormatCLUT8());
	memset(_solidMask->getPixels(), 1, _solidMask->pitch * _solidMask->h);

	// Audio only clips the original plays as clones (FUN_00412c20)
	static const char *const kClones[] = { "right", "wrong", "notyet", "../../common/sound/cheat", nullptr };
	for (int i = 0; kClones[i]; i++)
		defineAnim(kClones[i], true, false, 0, 0, 0, 0, false);

	createControls();
	// FUN_00423460 / FUN_004234d0 set up the 14 tray and 14 car slots; the
	// latter also reads an obfuscated registry value (FUN_0040b8b0), and
	// sets DAT_005b7d40, an offset into the place tables, to 0 for good.
	setLabel(kLabels[kLabelMissing].name, Common::String::format("%s: %2d", _vm->getString("buildacar:MISSING").c_str(), kTotalParts - _won));
	setStage(_stage);
	playMusic("track11");
}

// FUN_00423dd0
void BuildacarScene::onClose() {
	// FUN_0040ae10 unregisters the cheat codes IAMLAZY (registered nowhere)
	// and SHOWMETHECAR.
	saveState();
	// FUN_0040ba00 deletes the registry value of FUN_00423c50; the font and
	// the labels are freed.
	_tooltip.hide();
	_buttons.removeAll();
	_drag = nullptr;
}

// FUN_00411e70(0, &DAT_005b7d48, 20): {parts won, parts fitted, stage,
// building stage, fitted mask}; zeros without a profile.
void BuildacarScene::loadState() {
	_won = _fitted = _stage = _buildStage = 0;
	_fittedMask = 0;
	byte buf[20];
	if (!_vm->profile() || !_vm->profile()->getRecord(_name, Profile::kRecordValue, 0, buf, sizeof(buf)))
		return;
	_won = READ_LE_INT32(buf);
	_fitted = READ_LE_INT32(buf + 4);
	// The original trusts the stored stages; out of range ones would index
	// past its tables.
	_stage = CLIP<int32>(READ_LE_INT32(buf + 8), 0, kStages - 1);
	_buildStage = CLIP<int32>(READ_LE_INT32(buf + 12), 0, kStages);
	_fittedMask = READ_LE_UINT32(buf + 16);
}

// FUN_00411e40(0, &DAT_005b7d48, 20)
void BuildacarScene::saveState() {
	if (!_vm->profile())
		return;
	byte buf[20];
	WRITE_LE_INT32(buf, _won);
	WRITE_LE_INT32(buf + 4, _fitted);
	WRITE_LE_INT32(buf + 8, _stage);
	WRITE_LE_INT32(buf + 12, _buildStage);
	WRITE_LE_UINT32(buf + 16, _fittedMask);
	_vm->profile()->setRecord(_name, Profile::kRecordValue, 0, buf, sizeof(buf));
	_vm->profileChanged();
}

// FUN_00423c50
void BuildacarScene::createControls() {
	// FUN_0040b940 writes a marker into an obfuscated registry key first.
	const uint32 green = _vm->_screen->format.RGBToColor(0, 255, 0);
	for (int i = 0; i < ARRAYSIZE(kLabels); i++)
		defineSurfaceAnim(kLabels[i].name, kLabels[i].right - kLabels[i].left, kLabels[i].bottom - kLabels[i].top, green)
			->add(kLabels[i].left, kLabels[i].top, 0);

	// BUTTON module (FUN_00406690, z 1): opaque push buttons
	_buttons.add(kButtonHint, 0, 0x1a5, "hint_0", "hint_1", "hint_2", nullptr, "", false);
	_buttons.add(kButtonNext, 0x2eb, 0x1a5, "next_0", "next_1", "next_2", nullptr, "", false);
	_buttons.add(kButtonPrev, 0x2c0, 0x1ac, "prev_0", "prev_1", "prev_2", nullptr, "", false);
	_buttons.add(kButtonDemolish, 0x35, 0x1ac, "demolish_0", "demolish_1", nullptr, nullptr, "", false);
}

// FUN_004079c0(label, font, text, 0x101): the text centred in the label
void BuildacarScene::setLabel(const char *label, const Common::String &text) {
	Graphics::ManagedSurface *s = anim(label)->surface();
	s->fillRect(Common::Rect(0, 0, s->w, s->h), _vm->_screen->format.RGBToColor(0, 255, 0));
	_font.drawStringCentred(*s, Common::Rect(0, 0, s->w, s->h), text);
}

// ---- stages ------------------------------------------------------------------

// FUN_00423500
void BuildacarScene::setStage(int stage) {
	// FUN_00423bc0 / FUN_00423c10: the clips of the old stage go, those of
	// the new one start
	if (kStageAnims[_stage])
		for (int i = 0; kStageAnims[_stage][i]; i++)
			if (isAnimAdded(kStageAnims[_stage][i]))
				removeAnim(kStageAnims[_stage][i]);
	if (kStageAnims[stage])
		for (int i = 0; kStageAnims[stage][i]; i++)
			playAnim(kStageAnims[stage][i]);
	_stage = stage;
	_lastPart = -1;
	showStageBitmap(stage);
	placeMenuParts(stage);
	placeFittedParts(stage);
	// FUN_00423670
	setLabel(kLabels[kLabelStage].name, Common::String::format("%s: %d / %d", _vm->getString("buildacar:STAGE").c_str(), stage + 1, (int)kVisibleStages));
	updatePercentages();
	playNarration(stage);

	// The next button shows the car on the last stage. The original creates
	// button 6 again, which only replaces its bitmaps (FUN_00406540 refuses
	// an existing id); its state is set below anyway.
	_buttons.remove(kButtonNext);
	if (stage < 4)
		_buttons.add(kButtonNext, 0x2eb, 0x1a5, "next_0", "next_1", "next_2", nullptr, "", false);
	else
		_buttons.add(kButtonNext, 0x2eb, 0x1a5, "car_0", "car_1", "car_2", nullptr, "", false);
	bool complete = stageComplete();
	_buttons.setState(kButtonHint, complete ? SceneButtons::kStateDisabled : SceneButtons::kStateNormal);
	_buttons.setState(kButtonPrev, stage < 1 ? SceneButtons::kStateDisabled : SceneButtons::kStateNormal);
	_buttons.setState(kButtonNext, complete ? SceneButtons::kStateNormal : SceneButtons::kStateDisabled);
}

// FUN_00423880: the stage's view of the car, "stage1".."stage5" (800x455)
void BuildacarScene::showStageBitmap(int stage) {
	if (!_stageBitmap.empty() && isAnimAdded(_stageBitmap.c_str()))
		removeAnim(_stageBitmap.c_str());
	_stageBitmap = Common::String::format("stage%d", stage + 1);
	defineAnim(_stageBitmap.c_str(), false, false, 0, 0, 0, kZStage)->add(Anim::kDefaultPos, Anim::kDefaultPos, kZStage);
}

// The parts of a stage the player has won.
int BuildacarScene::availableParts(int stage) const {
	return CLIP<int>(_won - kFirstPart[stage] + 1, 0, kPartCount[stage]);
}

// FUN_004238d0: the won parts not fitted yet lie in the tray
void BuildacarScene::placeMenuParts(int stage) {
	for (int i = 0; i < kMaxParts; i++)
		removeSlot(_menu[i]);
	int n = availableParts(stage);
	for (int i = 0; i < n; i++) {
		Slot &s = _menu[i];
		nameSlot(s, Common::String::format("menu_part_%02d", kFirstPart[stage] + i), kHotspotMenuPart + i);
		s.x = kMenuRects[_stage][i].left;
		s.y = kMenuRects[_stage][i].top;
		if (!isFitted(stage, i)) {
			addSlot(s, 0);
			s.anim->setHitMask(_solidMask);
		}
	}
}

// FUN_004239f0: the fitted parts on the car
void BuildacarScene::placeFittedParts(int stage) {
	for (int i = 0; i < kMaxParts; i++)
		removeSlot(_placed[i]);
	for (int i = 0; i < kPartCount[stage]; i++) {
		_placed[i].x = kPlaceRects[_stage][i].left;
		_placed[i].y = kPlaceRects[_stage][i].top;
		if (isFitted(stage, i))
			showPlaced(i);
	}
}

// FUN_00423850: the parts of the mask are fitted on the stage
bool BuildacarScene::stageDone(int stage, uint32 mask) const {
	if (_buildStage < stage)
		return false;
	if (_buildStage == stage && (_fittedMask & mask) != mask)
		return false;
	return true;
}

// FUN_00423830
bool BuildacarScene::stageComplete() const {
	return stageDone(_stage, (1u << kPartCount[_stage]) - 1);
}

// FUN_00423740: the parts shown on the car
int BuildacarScene::fittedCount() {
	int n = 0;
	for (int i = 0; i < kPartCount[_stage]; i++)
		if (slotAdded(_placed[i]))
			n++;
	return n;
}

// FUN_004236c0: "This stage" and "Total" (assuming the stages before are
// complete), rounded with float constants (1/62 at 0x49d5f8)
void BuildacarScene::updatePercentages() {
	int fitted = fittedCount();
	int total = (int)((double)(kFirstPart[_stage] + fitted - 1) * (double)0.016129032f * 100.0 + 0.5);
	int stage = (int)((double)fitted / kPartCount[_stage] * 100.0 + 0.5);
	setLabel(kLabels[kLabelStagePercent].name, Common::String::format("%s: %3d %%", _vm->getString("buildacar:STAGEPRCNT").c_str(), stage));
	setLabel(kLabels[kLabelTotalPercent].name, Common::String::format("%s: %3d %%", _vm->getString("buildacar:TOTALPRCNT").c_str(), total));
}

// FUN_004235e0: each stage's narration plays once per profile (page value 1:
// the stages heard)
void BuildacarScene::playNarration(int stage) {
	stopNarrations();
	int32 heard = _vm->getSceneValue(_name, 1, 0);
	if (heard <= stage) {
		playAnim(kNarrations[stage]);
		_vm->setSceneValue(_name, 1, stage + 1);
	}
}

// FUN_00423640
void BuildacarScene::stopNarrations() {
	for (int i = 0; kNarrations[i]; i++)
		if (isAnimPlaying(kNarrations[i]))
			removeAnim(kNarrations[i]);
}

// FUN_00424610
bool BuildacarScene::isNarration(const Anim *a) const {
	for (int i = 0; kNarrations[i]; i++)
		if (!strcmp(a->name(), kNarrations[i]))
			return true;
	return false;
}

// ---- the parts on the car ------------------------------------------------------

// FUN_00423ab0: "part_NN", or "part_NNb" while the part covering it is missing
void BuildacarScene::namePlaced(int part) {
	nameSlot(_placed[part], Common::String::format("part_%02d%s", kFirstPart[_stage] + part, isCovered(part) ? "b" : ""), 0);
}

// FUN_00423b00
bool BuildacarScene::isCovered(int part) {
	int c = kCovers[_stage][part];
	return c != -1 && !slotAdded(_placed[c]);
}

// FUN_00423a70
void BuildacarScene::showPlaced(int part) {
	namePlaced(part);
	addSlot(_placed[part], kZPlaced + part);
	refreshCovered(part);
}

// FUN_00424590
void BuildacarScene::hidePlaced(int part) {
	removeSlot(_placed[part]);
	refreshCovered(part);
}

// FUN_00423b40: the parts covered by this one change their bitmap
void BuildacarScene::refreshCovered(int part) {
	for (int i = 0; i < kPartCount[_stage]; i++) {
		if (!slotAdded(_placed[i]) || kCovers[_stage][i] != part)
			continue;
		removeSlot(_placed[i]);
		namePlaced(i);
		addSlot(_placed[i], kZPlaced + i);
	}
}

// ---- input -------------------------------------------------------------------

// FUN_004240a0
void BuildacarScene::onMouseDown(int hotspot, int x, int y) {
	if (isCarShown())
		hideCar();
	if (!_hintRunning) {
		int i = menuPartAt(x, y);
		if (i != -1) {
			_lastPart = i;
			removeSlot(_menu[i]);
			// The part follows the mouse, centred on it (FUN_00412cf0), until
			// the button is released (the EVENT module's drag, FUN_0040c1f0).
			Common::String name = Common::String::format("mouse_part_%02d", kFirstPart[_stage] + i);
			_drag = defineAnim(name.c_str(), false, true, 0, 0, 0, kZDrag);
			_drag->add(x, y, kZDrag);
			Common::Rect r = _drag->rect();
			_drag->setPos(x - r.width() / 2, y - r.height() / 2);
			_dragPart = i;
			_dragMouse = Common::Point(x, y);
			_dragPos = Common::Point(_drag->x(), _drag->y());
			debug(1, "Buildacar: dragging part %d", kFirstPart[_stage] + i);
		}
	}
	// The BUTTON and tooltip modules see the press after the page.
	_tooltip.hide();
	_buttons.mouseDown(hotspot, x, y);
}

// FUN_00424140: the tray part under the mouse (by its rectangle), -1 none
int BuildacarScene::menuPartAt(int x, int y) {
	int n = availableParts(_stage);
	for (int i = 0; i < n; i++)
		if (kMenuRects[_stage][i].contains(x, y) && slotAdded(_menu[i]))
			return i;
	return -1;
}

void BuildacarScene::onMouseUp(int hotspot, int x, int y) {
	// 0x106 goes to the BUTTON module (which reports a click with 0x116),
	// then the EVENT module ends a drag with 0x10b (FUN_0040c260).
	int id = _buttons.mouseUp();
	if (id)
		onButton(id);
	if (_drag)
		endDrag(x, y);
}

void BuildacarScene::onMouseMove(int hotspot, int x, int y) {
	_buttons.setHover(hotspot);
	_tooltip.hotspotChanged(hotspot);
}

void BuildacarScene::onUpdate() {
	// The dragged part keeps its offset to the mouse (EVENT module, 0x109)
	if (_drag) {
		Common::Point m = _vm->mousePos();
		_drag->setPos(_dragPos.x + m.x - _dragMouse.x, _dragPos.y + m.y - _dragMouse.y);
	}
	// FUN_004244d0 (event 0x11a): the tray parts' names
	int h = _tooltip.poll();
	if (h >= kHotspotMenuPart && h < kHotspotMenuPart + kMaxParts) {
		Common::Point p = _vm->mousePos();
		debug(2, "Buildacar: tooltip for hotspot %d at %d,%d", h, p.x, p.y);
		_tooltip.show(awardItemName(_vm, kAwardCarPart, kFirstPart[_stage] - 1 + h - kHotspotMenuPart), p.x, p.y);
	}
}

// FUN_00424210 (event 0x10b): the part is dropped
void BuildacarScene::endDrag(int x, int y) {
	int i = _dragPart;
	_drag->remove();
	_drag = nullptr;
	if (!kPlaceRects[_stage][i].contains(x, y)) {
		addSlot(_menu[i], 0);
		playClone("wrong");
		return;
	}
	int dep = kDepends[_stage][i];
	if (dep != -1 && !slotAdded(_placed[dep])) {
		// Another part must go on first: it blinks.
		addSlot(_menu[i], 0);
		playClone("notyet");
		startHint(dep);
		return;
	}
	fitPart(i);
	playClone("right");
	updatePercentages();
	if (stageComplete()) {
		_buildStage++;
		_fittedMask = 0;
		_buttons.setState(kButtonHint, SceneButtons::kStateDisabled);
		_buttons.setState(kButtonNext, SceneButtons::kStateNormal);
	}
}

// FUN_004243c0
void BuildacarScene::fitPart(int part) {
	_fittedMask |= 1u << part;
	showPlaced(part);
	int n = fittedCount() - 1 + kFirstPart[_stage];
	debug(1, "Buildacar: part %d fitted, %d in all", kFirstPart[_stage] + part, n);
	if (_fitted < n) {
		giveAwards(_fitted + 1, n - _fitted);
		_fitted = n;
	}
}

// FUN_00424420 / FUN_004244b0
void BuildacarScene::giveAwards(int from, int count) {
	for (int p = from; p < from + count; p++) {
		for (int j = 0; j < ARRAYSIZE(kAwards); j++) {
			if (kAwards[j].parts != p)
				continue;
			int type = kAwards[j].type;
			if (type == kRaceGame || type == kCarUpgrade)
				_vm->messageBox("buildacar:AWARD", type == kRaceGame ? "buildacar:RACEGAME" : "buildacar:CARUPGRADE", MessageBox::kButtonOk);
			else
				showAward(_vm, type, "", "", kAwards[j].count, kAwards[j].flags);
			break;
		}
	}
}

// FUN_004245c0: on to the TV station once 13 parts were fitted
void BuildacarScene::onRightClick(int x, int y) {
	_tooltip.hide();
	_vm->showNavigator(_fitted >= kMinFittedForNext ? "tvstation" : nullptr, "intent");
}

// FUN_00424630
void BuildacarScene::onKey(const Common::KeyState &key) {
	if (key.keycode == Common::KEYCODE_SPACE)
		stopNarrations();
	// The global key handler feeds the cheat codes (FUN_0040d300 ->
	// FUN_0040ae60).
	feedCheat(key);
}

// FUN_0040ae60 for the one code of the page: virtual key codes (upper case
// letters) are compared one by one, a wrong key starts over.
void BuildacarScene::feedCheat(const Common::KeyState &key) {
	int vk = key.keycode;
	if (vk >= Common::KEYCODE_a && vk <= Common::KEYCODE_z)
		vk = 'A' + (vk - Common::KEYCODE_a);
	if (vk == kCheatCode[_cheatPos])
		_cheatPos++;
	else
		_cheatPos = 0;
	if (kCheatCode[_cheatPos])
		return;
	_cheatPos = 0;
	debug(1, "Buildacar: cheat %s", kCheatCode);
	playClone("../../common/sound/cheat");  // FUN_0040ae50
	showCar();                               // FUN_00423d50
}

// FUN_004245f0
void BuildacarScene::onAnimFinished(Anim *a) {
	if (isNarration(a))
		a->remove();
}

// ---- buttons (event 0x116, FUN_00423e40) -----------------------------------------

void BuildacarScene::onButton(int id) {
	switch (id) {
	case kButtonHint:
		hint();
		break;
	case kButtonNext:
		changeStage(1);
		break;
	case kButtonPrev:
		changeStage(-1);
		break;
	case kButtonDemolish:
		demolish();
		break;
	default:
		break;
	}
}

// FUN_00423e90: past the last stage the finished car is shown
void BuildacarScene::changeStage(int delta) {
	int s = _stage + delta;
	if (s < 0) {
		s = 0;
	} else if (s >= kStages) {
		showCar();
		return;
	}
	if (s != _stage)
		setStage(s);
}

// FUN_00423ec0: the part last picked up (or the one it needs) blinks in
// its place, else the next part to fit
void BuildacarScene::hint() {
	if (_lastPart >= 0 && _lastPart < kPartCount[_stage] && !slotAdded(_placed[_lastPart])) {
		int dep = kDepends[_stage][_lastPart];
		if (dep != -1 && !slotAdded(_placed[dep]))
			startHint(dep);
		else
			startHint(_lastPart);
		return;
	}
	// FUN_00423fb0
	int i = nextHintPart();
	if (i != -1)
		startHint(i);
}

// FUN_00423fd0: the first won part not on the car (or the part it needs)
int BuildacarScene::nextHintPart() {
	int n = availableParts(_stage);
	for (int i = 0; i < n; i++) {
		int dep = kDepends[_stage][i];
		if (dep != -1 && !slotAdded(_placed[dep]))
			return dep;
		if (!slotAdded(_placed[i]))
			return i;
	}
	return -1;
}

// FUN_00423f60: a timer every 200 ms from now (FUN_0040c480(now, 200,
// part)) shows and hides the part three times; no drags meanwhile
void BuildacarScene::startHint(int part) {
	namePlaced(part);
	_blinks = 0;
	setTimer(kTimerHint, 0, part);
	_hintRunning = true;
	_buttons.setState(kButtonHint, SceneButtons::kStateDisabled);
}

// FUN_00424510
void BuildacarScene::onTimer(int id, int data) {
	if (id != kTimerHint)
		return;
	if (!slotAdded(_placed[data])) {
		showPlaced(data);
	} else {
		hidePlaced(data);
		_blinks++;
	}
	if (_blinks > 2) {
		_hintRunning = false;
		_buttons.setState(kButtonHint, SceneButtons::kStateNormal);
	} else {
		setTimer(kTimerHint, kHintInterval, data);
	}
}

// FUN_00424060: everything comes off; the parts fitted before still count
// for the awards and the navigator
void BuildacarScene::demolish() {
	int r = _vm->messageBox("interfaceh:WARNING", "buildacar:DEMOLISH", MessageBox::kButtonYes | MessageBox::kButtonNo);
	if (r != MessageBox::kButtonYes)
		return;
	_buildStage = 0;
	_fittedMask = 0;
	setStage(0);
}

// ---- the finished car ----------------------------------------------------------

// FUN_00423db0
bool BuildacarScene::isCarShown() {
	return isAnimPlaying("iltempo");
}

// FUN_00423d50: Il Tempo Gigante in a frame over the dimmed page, with a
// fanfare (the next button of the last stage and the cheat code)
void BuildacarScene::showCar() {
	if (isCarShown())
		return;
	stopNarrations();
	_vm->_music->pause(true);  // FUN_0040b6f0
	addMesh(kZMesh);           // FUN_00407110(0, 0x95)
	addAnim("frame", Anim::kDefaultPos, Anim::kDefaultPos, kZFrame);
	addAnim("iltempo", Anim::kDefaultPos, Anim::kDefaultPos, kZCar);
	anim("iltempo")->play();
	playAnim("fanfare2");
}

// FUN_004241d0: a click takes it away again
void BuildacarScene::hideCar() {
	if (!isCarShown())
		return;
	removeAnim("frame");
	removeAnim("iltempo");
	removeAnim("fanfare2");
	removeMesh();               // FUN_00407210
	_vm->_music->pause(false);  // FUN_0040b720
}

// FUN_00412c20: the original plays a copy of the element, so the same
// sound can overlap; here it starts again.
void BuildacarScene::playClone(const char *name) {
	Anim *a = anim(name);
	a->remove();
	a->play();
	a->setRemoveWhenDone(true);
}

// FUN_00407110 / FUN_00407210: the checkerboard mesh that dims the page
// (the same as the dialogs' and the town page's).
void BuildacarScene::addMesh(int z) {
	if (!_mesh) {
		const uint32 green = _vm->_screen->format.RGBToColor(0, 255, 0);
		const uint32 dark = _vm->_screen->format.RGBToColor(0x20, 0x20, 0x40);
		_mesh = defineSurfaceAnim("mesh", kScreenWidth, kScreenHeight, green);
		Graphics::ManagedSurface *s = _mesh->surface();
		for (int y = 0; y < kScreenHeight; y++)
			for (int x = (y & 1) ? 0 : 1; x < kScreenWidth; x += 2)
				s->setPixel(x, y, dark);
	}
	if (!_mesh->isAdded())
		_mesh->add(Anim::kDefaultPos, Anim::kDefaultPos, z);
}

void BuildacarScene::removeMesh() {
	if (_mesh && _mesh->isAdded())
		_mesh->remove();
}

} // End of namespace Flaaklypa
