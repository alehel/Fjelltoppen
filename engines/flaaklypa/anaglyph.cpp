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
#include "common/textconsole.h"

#include "flaaklypa/anaglyph.h"
#include "flaaklypa/award.h"
#include "flaaklypa/dialog.h"
#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/listdlg.h"
#include "flaaklypa/music.h"
#include "flaaklypa/resources.h"

namespace Flaaklypa {

// "3D-titter" (handler FUN_0047c630, code 0x47c630..0x47e240): 0x103
// FUN_0047c710, 0x104 FUN_0047d950, 0x105 FUN_0047d9e0, 0x10f FUN_0047da50,
// 0x110 FUN_0047da70, 0x116 FUN_0047de90, 0x117 FUN_0047de70.

enum {
	// buttons (FUN_0047c740); the scroll module numbers its arrows from 2000
	kButtonHelp = 1,
	kButtonExit = 2,
	kButtonPlay = 3,      ///< START; in the rubber ball game a press hits the ball
	kButtonLoad = 4,      ///< "Åpne 3D-aktivitet"
	kButtonStop = 5,      ///< STOP; the high score table in the rubber ball game
	kButtonPrint = 7,
	kButtonFaster = 8,
	kButtonSlower = 9,
	kScrollBar = 0xc,     ///< the scroll bar's id, its thumb's hotspot
	kButtonUp = 2000,
	kButtonDown = 2001,

	// z orders
	kZBackground = -10,
	kZWindmill = -9,
	kZLabel = 0x14,
	kZCorner = 0x1d,
	kZButton = 0x1e,
	kZClip = 0x19,
	kZList = 0,           ///< DAT_005385a8, the list box module's z (0 unless set)

	// the forced frame rate (FUN_0047cf20): the clips are 15 fps
	kNormalFps = 15,
	kMaxFps = 30,

	// the chooser's rows (FUN_0047d880 / FUN_00408470)
	kChooserRowW = 0xee,
	kChooserRowH = 0x17,
	kChooserX = 0xf0,
	kChooserY = 10,

	k3DScenesKey = 100    ///< the profile value with a bit per 3D scene found (FUN_0041b8c0)
};

// The activities' ids (+0x00 of the table): the story pages' 3D scene
// numbers, then the five that are always open.
enum {
	kActYard = 1,
	kActPee = 3,
	kActHouse = 4,
	kActTvroom = 5,
	kActOuttent = 8,
	kActIntent = 9,
	kActOutro = 14,
	kActPingpong = 15,
	kActRubberball = 16,
	kActCars = 17,
	kActGallery = 18,
	kActJigsaw = 19
};

// The rubber ball clips (FUN_0047dce0 / FUN_0047d5b0)
enum {
	kBallIdle = 0,
	kBallStart = 1,
	kBallHit = 2,
	kBallMiss = 3,
	kBallPickUp = 4
};

// Layout (macros: constant Rect objects would need global constructors)
#define kRateTitleRect Common::Rect(717, 566, 788, 583)   // 0x4a0320: "FART" / "TREFF"
#define kRateValueRect Common::Rect(721, 515, 773, 550)   // 0x4a0330: the speed or the hits
#define kListRect Common::Rect(12, 512, 215, 587)         // 0x4a0340
#define kScrollTrack Common::Rect(221, 540, 235, 555)     // FUN_004090e0(0xc, 0xdd, 0x21c, 0xe, 0xf)
// The label at 0x4a0310, (244,514)-(705,535), is created but never written.

static const uint32 kListBar = 0x23234b;   // COLORREF of the list's selection bar (FUN_00407d90)

// The rows of the list (+0x0c of the table, 0x50d300..0x50d3ac)
static const char *const kYardOptions[] = { "anaglyph:PLAYALL", "anaglyph:YARD1", "anaglyph:YARD2", "anaglyph:YARD3", nullptr };
static const char *const kPeeOptions[] = { "anaglyph:PLAYALL", "anaglyph:PEE1", "anaglyph:PEE2", "anaglyph:PEE3", nullptr };
static const char *const kHouseOptions[] = { "anaglyph:SCENE_HOUSE", nullptr };
static const char *const kTvroomOptions[] = { "anaglyph:PLAYALL", "anaglyph:TVROOM1", "anaglyph:TVROOM2", "anaglyph:TVROOM3", nullptr };
static const char *const kOuttentOptions[] = { "anaglyph:SCENE_OUTTENT", nullptr };
static const char *const kIntentOptions[] = { "anaglyph:SCENE_INTENT", nullptr };
static const char *const kOutroOptions[] = { "anaglyph:SCENE_OUTRO", nullptr };
static const char *const kPingpongOptions[] = {
	"anaglyph:PINGPONG0", "anaglyph:PINGPONG1", "anaglyph:PINGPONG2", "anaglyph:PINGPONG3",
	"anaglyph:PINGPONG4", "anaglyph:PINGPONG5", "anaglyph:PINGPONG6", nullptr
};
static const char *const kRubberballOptions[] = { "anaglyph:RUBBERBALL0", "anaglyph:RUBBERBALL1", nullptr };
static const char *const kCarsOptions[] = { "anaglyph:CARS0", "anaglyph:CARS1", "anaglyph:CARS2", nullptr };
static const char *const kGalleryOptions[] = {
	"anaglyph:GALLERY0", "anaglyph:GALLERY1", "anaglyph:GALLERY2", "anaglyph:GALLERY3", "anaglyph:GALLERY4", nullptr
};

// The activity table (0x50d3b0, 12 entries of 0x10 bytes), the chooser's order
const AnaglyphScene::Activity AnaglyphScene::kActivities[] = {
	{ kActPingpong, "anaglyph:SCENE_PINGPONG", -1, kPingpongOptions },
	{ kActRubberball, "anaglyph:SCENE_RUBBERBALL", -1, kRubberballOptions },
	{ kActGallery, "anaglyph:SCENE_GALLERY", -1, kGalleryOptions },
	{ kActCars, "anaglyph:SCENE_CARS", -1, kCarsOptions },
	{ kActJigsaw, "anaglyph:SCENE_3DJIGSAW", -1, nullptr },
	{ kActYard, "anaglyph:SCENE_YARD", 1, kYardOptions },
	{ kActPee, "anaglyph:SCENE_PEE", 3, kPeeOptions },
	{ kActHouse, "anaglyph:SCENE_HOUSE", 4, kHouseOptions },
	{ kActTvroom, "anaglyph:SCENE_TVROOM", 5, kTvroomOptions },
	{ kActOuttent, "anaglyph:SCENE_OUTTENT", 8, kOuttentOptions },
	{ kActIntent, "anaglyph:SCENE_INTENT", 9, kIntentOptions },
	{ kActOutro, "anaglyph:SCENE_OUTRO", 14, kOutroOptions }
};

// The element lists (NULL terminated in the original, kNoElem here)
typedef AnaglyphScene E;

// 0x510a40: every element but the corners and the music, in table order
static const int kAllList[] = {
	E::kRbIdle1, E::kRbIdle2, E::kRbIdle3, E::kRbStartCenter,
	E::kRbHitCenterCenter, E::kRbHitCenterDown, E::kRbHitCenterUp,
	E::kRbHitLeftCenter, E::kRbHitLeftDown, E::kRbHitLeftUp,
	E::kRbHitRightCenter, E::kRbHitRightDown, E::kRbHitRightUp,
	E::kRbNotHitCenter, E::kRbPickUp, E::kRubberball,
	E::kPingpong0, E::kPingpong1, E::kPingpong2, E::kPingpong4, E::kPingpong5, E::kPingpong7, E::kPingpong8,
	E::kPingpong1Bmp, E::kPingpong2Bmp, E::kPingpong4Bmp, E::kPingpong5Bmp, E::kPingpong7Bmp, E::kPingpong8Bmp,
	E::kCarsIltempo, E::kCarsBoom, E::kCarsRolls, E::kCars,
	E::kGallery1, E::kGallery2, E::kGallery4, E::kGallery5, E::kGallery6,
	E::kCharacter01, E::kCharacter02, E::kCharacter04, E::kCharacter05, E::kCharacter06,
	E::kYardShock, E::kYardSolan, E::kYardDrown, E::kYardPump, E::kYardWindmill, E::kYard, E::kYardBirds,
	E::kPeeLudvig, E::kPeeMagpie, E::kPeeSolan1, E::kPeeSolan2, E::kPee, E::kPeeBirds,
	E::kHouseClip, E::kHouseWindmill, E::kHouse,
	E::kTvroomBoxing, E::kTvroomExercise, E::kTvroomLaugh, E::kTvroomTwist, E::kTvroomLudvig, E::kTvroomCandles, E::kTvroom,
	E::kOuttentClip, E::kOuttent, E::kIntentClip, E::kIntent,
	E::kOutroClip, E::kOutroShot, E::kOutroWindmill, E::kOutro,
	E::kNoElem
};
// 0x510b6c: the music (no music for ping-pong, the intent and the outro)
static const int kMusicList[] = {
	E::kRbMusic, E::kCarsMusic, E::kGalleryMusic, E::kYardMusic, E::kPeeMusic, E::kHouseMusic,
	E::kTvroomMusic, E::kOuttentMusic, E::kNoElem
};
static const int kIdleList[] = { E::kRbIdle1, E::kRbIdle2, E::kRbIdle3, E::kNoElem };   // 0x510b90
static const int kHitList[] = {                                                        // 0x510ba0
	E::kRbHitCenterCenter, E::kRbHitCenterDown, E::kRbHitCenterUp,
	E::kRbHitLeftCenter, E::kRbHitLeftDown, E::kRbHitLeftUp,
	E::kRbHitRightCenter, E::kRbHitRightDown, E::kRbHitRightUp, E::kNoElem
};
static const int kYardList[] = { E::kYardShock, E::kYardSolan, E::kYardDrown, E::kNoElem };        // 0x510bc8
static const int kPeeList[] = { E::kPeeLudvig, E::kPeeMagpie, E::kPeeSolan1, E::kPeeSolan2, E::kNoElem };  // 0x510bd8
static const int kTvroomList[] = { E::kTvroomLaugh, E::kTvroomTwist, E::kTvroomBoxing, E::kTvroomExercise, E::kNoElem };  // 0x510bec
static const int kCarsList[] = { E::kCarsIltempo, E::kCarsBoom, E::kCarsRolls, E::kNoElem };      // 0x510c00

int AnaglyphScene::_idleCount = 0;
int AnaglyphScene::_idleMask = 0;
int AnaglyphScene::_hitCount = 0;
int AnaglyphScene::_hitMask = 0;

// Key colour of opaque procedural elements (see button.cpp)
static const uint32 kNoKey = 0;

// FUN_0040a150(&count, &mask, n): the first n picks in order, then at random
// among those not picked since the mask was last full
static int pickRandom(FlaaklypaEngine *vm, int &count, int &mask, int n) {
	if (mask == (1 << n) - 1)
		mask = 0;
	int pick = count;
	if (pick >= n) {
		int free = 0;
		for (int i = 0; i < n; i++)
			if (!(mask & (1 << i)))
				free++;
		int r = free > 0 ? (int)vm->getRandomNumber(free - 1) : 0;
		pick = mask;  // FUN_0040a1e0 returns the mask when nothing is free (cannot happen)
		for (int i = 0; i < n; i++) {
			if (mask & (1 << i))
				continue;
			if (r < 1) {
				pick = i;
				break;
			}
			r--;
		}
	}
	mask |= 1 << pick;
	count++;
	return pick;
}

static int indexIn(const int *list, int e) {
	for (int i = 0; list[i] != E::kNoElem; i++)
		if (list[i] == e)
			return i;
	return -1;
}

AnaglyphScene::AnaglyphScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def), _buttons(this),
	_activity(0), _option(0), _finished(false), _fps(0), _musicVolume(255), _leftDown(false), _lastMouseY(0),
	_hits(0), _target(0), _presses(0), _pressTime(0), _scores(0) {
	_highlights[0] = _highlights[1] = _highlights[2] = -1;
	// The scene table names several clips and bitmaps alike ("house" is the
	// clip and the bitmap under it, likewise outtent, intent, outro and
	// pingpong_N): Scene::anim() finds the first, the others get elements
	// of their own, so that each structure of the original has one.
	int n = 0;
	for (const AnimDef *d = def->anims; d && d->name && n < kElemCount; d++, n++) {
		Anim *a = anim(d->name);
		if (a->def() != d) {
			a = new Anim(this, d);
			_ownAnims.push_back(a);
		}
		_elems[n] = a;
	}
	if (n != kElemCount)
		error("Anaglyph: the scene table has %d elements, expected %d", n, (int)kElemCount);
}

AnaglyphScene::~AnaglyphScene() {
	for (auto *a : _ownAnims) {
		a->remove();
		delete a;
	}
}

int AnaglyphScene::elemIndex(const Anim *a) const {
	for (int i = 0; i < kElemCount; i++)
		if (_elems[i] == a)
			return i;
	return kNoElem;
}

bool AnaglyphScene::inList(const int *list, const Anim *a) const {
	int e = elemIndex(a);
	return e != kNoElem && indexIn(list, e) >= 0;
}

// FUN_00412840(anim, -1, -1, z)
void AnaglyphScene::addElem(int e, int z) {
	elem(e)->add(Anim::kDefaultPos, Anim::kDefaultPos, z);
}

// FUN_00412840(anim, -1, -1, anim->zOrder)
void AnaglyphScene::addElemDefault(int e) {
	addElem(e, elem(e)->def()->zOrder);
}

// FUN_00412b90
void AnaglyphScene::playElem(int e) {
	if (e != kNoElem)
		elem(e)->play();
}

// ---- init / close ------------------------------------------------------------

// FUN_0047c710
void AnaglyphScene::onInit(int arg) {
	_vm->_music->stop();

	// Development aid: "test_award_anaglyph=<mask>" (the preset of the
	// test_award profile) also sets the 3D scenes the active profile found.
	if (ConfMan.hasKey("test_award_anaglyph") && _vm->profile())
		_vm->setSceneValue(_name, k3DScenesKey, (int32)strtoul(ConfMan.get("test_award_anaglyph").c_str(), nullptr, 0));

	// FUN_0040b3a0: the music level of the scene's sound.ini (0.27); the
	// other clips play at full volume, as everywhere in the port
	Common::INIFile ini;
	Common::String v;
	if (resources()->loadIni(_name, "sound.ini", ini) && ini.getKey("music", "Volume", v))
		_musicVolume = CLIP((int)(atof(v.c_str()) * 255), 0, 255);

	createControls();
	_scores = 0;
	selectActivity(0);
	chooseActivity();
}

// FUN_0047d950: the forced frame rate ends (FUN_00402f40(-1)) and the
// activity shown goes to the profile's value 0, which nothing reads
void AnaglyphScene::onClose() {
	_vm->setSceneValue(_name, 0, _activity);
}

// FUN_0047c740
void AnaglyphScene::createControls() {
	_font10.load("Amerigo BT_10_");
	_font12.load("Amerigo BT_12_");
	_font12Grey.load("Amerigo BT_12_grey");

	_buttons.setDefaultZ(kZButton);
	_buttons.setFont(&_font10);
	_buttons.add(kButtonHelp, 0, 0, "help_0", "help_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonExit, 0x2d8, 0, "exit_0", "exit_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonLoad, 0xf2, 0x1ff, "buttonlong_0", "buttonlong_1", nullptr, nullptr, _vm->getString("anaglyph:LOAD"), false);
	_buttons.add(kButtonPlay, 0xf4, 0x229, "button_0", "button_1", nullptr, nullptr, _vm->getString("anaglyph:PLAY"), false);
	_buttons.add(kButtonStop, 0x19b, 0x229, "button_0", "button_1", nullptr, nullptr, _vm->getString("anaglyph:STOP"), false);
	_buttons.add(kButtonPrint, 0x242, 0x229, "button_0", "button_1", nullptr, nullptr, _vm->getString("anaglyph:PRINT"), false);
	// The backdrop shows the speed arrows; only the pressed and the
	// disabled bitmaps exist (the hit mask holds 8 and 9 there)
	_buttons.add(kButtonFaster, 0x305, 0x203, nullptr, "sizeinc_1", "sizeinc_2", nullptr, "", false);
	_buttons.add(kButtonSlower, 0x305, 0x216, nullptr, "sizedec_1", "sizedec_2", nullptr, "", false);
	addElem(kHelp2, kZCorner);
	addElem(kExit2, kZCorner);

	// The list box (FUN_00408720, rows rendered from strings in Amerigo
	// BT_10_) and its scroll bar 0xc: track (221,540) 14x15, thumb
	// "scrolltab", arrows hup (221,510) and hdown (221,569), opaque
	_list.init(kListRect, kListBar);
	const Graphics::ManagedSurface *thumb = _buttons.bitmap("scrolltab");
	_scroll.init(kScrollTrack, thumb ? thumb->w : 0, thumb ? thumb->h : 0);
	_buttons.add(kButtonUp, 0xdd, 0x1fe, "hup_0", "hup_1", nullptr, nullptr, "", false);
	_buttons.add(kButtonDown, 0xdd, 0x239, "hdown_0", "hdown_1", nullptr, nullptr, "", false);
	showThumb();
}

// FUN_00407930 / FUN_004079c0: a label, the text centred (0x101) in Amerigo BT_10_
void AnaglyphScene::drawLabel(const char *name, const Common::Rect &r, const Common::String &text) {
	const uint32 green = _vm->_screen->format.RGBToColor(0, 0xff, 0);
	Anim *a = defineSurfaceAnim(name, r.width(), r.height(), green);
	_font10.drawStringCentred(*a->surface(), Common::Rect(r.width(), r.height()), text);
	if (!a->isAdded())
		a->add(r.left, r.top, kZLabel);
}

void AnaglyphScene::redrawList() {
	const Common::Rect r = kListRect;
	Anim *a = defineSurfaceAnim("list", r.width(), r.height(), kNoKey);
	if (!a->isAdded())
		a->add(r.left, r.top, kZList);
	Graphics::ManagedSurface *s = a->surface();
	s->fillRect(Common::Rect(s->w, s->h), kNoKey);
	_list.draw(*s, -r.left, -r.top);
}

void AnaglyphScene::showThumb() {
	const Common::Rect r = _scroll.thumbRect();
	_buttons.showBitmap("thumb", "scrolltab", false, kScrollBar, r.left, r.top, kZButton);
}

// Event 0x117 (FUN_0047de70): the list's top follows the scroll bar
void AnaglyphScene::scrolled() {
	showThumb();
	_list.setTop(_scroll.value());
	redrawList();
}

// FUN_0047cec0: the speed in percent of 15 fps
void AnaglyphScene::showRate() {
	const double percent = (double)_fps * (double)0.0666667f * 100.0;   // 0x4a0350, 0x49cc68
	drawLabel("ratevalue", kRateValueRect, Common::String::format("%3.0f %%", percent));
}

// FUN_0047ce80
void AnaglyphScene::showHits() {
	drawLabel("ratevalue", kRateValueRect, Common::String::format("%d", _hits));
}

// ---- the activities ------------------------------------------------------------

// FUN_0047d800 / FUN_0047d880: the list dialog at (240,10) with a row per
// activity, grey while its 3D glasses are not found
void AnaglyphScene::chooseActivity() {
	const Graphics::PixelFormat &format = _vm->_screen->format;
	const uint32 green = format.RGBToColor(0, 0xff, 0);
	Common::Array<Graphics::ManagedSurface *> rows;
	for (const Activity &act : kActivities) {
		const bool locked = act.scene3D >= 0 && !award3DSceneFound(_vm, act.scene3D);
		Graphics::ManagedSurface *row = new Graphics::ManagedSurface(kChooserRowW, kChooserRowH, format);
		row->fillRect(Common::Rect(kChooserRowW, kChooserRowH), green);
		(locked ? _font12Grey : _font12).drawString(*row, 0, 0, _vm->getString(act.title));
		rows.push_back(row);
	}
	const int row = runListDialog(_vm, "anaglyph:LOADTITLE", rows, Common::Array<int>(), kListDialogCancel, nullptr, kChooserX, kChooserY);
	for (auto *r : rows)
		delete r;
	if (row < 0 || row >= (int)ARRAYSIZE(kActivities))
		return;
	const Activity &act = kActivities[row];
	if (act.scene3D >= 0 && !award3DSceneFound(_vm, act.scene3D)) {
		_vm->messageBox("anaglyph:NOTAVAILABLE", "anaglyph:NOTAVAILABLEDESC", MessageBox::kButtonOk);
		return;
	}
	if (act.id == kActJigsaw) {
		// The jigsaw with the 3D images; it comes back here
		_vm->changeScene("jigsaw", kAward3DImage);
		return;
	}
	selectActivity(row);
}

// FUN_0047c9e0
void AnaglyphScene::selectActivity(int index) {
	_activity = index;
	const Activity &act = activity();
	debug(1, "Anaglyph: activity %d (%s)", act.id, act.title);

	// The list: a row per option in Amerigo BT_10_ (FUN_004087f0: the text's
	// extent, cut to the box), the scroll bar's range the rows not shown
	_list.clear();
	const Graphics::PixelFormat &format = _vm->_screen->format;
	const uint32 green = format.RGBToColor(0, 0xff, 0);
	int count = 0;
	for (; act.options && act.options[count]; count++) {
		const Common::String text = _vm->getString(act.options[count]);
		const int w = CLIP(_font10.stringWidth(text), 1, (int)kListRect.width());
		const int h = CLIP(_font10.height(), 1, (int)kListRect.height());
		Graphics::ManagedSurface *row = new Graphics::ManagedSurface(w, h, format);
		row->fillRect(Common::Rect(w, h), green);
		_font10.drawString(*row, 0, 0, text);
		_list.addItem(row);
	}
	_scroll.setRange(_list.count() - _list.rows());

	removeAll();
	removeList(kMusicList);
	drawLabel("ratetitle", kRateTitleRect, _vm->getString("anaglyph:FRAMERATE"));
	setRate(kNormalFps);
	showRate();
	_buttons.setFont(&_font10);
	_buttons.setLabel(kButtonStop, _vm->getString("anaglyph:STOP"));
	_buttons.setState(kButtonFaster, SceneButtons::kStateNormal);
	_buttons.setState(kButtonSlower, SceneButtons::kStateNormal);

	int music = kNoElem;
	switch (act.id) {
	case kActYard:
		addElem(kYard, kZBackground);
		addElem(kYardWindmill, kZWindmill);
		addElem(kYardBirds, kZBackground);
		addElem(kYardMusic, kZBackground);
		music = kYardMusic;
		break;
	case kActPee:
		addElem(kPee, kZBackground);
		addElem(kPeeBirds, kZBackground);
		addElem(kPeeMusic, kZBackground);
		music = kPeeMusic;
		break;
	case kActHouse:
		addElem(kHouse, kZBackground);
		addElem(kHouseWindmill, kZWindmill);
		addElem(kHouseMusic, kZBackground);
		music = kHouseMusic;
		break;
	case kActTvroom:
		addElem(kTvroomLudvig, kZWindmill);
		addElem(kTvroomCandles, kZWindmill);
		addElem(kTvroom, kZBackground);
		addElem(kTvroomMusic, kZBackground);
		music = kTvroomMusic;
		break;
	case kActOuttent:
		addElem(kOuttent, kZBackground);
		addElem(kOuttentMusic, kZBackground);
		music = kOuttentMusic;
		break;
	case kActIntent:
		addElem(kIntent, kZBackground);
		break;
	case kActOutro:
		addElem(kOutro, kZBackground);
		addElem(kOutroWindmill, kZWindmill);
		break;
	case kActRubberball:
		// The value shows the hits, button 5 the table; no speed
		drawLabel("ratetitle", kRateTitleRect, _vm->getString("anaglyph:HIT"));
		_hits = 0;
		showHits();
		_buttons.setLabel(kButtonStop, _vm->getString("anaglyph:HISCORE"));
		_buttons.setState(kButtonFaster, SceneButtons::kStateDisabled);
		_buttons.setState(kButtonSlower, SceneButtons::kStateDisabled);
		addElem(kRubberball, kZBackground);
		addElem(kRbMusic, kZBackground);
		music = kRbMusic;
		break;
	case kActCars:
		addElem(kCars, kZBackground);
		addElem(kCarsMusic, kZBackground);
		music = kCarsMusic;
		break;
	case kActGallery:
		addElem(kGalleryMusic, kZBackground);
		music = kGalleryMusic;
		break;
	default:
		break;
	}
	if (music != kNoElem) {
		// FUN_00403620(player, FUN_0040b3a0(0), 0): track 0 at the music volume
		elem(music)->setVolume(_musicVolume);
		playElem(music);
	}
	if (count > 0)
		_list.select(0);
	redrawList();
	selectOption(0);
}

// FUN_0047d090: a row of the list; the clips wait for START (only the
// rubber ball game starts at once)
void AnaglyphScene::selectOption(int option) {
	_option = option;
	_finished = false;
	stopUnlessBall();
	switch (activityId()) {
	case kActYard:
		// FUN_0047d180
		removeList(kYardList);
		if (option == 0 || option == 1) {
			addElemDefault(kYardShock);
		} else if (option == 2) {
			addElemDefault(kYardPump);
			addElemDefault(kYardSolan);
		} else if (option == 3) {
			addElemDefault(kYardDrown);
		}
		break;
	case kActPee:
		// FUN_0047d220
		removeList(kPeeList);
		if (option == 0 || option == 1)
			addElemDefault(kPeeLudvig);
		else if (option == 2)
			addElemDefault(kPeeSolan1);
		else if (option == 3)
			addElemDefault(kPeeSolan2);
		break;
	case kActHouse:
		// FUN_0047d2a0
		if (option == 0)
			addElem(kHouseClip, kZClip);
		break;
	case kActTvroom:
		// FUN_0047d2c0
		removeList(kTvroomList);
		if (option == 0 || option == 1)
			addElemDefault(kTvroomLaugh);
		else if (option == 2)
			addElemDefault(kTvroomTwist);
		else if (option == 3)
			addElemDefault(kTvroomBoxing);
		else if (option == 4)
			addElemDefault(kTvroomExercise);
		break;
	case kActOuttent:
		// FUN_0047d370
		if (option == 0)
			addElem(kOuttentClip, kZClip);
		break;
	case kActIntent:
		// FUN_0047d390
		if (option == 0)
			addElem(kIntentClip, kZClip);
		break;
	case kActOutro:
		// FUN_0047d3b0
		if (option == 0) {
			addElem(kOutroClip, kZClip);
			addElem(kOutroShot, kZClip);
		}
		break;
	case kActPingpong: {
		// FUN_0047d450: camera 0 circles, cameras 1..6 are clips over a still
		static const int kClips[7] = { kPingpong0, kPingpong1, kPingpong2, kPingpong4, kPingpong5, kPingpong7, kPingpong8 };
		static const int kStills[7] = { kNoElem, kPingpong1Bmp, kPingpong2Bmp, kPingpong4Bmp, kPingpong5Bmp, kPingpong7Bmp, kPingpong8Bmp };
		removeAll();
		if (option >= 0 && option < 7) {
			addElem(kClips[option], kZClip);
			if (kStills[option] != kNoElem)
				addElem(kStills[option], kZBackground);
		}
		break;
	}
	case kActRubberball:
		// FUN_0047d580 / FUN_0047d660
		removeList(kIdleList);
		removeList(kHitList);
		if (elem(kRbStartCenter)->isAdded())
			elem(kRbStartCenter)->remove();
		if (elem(kRbNotHitCenter)->isAdded())
			elem(kRbNotHitCenter)->remove();
		if (elem(kRbPickUp)->isAdded())
			elem(kRbPickUp)->remove();
		if (option >= 0 && option < 2)
			playElem(ballClip(kBallIdle, nullptr));
		break;
	case kActCars:
		// FUN_0047d3e0
		removeList(kCarsList);
		if (option >= 0 && option < 3)
			addElemDefault(kCarsList[option]);
		break;
	case kActGallery: {
		// FUN_0047d6d0: a picture and the character's presentation
		static const int kPictures[5] = { kGallery1, kGallery2, kGallery4, kGallery5, kGallery6 };
		static const int kVoices[5] = { kCharacter01, kCharacter02, kCharacter04, kCharacter05, kCharacter06 };
		removeAll();
		if (option >= 0 && option < 5) {
			addElem(kPictures[option], kZClip);
			addElem(kVoices[option], kZBackground);
		}
		break;
	}
	default:
		break;
	}
}

// FUN_0047ce40
void AnaglyphScene::removeList(const int *list) {
	for (int i = 0; list[i] != kNoElem; i++)
		if (elem(list[i])->isAdded())
			elem(list[i])->remove();
}

// FUN_0047cf10
void AnaglyphScene::removeAll() {
	removeList(kAllList);
}

// FUN_0047d7d0: every clip stops where it is (FUN_00412a60)
void AnaglyphScene::stopAll() {
	for (int i = 0; kAllList[i] != kNoElem; i++) {
		Anim *a = elem(kAllList[i]);
		if (a->isAdded() && a->isSmacker())
			a->stop();
	}
}

// FUN_0047d7c0
void AnaglyphScene::stopUnlessBall() {
	if (activityId() != kActRubberball)
		stopAll();
}

// FUN_0047e000 (START): the stopped clips go on (FUN_00412a20), or start
// again from their first frame once they have ended (FUN_004129d0)
void AnaglyphScene::startAll() {
	for (int i = 0; kAllList[i] != kNoElem; i++) {
		Anim *a = elem(kAllList[i]);
		if (!a->isAdded() || a->isPlaying() || !a->isSmacker())
			continue;
		if (_finished) {
			a->play();
		} else {
			a->resume();
			applyRate(a);
		}
	}
	_finished = false;
}

// FUN_0047e060: "play all" starts with the list's first clip again
void AnaglyphScene::restartList(const int *list) {
	if (_finished && _option == 0) {
		removeList(list);
		addElemDefault(list[0]);
	}
	startAll();
}

// FUN_0047ddb0 / FUN_0047dde0 / FUN_0047de40: "play all" goes on with the
// list's next clip; the last one, or a single one, stops everything
void AnaglyphScene::listClipFinished(const int *list, Anim *a) {
	const int e = elemIndex(a);
	const int i = e == kNoElem ? -1 : indexIn(list, e);
	if (i < 0)
		return;
	if (_option == 0 && list[i + 1] != kNoElem) {
		a->remove();
		playElem(list[i + 1]);
		return;
	}
	stopAll();
	_finished = true;
}

// FUN_0047cf20: 15..30 fps. The original sets the Smacker frame rate of
// the clips opened from now on (FUN_00402f40 -> SmackFrameRate) and opens
// the clips shown again at their frame; the music is not opened again.
void AnaglyphScene::setRate(int fps) {
	fps = CLIP<int>(fps, kNormalFps, kMaxFps);
	if (fps == _fps)
		return;
	_fps = fps;
	debug(1, "Anaglyph: %d fps", _fps);
	showRate();
	for (int i = 0; kAllList[i] != kNoElem; i++) {
		Anim *a = elem(kAllList[i]);
		if (a->isAdded() && a->isSmacker() && a->isPlaying()) {
			a->setFrameRate(_fps);
			a->setVolume(_fps > kNormalFps ? 0 : 255);
		}
	}
}

// A clip (re)started plays at the forced rate. Smacker plays its sound at
// the normal rate whatever the frame rate (the help: "Når du bruker FART,
// vil du ikke høre lyd"); the port mutes a faster clip instead of letting
// the mixer raise its pitch.
void AnaglyphScene::applyRate(Anim *a) {
	if (!a->isSmacker() || !a->isPlaying())
		return;
	if (_fps != kNormalFps)
		a->setFrameRate(_fps);
	a->setVolume(_fps > kNormalFps ? 0 : 255);
}

// ---- events ------------------------------------------------------------------------

void AnaglyphScene::onMouseMove(int hotspot, int x, int y) {
	_buttons.setHover(hotspot);
}

// FUN_0047d9e0 (event 0x105), then the BUTTON module
void AnaglyphScene::onMouseDown(int hotspot, int x, int y) {
	_leftDown = true;
	if (hotspot != kButtonPlay) {
		// FUN_004093f0: the scroll bar; FUN_00407ec0: a row of the list
		bool hit;
		if (_scroll.mouseDown(x, y, hit))
			scrolled();
		else if (hit)
			showThumb();
		const int row = _list.click(x, y);
		if (row >= 0) {
			redrawList();
			if (row != _option)
				selectOption(row);
		}
	} else {
		// A press on START: the rubber ball game times the first one since
		// the last hit clip began
		if (_presses == 0)
			_pressTime = _vm->getGameMillis();
		_presses++;
	}
	_buttons.mouseDown(hotspot, x, y);
}

void AnaglyphScene::onMouseUp(int hotspot, int x, int y) {
	_leftDown = false;
	const bool dragging = _scroll.isDragging();
	if (_scroll.release())
		scrolled();
	else if (dragging)
		showThumb();
	const int id = _buttons.mouseUp();
	if (id)
		onButton(id);
}

void AnaglyphScene::onUpdate() {
	// Event 0x10a of the scroll module: the thumb follows the mouse
	const int y = _vm->mousePos().y;
	if (_leftDown && _scroll.isDragging() && y != _lastMouseY) {
		if (_scroll.drag(y))
			scrolled();
		else
			showThumb();
	}
	_lastMouseY = y;
}

// FUN_0047da50 (event 0x10f)
void AnaglyphScene::onAnimStarted(Anim *a) {
	if (inList(kAllList, a))
		applyRate(a);
	// Solan on the pump: the pump moves with him
	if (activityId() == kActYard && a == elem(kYardSolan))
		playElem(kYardPump);
}

// FUN_0047da70 (event 0x110)
void AnaglyphScene::onAnimFinished(Anim *a) {
	switch (activityId()) {
	case kActYard:
		if (a == elem(kYardPump))
			a->remove();
		listClipFinished(kYardList, a);
		break;
	case kActPee:
		listClipFinished(kPeeList, a);
		break;
	case kActTvroom:
		listClipFinished(kTvroomList, a);
		break;
	case kActOutro:
		if (a == elem(kOutroShot))
			break;
		// fall through
	case kActHouse:
	case kActOuttent:
	case kActIntent:
	case kActPingpong:
	case kActCars:
	case kActGallery:
		stopAll();
		_finished = true;
		break;
	case kActRubberball:
		ballClipFinished(a);
		break;
	default:
		break;
	}
}

// FUN_0047de90 (event 0x116)
void AnaglyphScene::onButton(int id) {
	const bool ball = activityId() == kActRubberball;
	switch (id) {
	case kButtonHelp:
		// FUN_0041e180: the rubber ball game has a help file of its own
		if (ball)
			_vm->showHelp("rubberball.ini");
		else
			_vm->showHelp();
		break;
	case kButtonExit:
		_vm->endGame();
		break;
	case kButtonPlay:
		switch (activityId()) {
		case kActYard:
			restartList(kYardList);
			break;
		case kActPee:
			restartList(kPeeList);
			break;
		case kActTvroom:
			restartList(kTvroomList);
			break;
		case kActRubberball:
			break;
		default:
			startAll();
			break;
		}
		break;
	case kButtonLoad:
		chooseActivity();
		break;
	case kButtonStop:
		stopUnlessBall();
		if (ball) {
			// FUN_0041f640: the table, with this visit's last score
			const bool own = _scores > 0;
			showScoreTables(_vm, "anaglyph:SCENE_RUBBERBALL", 0, own ? &_entry : nullptr, own ? _highlights : nullptr, kScorePoints);
		}
		break;
	case kButtonPrint:
		printPicture();
		break;
	case kButtonFaster:
		if (!ball)
			setRate(_fps + 1);
		break;
	case kButtonSlower:
		if (!ball)
			setRate(_fps - 1);
		break;
	case kButtonUp:
		// FUN_00409450: one step
		if (_scroll.setValue(_scroll.value() - 1))
			scrolled();
		break;
	case kButtonDown:
		if (_scroll.setValue(_scroll.value() + 1))
			scrolled();
		break;
	default:
		break;
	}
}

// FUN_0047e0a0: the visible elements as one picture for the activity centre
void AnaglyphScene::printPicture() {
	if (_vm->messageBox("anaglyph:ACTIVITY_TITLE", "anaglyph:ACTIVITY_TEXT", MessageBox::kButtonYes | MessageBox::kButtonNo) != MessageBox::kButtonYes)
		return;
	// The shown elements (+0x44, the clips' current frames) and the
	// rectangle around them
	Common::Array<Anim *> shown;
	Common::Rect area;
	for (int i = 0; kAllList[i] != kNoElem; i++) {
		Anim *a = elem(kAllList[i]);
		if (!a->isAdded() || !a->def()->visible || !a->surface())
			continue;
		if (shown.empty())
			area = a->rect();
		else
			area.extend(a->rect());
		shown.push_back(a);
	}
	if (shown.empty() || area.isEmpty())
		return;
	// qsort by z (LAB_0047e220); stable here
	for (uint i = 1; i < shown.size(); i++) {
		Anim *a = shown[i];
		int j = i - 1;
		while (j >= 0 && shown[j]->z() > a->z()) {
			shown[j + 1] = shown[j];
			j--;
		}
		shown[j + 1] = a;
	}
	// On black (FUN_004048e0), each element with its colour key
	Graphics::ManagedSurface picture(area.width(), area.height(), _vm->_screen->format);
	picture.fillRect(Common::Rect(area.width(), area.height()), _vm->_screen->format.RGBToColor(0, 0, 0));
	for (auto *a : shown) {
		const int x = a->x(), y = a->y();
		a->setPos(x - area.left, y - area.top);
		a->draw(picture);
		a->setPos(x, y);
	}
	debug(1, "Anaglyph: print %d elements, %dx%d", (int)shown.size(), area.width(), area.height());
	_vm->showActivity("anaglyph", 0, picture);
}

// ---- the rubber ball game ----------------------------------------------------------

// FUN_0047dce0: which kind of clip ended; a hit clip's column (center,
// left, right: the hit list's index / 3)
int AnaglyphScene::ballClipKind(Anim *a, int *column) const {
	*column = 0;
	const int e = elemIndex(a);
	if (e == kNoElem)
		return -1;
	if (indexIn(kIdleList, e) >= 0)
		return kBallIdle;
	const int i = indexIn(kHitList, e);
	if (i >= 0) {
		*column = i / 3;
		return kBallHit;
	}
	if (e == kRbStartCenter)
		return kBallStart;
	if (e == kRbNotHitCenter)
		return kBallMiss;
	return e == kRbPickUp ? kBallPickUp : -1;
}

// FUN_0047d5b0: the next clip. Idle clips and hits come at random without
// repeats; after the serve a hit of the column; both reset the presses.
int AnaglyphScene::ballClip(int kind, const int *column) {
	switch (kind) {
	case kBallIdle: {
		const int n = pickRandom(_vm, _idleCount, _idleMask, 3);
		_presses = 0;
		return kIdleList[n];
	}
	case kBallStart:
		return kRbStartCenter;
	case kBallHit: {
		const int n = column ? *column * 3 + (int)_vm->getRandomNumber(2) : pickRandom(_vm, _hitCount, _hitMask, 9);
		_presses = 0;
		return kHitList[n];
	}
	case kBallMiss:
		return kRbNotHitCenter;
	case kBallPickUp:
		return kRbPickUp;
	default:
		return kNoElem;
	}
}

// FUN_0047dc30: whether Solan hits the ball at the end of a hit clip.
// Watching (row 0) he hits the number drawn at the start; playing (row 1)
// the first press of START since the clip began must have come at most
// (100 - hits) * 20 ms before its end, from 80 hits on (160 - hits) * 5 ms,
// at least 200 ms. A clip without a press is judged by the last press.
bool AnaglyphScene::ballHit(uint32 now) const {
	if (_option == 0)
		return _hits < _target;
	if (_option == 1) {
		int window = (100 - _hits) * 20;
		if (window < 400)
			window = (160 - _hits) * 5;
		if (window < 200)
			window = 200;
		return (int)(now - _pressTime) <= window;
	}
	return now != 0;  // the original returns the time itself (no such row)
}

// FUN_0047db40: idle -> (START pressed) serve -> hits -> miss -> pick up -> idle
void AnaglyphScene::ballClipFinished(Anim *a) {
	int column = 0;
	const int kind = ballClipKind(a, &column);
	if (kind < 0)
		return;
	int next = kind;
	const int *nextColumn = nullptr;
	switch (kind) {
	case kBallIdle:
		if (_presses > 0) {
			next = kBallStart;
			_target = 15 + (int)_vm->getRandomNumber(34);   // FUN_0040c550(15, 50)
			_hits = 0;
			showHits();
		}
		break;
	case kBallStart:
		next = kBallHit;
		nextColumn = &column;
		break;
	case kBallHit:
		if (!ballHit(_vm->getGameMillis())) {
			next = kBallMiss;
			nextColumn = &column;
		} else {
			_hits++;
			showHits();
		}
		break;
	case kBallMiss:
		next = kBallPickUp;
		// FUN_0047dca0: the player's hits go to the table (no medals, no dialog)
		if (_option == 1) {
			ScoreTable tables[3];
			addScore(_vm, 0, _hits, nullptr, kScorePoints, &_entry, tables);
			for (int i = 0; i < 3; i++)
				_highlights[i] = tables[i].highlight;
			_scores++;
			debug(1, "Anaglyph: rubber ball, %d hits entered", _hits);
		}
		break;
	case kBallPickUp:
		next = kBallIdle;
		break;
	default:
		break;
	}
	debug(1, "Anaglyph: rubber ball clip %s ended (kind %d, hits %d, presses %d), next kind %d", a->name(), kind, _hits, _presses, next);
	a->remove();
	playElem(ballClip(next, nextColumn));
}

} // End of namespace Flaaklypa
