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
#include "common/system.h"

#include "flaaklypa/award.h"
#include "flaaklypa/dialog.h"
#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/globals.h"
#include "flaaklypa/hiscore.h"
#include "flaaklypa/profile.h"
#include "flaaklypa/racemenu.h"
#include "flaaklypa/resources.h"

namespace Flaaklypa {

// ---- the settings shared with the race -------------------------------------

RaceSettings &raceMenuSettings() {
	return g_engine->_globals->race.settings;
}

RaceScoreState &raceScoreState() {
	return g_engine->_globals->race.score;
}

// FUN_0047a980: the default keys (0x4a0150 player 0 keyboard, 0x4a0170
// player 1 keyboard, 0x4a0190 / 0x4a01b0 the gamepads: axes 12 13 10 11,
// buttons 1 2), the type and the device (FUN_0040e4f0 the keyboard,
// FUN_0040e500 a gamepad; none here).
void raceDefaultControls(RaceControls &c, int player, int type) {
	static const int32 kKeys[4][6] = {
		{ 0x26, 0x28, 0x25, 0x27, 0x11, 0x10 },  // Up Down Left Right Ctrl Shift
		{ 0x54, 0x47, 0x46, 0x48, 0x41, 0x5a },  // T G F H A Z
		{ 12, 13, 10, 11, 1, 2 },                // gamepad, player 0
		{ 12, 13, 10, 11, 1, 2 }                 // gamepad, player 1
	};
	int row = -1;
	int32 device = 0;
	if (type == RaceControls::kTypeKeyboard) {
		row = player == 0 ? 0 : (player == 1 ? 1 : -1);
		device = 1;
	} else if (type == RaceControls::kTypeGamepad1 || type == RaceControls::kTypeGamepad2) {
		row = player == 0 ? 2 : (player == 1 ? 3 : -1);
		device = 0; // no gamepads (FUN_0040e500 gives 0)
	}
	for (int i = 0; i < RaceControls::kKeys; i++)
		c.keys[i] = row >= 0 ? kKeys[row][i] : 0;
	c.type = type;
	c.device = device;
}

// ---- tables of the original ------------------------------------------------

enum {
	kButtonHelp = 1,
	kButtonExit = 2,
	kHotspotTrack = 3,          ///< the ovals' clips
	kHotspotCar1 = 4,
	kHotspotCar2 = 5,
	kSliderMode = 6,            ///< the SCROLL module's ids
	kSliderLaps = 7,
	kSliderDrivers = 8,
	kButtonTrackPrev = 10,
	kButtonTrackNext = 11,
	kButtonControls1 = 12,
	kButtonCar1Prev = 13,
	kButtonCar1Next = 14,
	kButtonControls2 = 15,
	kButtonCar2Prev = 16,
	kButtonCar2Next = 17,
	kButtonStart = 18,
	kButtonTricks = 19,
	kButtonRaceTimes = 20,
	kButtonLapTimes = 21,
	kButtonSound = 22,

	kMaxCars = 0x18,            ///< FUN_0047b180: list sizes
	kMaxTracks = 0x10,
	kStateSize = 0xf0           ///< the saved menu (value 0 of the scene)
};

// 0x4a0128: the laps of the laps slider; 0x4a0134: the mode of the mode
// slider; 0x4a0140: the flag of the race time table key per laps.
static const int kLaps[3] = { 3, 5, 8 };
static const int kModes[3] = { RaceSettings::kModeOnePlayer, RaceSettings::kModeTwoPlayers, RaceSettings::kModeDemo };
static const uint32 kLapsFlag[3] = { 0x40000000, 0x20000000, 0x10000000 };

// FUN_0047b440: the sliders' tracks (x0, y0, x1, y1: 0x4a0290, 0x4a02a0,
// 0x4a02b0) and their ranges.
static const int kSliderRect[3][4] = {
	{ 536, 83, 677, 94 },
	{ 533, 182, 677, 193 },
	{ 522, 274, 688, 285 }
};
static const int kSliderRange[3] = { 2, 2, 3 };

// FUN_0047aa50: the bitmap of a slider's value, "%s%d" of the name and
// value + 1, at the positions of 0x4a02c0 / 0x4a02d8 / 0x4a02f0.
static const char *const kSliderBitmap[3] = { "mode", "laps", "drivers" };
static const int kSliderBitmapPos[3][4][2] = {
	{ { 520, 53 }, { 583, 52 }, { 656, 53 }, { 0, 0 } },
	{ { 522, 153 }, { 590, 152 }, { 663, 152 }, { 0, 0 } },
	{ { 507, 245 }, { 555, 245 }, { 610, 246 }, { 659, 245 } }
};

// The tooltips of the buttons (FUN_00406910(id, 10, key) in FUN_0047b700).
static const struct {
	int id;
	const char *key;
} kTooltips[] = {
	{ kButtonStart, "racemenu:TIP_START" },
	{ kButtonLapTimes, "racemenu:TIP_LAPTIME" },
	{ kButtonRaceTimes, "racemenu:TIP_RACETIME" },
	{ kButtonSound, "racemenu:TIP_SOUND" },
	{ kButtonTricks, "racemenu:TIP_TRICKS" },
	{ kButtonTrackNext, "racemenu:TRACK_NEXT" },
	{ kButtonCar1Next, "racemenu:CAR_NEXT" },
	{ kButtonCar2Next, "racemenu:CAR_NEXT" },
	{ kButtonTrackPrev, "racemenu:TRACK_PREV" },
	{ kButtonCar1Prev, "racemenu:CAR_PREV" },
	{ kButtonCar2Prev, "racemenu:CAR_PREV" },
	{ kButtonControls1, "racemenu:CAR_INFO" },
	{ kButtonControls2, "racemenu:CAR_INFO" }
};

// The sample clips of the volume dialog (elements of the scene).
static const char *const kSoundClips[3] = {
	"../../common/sound/horn", "../../common/sound/speaker", "../../common/sound/engine"
};

// FUN_00469910: (value + delta) wrapped to 0..count-1.
static int wrapIndex(int value, int count, int delta) {
	value += delta;
	if (count < 2)
		return 0;
	if (value < 0)
		return count - (-value % count);
	return value % count;
}

static Common::String iniString(Common::INIFile &ini, const Common::String &section, const Common::String &key, bool *found = nullptr) {
	Common::String v;
	bool ok = ini.getKey(key, section, v);
	if (found)
		*found = ok;
	if (!ok)
		return "";
	v = Resources::iniValue(v);
	v.trim();
	return v;
}

static int iniInt(Common::INIFile &ini, const Common::String &section, const Common::String &key, int def) {
	bool found;
	Common::String v = iniString(ini, section, key, &found);
	return found ? atoi(v.c_str()) : def;
}

// ---- the volume dialog of the race (FUN_00422180 with the menu's labels) ------

/**
 * FUN_004221a0 without the subtitle part ("sounddlg"), as the race menu
 * opens it (FUN_0047c400): the effect, commentator and engine volumes of
 * the race (profile value 0x29a of "racing"), with the menu's labels; a
 * slider plays the horn, speaker or engine sample (LAB_0047c4e0).
 */
class RaceSoundDialog : public Dialog {
public:
	RaceSoundDialog(FlaaklypaEngine *vm, Scene *scene, float *volumes);
	~RaceSoundDialog() override;

protected:
	void onButton(int id) override;
	void onMouseDown(int x, int y, bool onDialog) override;
	void onMouseMove(int x, int y, bool leftHeld) override;
	void onMouseUp(int x, int y) override;
	void onKey(const Common::KeyState &key) override;
	void drawContents(Graphics::ManagedSurface &dst) override;

private:
	enum {
		kButtonOk = 0x3ec,
		kButtonCancel = 0x3ed,
		kButtonArrows = 2000
	};
	void valueChanged(int channel);
	void sliderEvent(int event, int channel, float value);

	Scene *_scene;
	float *_volumes;
	BitmapFont _font, _titleFont;
	ScrollBar _sliders[3];
	Graphics::ManagedSurface *_thumb;
	int _drag;
};

static const int kSoundLabelTop[3] = { 52, 90, 129 };
static const int kSoundLabelBottom[3] = { 75, 112, 151 };
static const int kSoundSliderY[3] = { 53, 91, 130 };
static const char *const kSoundLabels[3] = { "racemenu:EFFECT", "racemenu:SPEECH", "racemenu:MUSIC" };

RaceSoundDialog::RaceSoundDialog(FlaaklypaEngine *vm, Scene *scene, float *volumes) :
	Dialog(vm, true), _scene(scene), _volumes(volumes), _thumb(nullptr), _drag(-1) {
	Graphics::ManagedSurface *backdrop = loadBitmap("sounddlg", "backdrop");
	if (!backdrop) {
		close(2);
		return;
	}
	// FUN_00407070: 0x180 x 0xd7 centred
	setSurface(backdrop, (_vm->screenWidth() - 0x180) / 2, (_vm->screenHeight() - 0xd7) / 2);
	if (_vm->_resources->exists("common", "dialogue/sounddlg/hotspot.bmp"))
		setHitMask(_vm->_resources->loadMask("common", "dialogue/sounddlg/hotspot.bmp"));
	_font.load("Amerigo BT_10_");
	_titleFont.load("Amerigo BT_14_");
	// FUN_004223a0: the title and the given labels
	_titleFont.drawStringCentred(*backdrop, Common::Rect(13, 8, 364, 38), _vm->getString("sounddlg:TITLE"));
	for (int c = 0; c < 3; c++)
		_font.drawStringCentred(*backdrop, Common::Rect(11, kSoundLabelTop[c], 80, kSoundLabelBottom[c]), _vm->getString(kSoundLabels[c]));
	// FUN_004222e0: OK and Cancel
	addButton(kButtonOk, 0xc5, 0xa8, "sounddlg", "button_0", "button_1", nullptr, _vm->getString("interfaceh:OK"), false);
	addButton(kButtonCancel, 0x11e, 0xa8, "sounddlg", "button_0", "button_1", nullptr, _vm->getString("interfaceh:CANCEL"), false);
	// FUN_004224a0: the sliders 0..100, arrows a step of 10
	_thumb = loadBitmap("sounddlg", "thumb");
	for (int c = 0; c < 3; c++) {
		_sliders[c].init(Common::Rect(0x7b, kSoundSliderY[c], 0x7b + 0xa3, kSoundSliderY[c] + 0x15),
		                 _thumb ? _thumb->w : 0, _thumb ? _thumb->h : 0, true);
		_sliders[c].setRange(100);
		addButton(kButtonArrows + 2 * c, 0x5b, kSoundSliderY[c], "sounddlg", "down_0", "down_1", nullptr, "", false);
		addButton(kButtonArrows + 2 * c + 1, 300, kSoundSliderY[c], "sounddlg", "up_0", "up_1", nullptr, "", false);
	}
	// FUN_00422930: the volumes in percent, rounded
	for (int c = 0; c < 3; c++) {
		float v = _volumes[c] * 100.0f;
		_sliders[c].setValue((int)(v < 0.0f ? v - 0.5f : v + 0.5f));
	}
}

RaceSoundDialog::~RaceSoundDialog() {
	delete _thumb;
}

void RaceSoundDialog::valueChanged(int channel) {
	// FUN_00423050: the callback gets value / 100
	sliderEvent(0, channel, _sliders[channel].value() * 0.01f);
}

// LAB_0047c4e0: event 0 keeps the volume (value >= 0) and plays the
// channel's sample (added and started when it is not playing) at that
// volume; event 1 removes the sample.
void RaceSoundDialog::sliderEvent(int event, int channel, float value) {
	Anim *a = _scene->anim(kSoundClips[channel]);
	if (!a)
		return;
	if (event == 1) {
		if (a->isAdded())
			a->remove();
		return;
	}
	if (event != 0)
		return;
	if (value >= 0.0f)
		_volumes[channel] = value;
	if (!a->isAdded())
		a->add(0, 0, 0);
	if (!a->isPlaying())
		a->play();
	// FUN_00403620(player, volume, track 0): the volume of the sample
	// (here relative to the sound volume, Anim::setVolume())
	a->setVolume(CLIP((int)(_volumes[channel] * 255.0f), 0, 255));
}

void RaceSoundDialog::onButton(int id) {
	switch (id) {
	case kButtonOk:
		close(1);
		break;
	case kButtonCancel:
		close(2);
		break;
	default:
		if (id >= kButtonArrows && id < kButtonArrows + 6) {
			int c = (id - kButtonArrows) / 2;
			int step = (id - kButtonArrows) % 2 ? 10 : -10;
			if (_sliders[c].setValue(_sliders[c].value() + step))
				valueChanged(c);
		}
		break;
	}
}

void RaceSoundDialog::onMouseDown(int x, int y, bool onDialog) {
	for (int c = 0; c < 3; c++) {
		bool hit;
		bool changed = _sliders[c].mouseDown(x - this->x(), y - this->y(), hit);
		if (hit) {
			_drag = c;
			sliderEvent(0, c, -1.0f);
		}
		if (changed)
			valueChanged(c);
	}
}

void RaceSoundDialog::onMouseMove(int x, int y, bool leftHeld) {
	if (!leftHeld || _drag < 0)
		return;
	if (_sliders[_drag].drag(x - this->x()))
		valueChanged(_drag);
}

void RaceSoundDialog::onMouseUp(int x, int y) {
	if (_drag < 0)
		return;
	int c = _drag;
	_drag = -1;
	sliderEvent(1, c, -1.0f);
	if (_sliders[c].release())
		valueChanged(c);
}

void RaceSoundDialog::onKey(const Common::KeyState &key) {
	if (key.keycode == Common::KEYCODE_RETURN || key.keycode == Common::KEYCODE_KP_ENTER)
		close(1);
	else if (key.keycode == Common::KEYCODE_ESCAPE)
		close(2);
}

void RaceSoundDialog::drawContents(Graphics::ManagedSurface &dst) {
	const int dx = x(), dy = y();
	for (int c = 0; c < 3; c++) {
		_font.drawStringCentred(dst, Common::Rect(dx + 0x146, dy + kSoundLabelTop[c], dx + 0x146 + 0x27, dy + kSoundLabelTop[c] + 0x17),
		                        Common::String::format("%d", _sliders[c].value()));
		if (_thumb) {
			Common::Rect r = _sliders[c].thumbRect();
			dst.blitFrom(*_thumb, Common::Point(dx + r.left, dy + r.top));
		}
	}
}

// ---- the controls dialog (FUN_0041d140, dialogue "ctrlcfg") ----------------------

/**
 * A player's controls: the device tabs (keyboard; the gamepads are greyed
 * without one), the six inputs with their keys and CHANGE buttons (then
 * the next key pressed that no other input uses; F1..F24 are refused,
 * Escape stops waiting and leaves the input on "?"), RESET (the device's
 * defaults, FUN_0047a980), OK (Enter) and CANCEL (Escape). The handler
 * LAB_0041d7d0 ignores the mouse while waiting for a key.
 */
class ControlsDialog : public Dialog {
public:
	ControlsDialog(FlaaklypaEngine *vm, RacePlayerControls &target, int player) :
		Dialog(vm, false), _target(target), _copy(target), _player(player), _waiting(-1) {
		Graphics::ManagedSurface *backdrop = loadBitmap("ctrlcfg", "backdrop");
		if (!backdrop) {
			close(2);
			return;
		}
		// FUN_00407290("ctrlcfg", 0xe2, 0x5d, 500)
		setSurface(backdrop, 0xe2, 0x5d);
		if (_vm->_resources->exists("common", "dialogue/ctrlcfg/hotspot.bmp"))
			setHitMask(_vm->_resources->loadMask("common", "dialogue/ctrlcfg/hotspot.bmp"));
		_font.load("Amerigo BT_10_");
		_title.load("Amerigo BT_14_");
		addButton(kCancel, 0xfb, 0x16f, "ctrlcfg", "button_0", "button_1", nullptr, _vm->getString("interfaceh:CANCEL"), false);
		addButton(kOk, 0xa5, 0x16f, "ctrlcfg", "button_0", "button_1", nullptr, _vm->getString("interfaceh:OK"), false);
		addButton(kReset, 10, 0x16f, "ctrlcfg", "button_0", "button_1", nullptr, _vm->getString("ctrlcfg:RESET"), false);
		// the title (0x100: centred, at the top) in (13, 11) 318 x 30
		const Common::String title = _vm->getString("ctrlcfg:TITLE");
		_title.drawString(*backdrop, 13 + (0x13e - _title.stringWidth(title)) / 2, 11, title);
		// the device tabs (group 1): kb, gp1, gp2 with their grey bitmaps
		static const char *const kTabs[3] = { "kb", "gp1", "gp2" };
		static const int kTabX[3] = { 0x17, 0x7f, 0xe6 };
		for (int i = 0; i < 3; i++) {
			_tabs[i][0] = loadBitmap("ctrlcfg", Common::String::format("%s_0", kTabs[i]).c_str());
			_tabs[i][1] = loadBitmap("ctrlcfg", Common::String::format("%s_1", kTabs[i]).c_str());
			_tabs[i][2] = loadBitmap("ctrlcfg", Common::String::format("%s_grey", kTabs[i]).c_str());
			_tabX[i] = kTabX[i];
		}
		// the six rows: label, key, CHANGE (0x138c + i at (0xf6, 0x8f + 32 i))
		static const char *const kLabels[6] = { "ctrlcfg:ACCELERATE", "ctrlcfg:REVERSE", "ctrlcfg:LEFT", "ctrlcfg:RIGHT", "ctrlcfg:TRICK", "ctrlcfg:HORN" };
		for (int i = 0; i < 6; i++) {
			const int y = 0xe9 - 0x5d + 32 * i;
			const Common::String label = _vm->getString(kLabels[i]);
			_font.drawString(*backdrop, 0xf1 - 0xe2, y + (32 - _font.height()) / 2, label);
			addButton(kChange + i, 0xf6, 0xe9 - 0x5a + 32 * i, "ctrlcfg", "button_0", "button_1", nullptr, _vm->getString("ctrlcfg:CHANGE"), false);
		}
	}

	~ControlsDialog() override {
		for (auto &t : _tabs)
			for (auto *b : t)
				delete b;
	}

protected:
	enum {
		kOk = 0x1389,
		kCancel = 0x138a,
		kReset = 0x138b,
		kChange = 0x138c,
		kTabKeyboard = 0x1392
	};

	RaceControls &current() { return _copy.configs[CLIP<int32>(_copy.selected, 0, 2)]; }

	// FUN_0041d660: a key's name (0x4af730), else the character; a
	// gamepad's buttons and axes
	Common::String keyName(int32 key, int32 type) {
		static const struct {
			int vk;
			const char *name;
		} kNames[15] = {
			{ 0x0d, "ctrlcfg:RETURN" }, { 0x20, "ctrlcfg:SPACE" }, { 0x26, "ctrlcfg:ARROW_UP" },
			{ 0x28, "ctrlcfg:ARROW_DOWN" }, { 0x25, "ctrlcfg:ARROW_LEFT" }, { 0x27, "ctrlcfg:ARROW_RIGHT" },
			{ 0x10, "ctrlcfg:SHIFT" }, { 0x11, "ctrlcfg:CONTROL" }, { 0x09, "ctrlcfg:TAB" }, { 0x2d, "ctrlcfg:INS" },
			{ 0x2e, "ctrlcfg:DEL" }, { 0x24, "ctrlcfg:HOME" }, { 0x23, "ctrlcfg:END" }, { 0x22, "ctrlcfg:PGDN" },
			{ 0x21, "ctrlcfg:PGUP" }
		};
		if (type == RaceControls::kTypeKeyboard) {
			for (const auto &n : kNames)
				if (n.vk == key)
					return _vm->getString(n.name);
			return Common::String::format("%c", (char)key);
		}
		switch (key) {
		case 1:
			return _vm->getString("ctrlcfg:BUTTON1");
		case 2:
			return _vm->getString("ctrlcfg:BUTTON2");
		case 3:
			return _vm->getString("ctrlcfg:BUTTON3");
		case 4:
			return _vm->getString("ctrlcfg:BUTTON4");
		case 10:
			return _vm->getString("ctrlcfg:JOY_LEFT");
		case 11:
			return _vm->getString("ctrlcfg:JOY_RIGHT");
		case 12:
			return _vm->getString("ctrlcfg:JOY_UP");
		case 13:
			return _vm->getString("ctrlcfg:JOY_DOWN");
		default:
			return "??";
		}
	}

	void onButton(int id) override {
		if (_waiting >= 0)
			return;
		// FUN_0041dae0
		if (id == kOk) {
			_target = _copy;
			close(1);
		} else if (id == kCancel) {
			close(2);
		} else if (id == kReset) {
			raceDefaultControls(current(), _player, current().type);
		} else if (id >= kChange && id < kChange + 6) {
			// FUN_0041dbd0: "?" until a key comes
			_waiting = id - kChange;
			current().keys[_waiting] = 0x3f;
		}
	}

	void onMouseDown(int x, int y, bool onDialog) override {
		if (_waiting >= 0)
			return;
		// the tabs (FUN_004075c0, group 1): a device chosen; no gamepads here
		for (int i = 0; i < 3; i++) {
			const Graphics::ManagedSurface *b = _tabs[i][0];
			if (!b)
				continue;
			Common::Rect r(this->x() + _tabX[i], this->y() + 0x57, this->x() + _tabX[i] + b->w, this->y() + 0x57 + b->h);
			if (r.contains(x, y) && i == 0)
				_copy.selected = 0;
		}
	}

	// FUN_0041d970
	void onKey(const Common::KeyState &key) override {
		int vk = virtualKey(key);
		if (vk <= 3)
			return;
		if (_waiting < 0) {
			if (vk == 0x0d) {
				_target = _copy;
				close(1);
			} else if (vk == 0x1b) {
				close(2);
			}
			return;
		}
		if (vk == 0x1b) {
			_waiting = -1;
			return;
		}
		RaceControls &c = current();
		if (c.type != RaceControls::kTypeKeyboard || (0x70 <= vk && vk <= 0x87))
			return;
		bool used = false;
		for (int i = 0; i < RaceControls::kKeys; i++)
			if (c.keys[i] == vk)
				used = true;
		if (!used || vk == c.keys[_waiting]) {
			c.keys[_waiting] = vk;
			_waiting = -1;
		}
	}

	void drawContents(Graphics::ManagedSurface &dst) override {
		for (int i = 0; i < 3; i++) {
			int state = i == 0 ? (_copy.selected == 0 ? 1 : 0) : 2;   // FUN_0040e500: no gamepads
			const Graphics::ManagedSurface *b = _tabs[i][state] ? _tabs[i][state] : _tabs[i][0];
			if (b)
				dst.blitFrom(*b, Common::Point(x() + _tabX[i], y() + 0x57));
		}
		// FUN_0041d5a0: the keys right aligned in (0x16e, row) 100 x 32
		const RaceControls &c = _copy.configs[CLIP<int32>(_copy.selected, 0, 2)];
		for (int i = 0; i < 6; i++) {
			const Common::String name = keyName(c.keys[i], c.type);
			const int top = 0xe9 + 32 * i;
			_font.drawString(dst, 0x16e + 100 - _font.stringWidth(name), top + (32 - _font.height()) / 2, name);
		}
	}

private:
	RacePlayerControls &_target;
	RacePlayerControls _copy;   ///< DAT_0059ec00
	int _player;
	int _waiting;               ///< DAT_005a01e0: the input waiting for a key, -1
	BitmapFont _font, _title;
	Graphics::ManagedSurface *_tabs[3][3];
	int _tabX[3];
};

// ---- the scene -----------------------------------------------------------------

RacemenuScene::RacemenuScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def), _buttons(this), _tooltip(this),
	_track(0), _car1(0), _car2(0), _tempoStage(0), _drag(-1), _hover(0), _labelKey(0) {
	static const char *const kElems[3] = { "oval_track", "oval_car1", "oval_car2" };
	static const int kPos[3][3] = { { kHotspotTrack, 64, 67 }, { kHotspotCar1, 64, 364 }, { kHotspotCar2, 468, 364 } };
	for (int i = 0; i < 3; i++) {
		Oval &o = _ovals[i];
		o.elem = kElems[i];
		o.hotspot = kPos[i][0];
		o.x = kPos[i][1];
		o.y = kPos[i][2];
		o.clip = nullptr;
		o.surface = nullptr;
		o.playing = false;
		o.nextFrame = 0;
		o.frame = 0;
		_sliderMax[i] = kSliderRange[i];
	}
	memset((void *)_controls, 0, sizeof(_controls));
}

RacemenuScene::~RacemenuScene() {
	for (auto &o : _ovals)
		closeClip(o);
}

// FUN_0047ae80
void RacemenuScene::onInit(int arg) {
	_labelKey = _vm->_screen->format.RGBToColor(0, 0xff, 0);
	playMusic("theme");
	// race.ini of the menu (data2's lists the add-on tracks)
	if (!resources()->loadIni(_name, "race.ini", _ini))
		warning("Racemenu: race.ini not found");
	// FUN_00409b50(0): tooltips
	_tooltip.start(hotspotAt(_vm->mousePos().x, _vm->mousePos().y));
	// FUN_0047b410
	_font.load("Arial Black 08");
	_grayFont.load("Arial Black 08 Gray");
	createLabels();
	createButtons();
	createSliders();
	buildLists();
	MenuState s;
	defaultState(s);
	readState(s);
	// FUN_0047b150: the race's settings start from zero with the saved controls
	RaceSettings &r = raceMenuSettings();
	memset((void *)&r, 0, sizeof(r));
	r.players[0] = s.players[0];
	r.players[1] = s.players[1];
	_controls[0] = s.players[0];
	_controls[1] = s.players[1];
	applyState(s);
	returnFromInfo(arg);
	// the blinking lights over the start key
	anim("blinker")->add(Anim::kDefaultPos, Anim::kDefaultPos, -1);
	anim("blinker")->play();
}

// FUN_0047ba50
void RacemenuScene::onClose() {
	MenuState s;
	currentState(s);
	writeState(s);
	_tooltip.hide();
	for (auto &o : _ovals)
		closeClip(o);
}

// ---- init helpers ------------------------------------------------------------

// FUN_0047b530: the name areas of the ovals and the fixed labels (rects
// 0x4a01d0..0x4a0280, text centred in Arial Black 08).
void RacemenuScene::createLabels() {
	label("lbl_trackname", 115, 267, 278, 282, "");
	label("lbl_car1name", 115, 560, 278, 576, "");
	label("lbl_car2name", 522, 560, 684, 576, "");
	label("lbl_track", 110, 29, 282, 48, _vm->getString("racemenu:TRACK"));
	label("lbl_player1", 110, 323, 282, 342, _vm->getString("racemenu:PLAYER1"));
	label("lbl_player2", 517, 323, 690, 342, _vm->getString("racemenu:PLAYER2"));
	label("lbl_mode", 511, 29, 696, 44, _vm->getString("racemenu:MODE"));
	label("lbl_laps", 511, 128, 696, 143, _vm->getString("racemenu:LAPS"));
	label("lbl_drivers", 511, 223, 696, 238, _vm->getString("racemenu:DRIVERS"));
	label("lbl_racetime", 364, 61, 438, 78, _vm->getString("racemenu:RACETIME"));
	label("lbl_laptime", 364, 28, 438, 45, _vm->getString("racemenu:LAPTIME"));
	label("lbl_sound", 364, 93, 438, 110, _vm->getString("racemenu:SOUND"));
}

// FUN_00407a30 / FUN_00407930: a text area at z with the text centred
// (0x101). The fixed labels are as large as their text and centred in the
// rectangle; that comes to the same.
void RacemenuScene::label(const char *name, int x0, int y0, int x1, int y1, const Common::String &text, int z) {
	Anim *a = defineSurfaceAnim(name, x1 - x0, y1 - y0, _labelKey);
	if (!a->isAdded())
		a->add(x0, y0, z);
	setLabel(name, text);
}

void RacemenuScene::setLabel(const char *name, const Common::String &text) {
	Anim *a = anim(name);
	if (!a || !a->surface())
		return;
	Graphics::ManagedSurface *s = a->surface();
	s->fillRect(Common::Rect(s->w, s->h), _labelKey);
	if (!text.empty())
		_font.drawStringCentred(*s, Common::Rect(s->w, s->h), text);
}

// FUN_0047b700: the buttons at z 0 (FUN_00406ec0(0))
void RacemenuScene::createButtons() {
	_buttons.setDefaultZ(0);
	_buttons.add(kButtonHelp, 0, 0, "help_0", "help_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonExit, 0x2d8, 0, "exit_0", "exit_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonStart, 0x165, 0x121, "start_0", "start_1", nullptr, nullptr, "", false);
	_buttons.add(kButtonLapTimes, 0x1b7, 0x1c, "button_0", "button_1", nullptr, nullptr, "", false);
	_buttons.add(kButtonRaceTimes, 0x1b7, 0x3d, "button_0", "button_1", nullptr, nullptr, "", false);
	_buttons.add(kButtonSound, 0x1b7, 0x5d, "button_0", "button_1", nullptr, nullptr, "", false);
	_buttons.addToggle(kButtonTricks, 0x163, 0x1f2, "tricks_0", "tricks_1", "", false);
	_buttons.add(kButtonTrackNext, 0x117, 0xfb, "next_0", "next_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonCar1Next, 0x117, 0x222, "next_0", "next_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonCar2Next, 0x2af, 0x222, "next_0", "next_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonTrackPrev, 0x51, 0xfb, "prev_0", "prev_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonCar1Prev, 0x51, 0x222, "prev_0", "prev_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonCar2Prev, 0x1e8, 0x221, "prev_0", "prev_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonControls1, 0x1e, 0x200, "ply1_0", "ply1_1", nullptr, nullptr, "", false);
	_buttons.add(kButtonControls2, 0x2da, 0x1ff, "ply2_0", "ply2_1", nullptr, nullptr, "", false);
}

// FUN_0047b440: three horizontal sliders (flags 0x100) with the thumb
// "slider_button" at z 0 (FUN_004094c0(0)); ranges 0..2, 0..2, 0..3.
void RacemenuScene::createSliders() {
	const Graphics::ManagedSurface *thumb = _buttons.bitmap("slider_button");
	for (int i = 0; i < 3; i++) {
		const int *r = kSliderRect[i];
		_sliders[i].init(Common::Rect(r[0], r[1], r[2], r[3]), thumb ? thumb->w : 0, thumb ? thumb->h : 0, true);
		_sliders[i].setRange(_sliderMax[i]);
		showThumb(i);
	}
}

void RacemenuScene::showThumb(int i) {
	Common::Rect r = _sliders[i].thumbRect();
	_buttons.showBitmap(Common::String::format("thumb%d", i), "slider_button", false, kSliderMode + i, r.left, r.top, 0);
}

// FUN_0047b180: the tracks and cars of race.ini the player may choose,
// then the silent award of them (FUN_0047b330).
void RacemenuScene::buildLists() {
	// FUN_004231e0: the stage of Il Tempo Gigante, from the most parts
	// ever fitted (buildacar's value 0, second int) and the parts per
	// stage {13, 13, 14, 12, 10}
	{
		static const int kStageParts[5] = { 13, 13, 14, 12, 10 };
		int32 most = 0;
		byte car[20];
		if (_vm->profile() && _vm->profile()->getRecord("buildacar", Profile::kRecordValue, 0, car, sizeof(car)))
			most = (int32)READ_LE_UINT32(car + 4);
		_tempoStage = 5;
		for (int i = 0; i < 5; i++) {
			most -= kStageParts[i];
			if (most < 0) {
				_tempoStage = i;
				break;
			}
		}
	}
	buildList("cars", 2, _cars, kMaxCars);
	buildList("tracks", 1, _tracks, kMaxTracks);
	silentAward();
	_track = _car1 = _car2 = 0;
	debug(1, "Racemenu: %d tracks, %d cars available, Il Tempo stage %d", _tracks.size(), _cars.size(), _tempoStage);
}

// FUN_0047b1d0: "resource<n>" until one is missing; "state<n>" (default
// defState): 0 always, 1 the track award "level<n>" won, 2 the car award
// "level<n>" won, 3 Il Tempo's stage at least "level<n>".
int RacemenuScene::buildList(const char *section, int defState, Common::Array<int> &list, int max) {
	list.clear();
	const int32 tracks = awardCounter(_vm, kAwardTrack);
	const int32 cars = awardCounter(_vm, kAwardCar);
	for (int i = 0; i < max; i++) {
		bool found;
		iniString(_ini, section, Common::String::format("resource%d", i), &found);
		if (!found)
			break;
		int state = iniInt(_ini, section, Common::String::format("state%d", i), defState);
		int level = iniInt(_ini, section, Common::String::format("level%d", i), 0);
		bool ok;
		switch (state) {
		case 0:
			ok = true;
			break;
		case 1:
			ok = awardIsWon(kAwardTrack, tracks, level);
			break;
		case 2:
			ok = awardIsWon(kAwardCar, cars, level);
			break;
		case 3:
			ok = level <= _tempoStage;
			break;
		default:
			ok = false;
			break;
		}
		if (ok)
			list.push_back(i);
	}
	return list.size();
}

// FUN_0047b330: every car and track the menu offers counts as won (the
// award counters 2 and 3 get their bits), written without a dialog when
// they changed (FUN_00411c40).
void RacemenuScene::silentAward() {
	int32 cars = awardCounter(_vm, kAwardCar);
	for (int c : _cars)
		if (!awardIsWon(kAwardCar, cars, c))
			cars = awardAddItem(kAwardCar, cars, c);
	int32 tracks = awardCounter(_vm, kAwardTrack);
	for (int t : _tracks)
		if (!awardIsWon(kAwardTrack, tracks, t))
			tracks = awardAddItem(kAwardTrack, tracks, t);
	Profile *p = _vm->profile();
	if (p && (p->award(kAwardCar) != cars || p->award(kAwardTrack) != tracks)) {
		p->setAward(kAwardCar, cars);
		p->setAward(kAwardTrack, tracks);
		_vm->profileChanged();
		debug(1, "Racemenu: award counters cars 0x%x, tracks 0x%x", cars, tracks);
	}
}

// FUN_0047b0a0: the defaults, the saved menu over them, then the devices
// of the controls again (their pointers are not kept).
void RacemenuScene::defaultState(MenuState &s) {
	memset((void *)&s, 0, sizeof(s));
	for (int p = 0; p < 2; p++)
		for (int t = 0; t < 3; t++)
			raceDefaultControls(s.players[p].configs[t], p, t + 1);
	s.slider[2] = 3;
	s.tricks = 1;
}

void RacemenuScene::readState(MenuState &s) {
	byte data[kStateSize];
	if (_vm->profile() && _vm->profile()->getRecord(_name, Profile::kRecordValue, 0, data, kStateSize)) {
		const byte *p = data;
		auto rd = [&p]() {
			int32 v = (int32)READ_LE_UINT32(p);
			p += 4;
			return v;
		};
		s.track = rd();
		s.car1 = rd();
		s.car2 = rd();
		for (int i = 0; i < 3; i++)
			s.slider[i] = rd();
		for (int i = 0; i < 3; i++)
			s.frame[i] = rd();
		for (int pl = 0; pl < 2; pl++) {
			for (int t = 0; t < 3; t++) {
				for (int k = 0; k < RaceControls::kKeys; k++)
					s.players[pl].configs[t].keys[k] = rd();
				s.players[pl].configs[t].type = rd();
				s.players[pl].configs[t].device = rd();
			}
			s.players[pl].selected = rd();
		}
		s.tricks = rd();
	}
	for (int pl = 0; pl < 2; pl++) {
		s.players[pl].configs[0].type = RaceControls::kTypeKeyboard;
		s.players[pl].configs[0].device = 1;
		s.players[pl].configs[1].type = RaceControls::kTypeGamepad1;
		s.players[pl].configs[1].device = 0;
		s.players[pl].configs[2].type = RaceControls::kTypeGamepad2;
		s.players[pl].configs[2].device = 0;
	}
}

void RacemenuScene::writeState(const MenuState &s) {
	if (!_vm->profile())
		return;
	byte data[kStateSize];
	byte *p = data;
	auto wr = [&p](int32 v) {
		WRITE_LE_UINT32(p, (uint32)v);
		p += 4;
	};
	wr(s.track);
	wr(s.car1);
	wr(s.car2);
	for (int i = 0; i < 3; i++)
		wr(s.slider[i]);
	for (int i = 0; i < 3; i++)
		wr(s.frame[i]);
	for (int pl = 0; pl < 2; pl++) {
		for (int t = 0; t < 3; t++) {
			for (int k = 0; k < RaceControls::kKeys; k++)
				wr(s.players[pl].configs[t].keys[k]);
			wr(s.players[pl].configs[t].type);
			wr(s.players[pl].configs[t].device);
		}
		wr(s.players[pl].selected);
	}
	wr(s.tricks);
	_vm->profile()->setRecord(_name, Profile::kRecordValue, 0, data, kStateSize);
	_vm->profileChanged();
}

// FUN_0047baa0: the menu as it is
void RacemenuScene::currentState(MenuState &s) {
	memset((void *)&s, 0, sizeof(s));
	for (int i = 0; i < 3; i++)
		s.frame[i] = _ovals[i].frame;
	s.track = _track;
	s.car1 = _car1;
	s.car2 = _car2;
	for (int i = 0; i < 3; i++)
		s.slider[i] = _sliders[i].value();
	s.players[0] = _controls[0];
	s.players[1] = _controls[1];
	s.tricks = _buttons.state(kButtonTricks) == SceneButtons::kStatePressed ? 1 : 0;
}

// FUN_0047af20: the saved choices (clamped to the lists), the clips at
// their frames, the sliders and the tricks switch
void RacemenuScene::applyState(const MenuState &s) {
	_track = s.track < 0 ? 0 : MIN<int>(s.track, (int)_tracks.size() - 1);
	_car1 = s.car1 < 0 ? 0 : MIN<int>(s.car1, (int)_cars.size() - 1);
	_car2 = s.car2 < 0 ? 0 : MIN<int>(s.car2, (int)_cars.size() - 1);
	if (_track < 0)
		_track = 0;
	if (_car1 < 0)
		_car1 = 0;
	if (_car2 < 0)
		_car2 = 0;
	if (!_tracks.empty())
		showItem(_tracks[_track], "tracks", kOvalTrack, "lbl_trackname", 0);
	if (!_cars.empty()) {
		showItem(_cars[_car1], "cars", kOvalCar1, "lbl_car1name", itsTempoVersion());
		showItem(_cars[_car2], "cars", kOvalCar2, "lbl_car2name", itsTempoVersion());
	}
	// FUN_00403770: the saved frames
	for (int i = 0; i < 3; i++) {
		Oval &o = _ovals[i];
		if (o.clip && !o.resource.empty())
			openClip(o, o.resource, s.frame[i], false);
	}
	for (int i = 0; i < 3; i++) {
		_sliders[i].setValue(s.slider[i]);
		showThumb(i);
	}
	for (int i = 0; i < 3; i++)
		sliderChanged(i);
	_buttons.setState(kButtonTricks, s.tricks ? SceneButtons::kStatePressed : SceneButtons::kStateNormal);
}

// FUN_0047b960: back from the info page of an oval (arg = its hotspot):
// the item shown last goes into the oval, and becomes the choice when it
// is in the list.
void RacemenuScene::returnFromInfo(int arg) {
	if (arg < kHotspotTrack || arg > kHotspotCar2)
		return;
	const int item = _vm->infoRequest().index;
	Common::Array<int> &list = arg == kHotspotTrack ? _tracks : _cars;
	int found = -1;
	for (uint i = 0; i < list.size(); i++)
		if (list[i] == item) {
			found = i;
			break;
		}
	if (arg == kHotspotTrack) {
		showItem(item, "tracks", kOvalTrack, "lbl_trackname", 0);
		if (found >= 0)
			_track = found;
	} else if (arg == kHotspotCar1) {
		showItem(item, "cars", kOvalCar1, "lbl_car1name", itsTempoVersion());
		if (found >= 0)
			_car1 = found;
	} else {
		showItem(item, "cars", kOvalCar2, "lbl_car2name", itsTempoVersion());
		if (found >= 0)
			_car2 = found;
	}
}

// ---- the ovals ------------------------------------------------------------------

// FUN_0047ac10(resource, section, oval, label, arg): the clip
// "resource<n>" of the section replaces the oval's, at the frame the old
// one showed (1 when there was none) and playing when it played; the label
// shows "name<n>" (a language.ini key, formatted with arg: Il Tempo's
// version) centred.
void RacemenuScene::showItem(int resource, const char *section, int oval, const char *labelName, int arg) {
	if (oval >= 0) {
		Oval &o = _ovals[oval];
		int frame = 1;
		bool playing = false;
		if (o.clip) {
			frame = o.frame;
			playing = o.playing;
			closeClip(o);
		}
		bool found;
		Common::String name = iniString(_ini, section, Common::String::format("resource%d", resource), &found);
		if (found)
			openClip(o, name, frame, playing);
	}
	if (labelName) {
		bool found;
		Common::String key = iniString(_ini, section, Common::String::format("name%d", resource), &found);
		if (found)
			setLabel(labelName, Common::String::format(_vm->getString(key).c_str(), arg));
	}
}

void RacemenuScene::closeClip(Oval &o) {
	delete o.clip;
	o.clip = nullptr;
	o.playing = false;
	if (o.surface) {
		Anim *a = anim(o.elem);
		if (a && a->isAdded())
			a->remove();
	}
}

// The oval's element: the clip (transparent, the colour of the first
// pixel; looping, 15 fps) decoded into a surface by the scene, so it can
// stop and go on from its frame (FUN_00412a60 / FUN_004129d0) and jump to
// a frame (FUN_00403770). The clips have no sound.
void RacemenuScene::openClip(Oval &o, const Common::String &name, int frame, bool play) {
	if (!o.clip || o.resource != name) {
		closeClip(o);
		o.clip = resources()->openSmacker(_name, "animation/" + name + ".smk");
		o.resource = name;
		if (!o.clip)
			return;
	}
	const int count = o.clip->getFrameCount();
	frame = count > 0 ? CLIP(frame, 0, count - 1) : 0;
	Anim *a = defineSurfaceAnim(o.elem, o.clip->getWidth(), o.clip->getHeight(), _labelKey, o.hotspot);
	o.surface = a->surface();
	if (!a->isAdded())
		a->add(o.x, o.y, 0);
	// SmackGoto: decode up to the frame
	o.clip->rewind();
	const Graphics::Surface *f = nullptr;
	for (int i = 0; i <= frame; i++) {
		const Graphics::Surface *g = o.clip->decodeNextFrame();
		if (g)
			f = g;
	}
	o.frame = frame;
	if (f)
		drawFrame(o, f);
	o.playing = play;
	o.nextFrame = g_system->getMillis() + 66;
}

// The next frame, from the first after the last (the clips loop)
void RacemenuScene::advanceClip(Oval &o) {
	if (!o.clip)
		return;
	if (o.clip->getCurFrame() + 1 >= (int)o.clip->getFrameCount())
		o.clip->rewind();
	const Graphics::Surface *f = o.clip->decodeNextFrame();
	o.frame = o.clip->getCurFrame();
	if (f)
		drawFrame(o, f);
}

void RacemenuScene::drawFrame(Oval &o, const Graphics::Surface *f) {
	const byte *pal = o.clip->getPalette();
	if (!pal || !o.surface || f->format.bytesPerPixel != 1)
		return;
	// transparent: the first pixel's colour index is the key
	const byte key = *(const byte *)f->getBasePtr(0, 0);
	const Graphics::PixelFormat &fmt = o.surface->format;
	const uint32 near = fmt.RGBToColor(0, 0xfe, 0);
	for (int y = 0; y < f->h && y < o.surface->h; y++) {
		const byte *src = (const byte *)f->getBasePtr(0, y);
		for (int x = 0; x < f->w && x < o.surface->w; x++) {
			byte c = src[x];
			uint32 col = c == key ? _labelKey : fmt.RGBToColor(pal[c * 3], pal[c * 3 + 1], pal[c * 3 + 2]);
			if (col == _labelKey && c != key)
				col = near;
			o.surface->setPixel(x, y, col);
		}
	}
}

void RacemenuScene::playOval(int i) {
	// FUN_0047bc40: plays on from its frame
	Oval &o = _ovals[i];
	if (!o.clip)
		return;
	o.playing = true;
	o.nextFrame = g_system->getMillis() + 66;
}

void RacemenuScene::stopOval(int i) {
	// FUN_00412a60
	_ovals[i].playing = false;
}

int RacemenuScene::ovalAt(int hotspot) const {
	if (hotspot >= kHotspotTrack && hotspot <= kHotspotCar2)
		return hotspot - kHotspotTrack;
	return -1;
}

// ---- sliders ----------------------------------------------------------------------

// FUN_0047aa50 (event 0x117): the bitmap of the value; the mode decides
// whether player 2 has a car (FUN_0047ab50).
void RacemenuScene::sliderChanged(int i) {
	int v = CLIP(_sliders[i].value(), 0, 3);
	const int *pos = kSliderBitmapPos[i][v];
	_buttons.showBitmap(Common::String::format("value%d", i), Common::String::format("%s%d", kSliderBitmap[i], v + 1),
	                    false, 0, pos[0], pos[1], 0);
	if (i == 0) {
		// FUN_0047ab30: the modes 1 (demo) and 2 (two players)
		int mode = kModes[CLIP(v, 0, 2)];
		setTwoCars(mode > 0 && mode < 3);
	}
}

// FUN_0047ab50: car 2's buttons enabled or disabled, "player2off" over its
// oval (z 1) and its name cleared, or car 2 shown again.
void RacemenuScene::setTwoCars(bool on) {
	int state = on ? SceneButtons::kStateNormal : SceneButtons::kStateDisabled;
	_buttons.setState(kButtonCar2Prev, state);
	_buttons.setState(kButtonCar2Next, state);
	_buttons.setState(kButtonControls2, state);
	if (on) {
		if (isAnimAdded("player2off"))
			removeAnim("player2off");
		if (!_cars.empty())
			showItem(_cars[_car2], "cars", kOvalCar2, "lbl_car2name", itsTempoVersion());
	} else {
		if (!isAnimAdded("player2off"))
			anim("player2off")->add(Anim::kDefaultPos, Anim::kDefaultPos, 1);
		setLabel("lbl_car2name", "");
	}
}

// ---- input ------------------------------------------------------------------------

void RacemenuScene::onMouseMove(int hotspot, int x, int y) {
	// events 0x113 / 0x114: a clip turns while the mouse is on it
	if (hotspot != _hover) {
		int o = ovalAt(_hover);
		if (o >= 0)
			stopOval(o);
		o = ovalAt(hotspot);
		if (o >= 0)
			playOval(o);
		_hover = hotspot;
	}
	_buttons.setHover(hotspot);
	_tooltip.hotspotChanged(hotspot);
}

// FUN_0047bcc0 (event 0x105): an oval opens its info page, else a slider
void RacemenuScene::onMouseDown(int hotspot, int x, int y) {
	_tooltip.hide();
	if (ovalAt(hotspot) >= 0) {
		showInfo(hotspot);
		return;
	}
	for (int i = 0; i < 3; i++) {
		bool hit;
		bool changed = _sliders[i].mouseDown(x, y, hit);
		if (hit) {
			_drag = i;
			showThumb(i);
			if (changed)
				sliderChanged(i);
			return;
		}
	}
	int id = _buttons.mouseDown(hotspot, x, y);
	if (id)
		onButton(id);
}

void RacemenuScene::onMouseUp(int hotspot, int x, int y) {
	if (_drag >= 0) {
		// event 0x10b (FUN_004092b0): the thumb snaps to its value
		int i = _drag;
		_drag = -1;
		if (_sliders[i].release())
			sliderChanged(i);
		showThumb(i);
	}
	int id = _buttons.mouseUp();
	if (id)
		onButton(id);
}

void RacemenuScene::onUpdate() {
	// event 0x10a: the dragged thumb follows the mouse
	if (_drag >= 0) {
		if (_sliders[_drag].drag(_vm->mousePos().x))
			sliderChanged(_drag);
		showThumb(_drag);
	}
	// the turning clips, 15 frames a second
	const uint32 now = g_system->getMillis();
	for (auto &o : _ovals) {
		if (!o.playing || !o.clip)
			continue;
		if ((int32)(now - o.nextFrame) < 0)
			continue;
		o.nextFrame += 66;
		if ((int32)(now - o.nextFrame) > 200)
			o.nextFrame = now + 66;
		advanceClip(o);
	}
	// the tooltips of the buttons (the BUTTON module answers 0x11a)
	int h = _tooltip.poll();
	if (h > 0) {
		for (const auto &t : kTooltips) {
			if (t.id != h)
				continue;
			if (_buttons.state(h) == SceneButtons::kStateDisabled)
				break;
			Common::Point p = _vm->mousePos();
			_tooltip.show(_vm->getString(t.key), p.x, p.y);
			break;
		}
	}
}

void RacemenuScene::pauseAnims(bool pause) {
	Scene::pauseAnims(pause);
	if (!pause) {
		for (auto &o : _ovals)
			o.nextFrame = g_system->getMillis() + 66;
	}
}

// FUN_0047bdb0 (event 0x116)
void RacemenuScene::onButton(int id) {
	switch (id) {
	case kButtonHelp:
		_vm->showHelp();
		break;
	case kButtonExit:
		_vm->endGame();
		break;
	case kButtonTrackPrev:
		stepTrack(-1);
		break;
	case kButtonTrackNext:
		stepTrack(1);
		break;
	case kButtonControls1:
		controlsDialog(0);
		break;
	case kButtonCar1Prev:
		_car1 = stepCar(-1, _car1, kOvalCar1, "lbl_car1name");
		break;
	case kButtonCar1Next:
		_car1 = stepCar(1, _car1, kOvalCar1, "lbl_car1name");
		break;
	case kButtonControls2:
		controlsDialog(1);
		break;
	case kButtonCar2Prev:
		_car2 = stepCar(-1, _car2, kOvalCar2, "lbl_car2name");
		break;
	case kButtonCar2Next:
		_car2 = stepCar(1, _car2, kOvalCar2, "lbl_car2name");
		break;
	case kButtonStart:
		startRace();
		break;
	case kButtonRaceTimes:
	case kButtonLapTimes:
		showTimes(id);
		break;
	case kButtonSound:
		soundDialog();
		break;
	default:
		break;
	}
}

// FUN_0047bf50
void RacemenuScene::stepTrack(int delta) {
	if (_tracks.empty())
		return;
	int i = wrapIndex(_track, _tracks.size(), delta);
	showItem(_tracks[i], "tracks", kOvalTrack, "lbl_trackname", 0);
	_track = i;
}

// FUN_0047bed0
int RacemenuScene::stepCar(int delta, int index, int oval, const char *labelName) {
	if (_cars.empty())
		return index;
	int i = wrapIndex(index, _cars.size(), delta);
	showItem(_cars[i], "cars", oval, labelName, itsTempoVersion());
	return i;
}

// FUN_0047bcf0: the info page of the oval's track or car ({kind, return
// scene, the hotspot as the return argument, award category 3 tracks or 2
// cars, the item})
void RacemenuScene::showInfo(int hotspot) {
	MenuState s;
	currentState(s);
	InfoRequest r;
	r.returnScene = "racemenu";
	r.returnArg = hotspot;
	if (hotspot == kHotspotTrack) {
		r.kind = "track";
		r.category = kAwardTrack;
		r.index = _tracks.empty() ? 0 : _tracks[s.track];
	} else {
		r.kind = "car";
		r.category = kAwardCar;
		r.index = _cars.empty() ? 0 : _cars[hotspot == kHotspotCar1 ? s.car1 : s.car2];
	}
	_vm->showInfo(r);
}

// FUN_0047bfa0: the race's settings (FUN_0047c000), the menu remembers the
// track and laps for its tables, and on to the race
void RacemenuScene::startRace() {
	if (_tracks.empty() || _cars.empty())
		return;
	MenuState s;
	currentState(s);
	RaceSettings &r = raceMenuSettings();
	fillSettings(s, r);
	RaceScoreState &st = raceScoreState();
	st.track = _tracks[s.track];
	st.laps = kLaps[CLIP<int>(s.slider[1], 0, 2)];
	st.valid = false;
	debug(1, "Racemenu: race on %s, %d laps, %d cars (%s, %s, %s, %s), mode %d, tricks %d", r.track, r.laps, r.carCount,
	      r.cars[0], r.cars[1], r.cars[2], r.cars[3], r.mode, r.tricks);
	_vm->changeScene("racing", 1);
}

// FUN_0047c000
void RacemenuScene::fillSettings(const MenuState &s, RaceSettings &r) {
	const int laps = CLIP<int>(s.slider[1], 0, 2);
	const int track = _tracks[s.track];
	// FUN_0047c100 / FUN_0047c0e0: the keys of the time tables
	r.raceKey = kLapsFlag[laps] | (uint32)track;
	r.lapKey = (uint32)track | 0x80000000;
	r.carCount = s.slider[2] + 1;
	r.laps = kLaps[laps];
	r.mode = kModes[CLIP<int>(s.slider[0], 0, 2)];
	if (r.mode == RaceSettings::kModeTwoPlayers && r.carCount < 2)
		r.carCount = 2;
	Common::String name = iniString(_ini, "tracks", Common::String::format("resource%d", track));
	if (name.empty())
		name = "unknown";
	Common::strlcpy(r.track, name.c_str(), sizeof(r.track));
	pickCars(s, r);
	r.tricks = s.tricks;
	r.players[0] = _controls[0];
	r.players[1] = _controls[1];
}

// FUN_0047c120: car 1 is player 1's, car 2 player 2's in the modes with two
// cars; the others are drawn at random from the first seven cars of
// race.ini less the players' (compared with their list indices, as in the
// original; an index not among them drops the first car, the memmove of
// FUN_0040a0b0 with index -1).
void RacemenuScene::pickCars(const MenuState &s, RaceSettings &r) {
	auto carName = [this](int resource) {
		Common::String n = iniString(_ini, "cars", Common::String::format("resource%d", resource));
		return n.empty() ? Common::String("unknown") : n;
	};
	for (int i = 0; i < 4; i++)
		r.cars[i][0] = '\0';
	int given = 1;
	Common::strlcpy(r.cars[0], carName(_cars[s.car1]).c_str(), sizeof(r.cars[0]));
	const int mode = kModes[CLIP<int>(s.slider[0], 0, 2)];
	if (r.carCount > 1 && mode > 0 && mode < 3) {
		given = 2;
		Common::strlcpy(r.cars[1], carName(_cars[s.car2]).c_str(), sizeof(r.cars[1]));
	}
	Common::Array<int> pool;
	for (int i = 0; i < 7; i++)
		pool.push_back(i);
	auto removeValue = [&pool](int value) {
		int index = -1;
		for (uint i = 0; i < pool.size(); i++)
			if (pool[i] == value) {
				index = i;
				break;
			}
		pool.remove_at(index < 0 ? 0 : index);
	};
	if (given == 2)
		removeValue(s.car2);
	removeValue(s.car1);
	// FUN_004120e0: rand() % n picks, without repeats
	Common::Array<int> order;
	while (!pool.empty()) {
		int k = _vm->getRandomNumber(pool.size() - 1);
		order.push_back(pool[k]);
		pool.remove_at(k);
	}
	for (int i = given; i < r.carCount && i < 4; i++)
		Common::strlcpy(r.cars[i], carName(order[i - given]).c_str(), sizeof(r.cars[i]));
}

// FUN_0047c2a0: the race times (laps of the slider) or the lap times of the
// track ("racing"'s tables), the last race's entries in bold
void RacemenuScene::showTimes(int id) {
	if (_tracks.empty())
		return;
	MenuState s;
	currentState(s);
	const int track = _tracks[s.track];
	const int laps = kLaps[CLIP<int>(s.slider[1], 0, 2)];
	Common::String key = iniString(_ini, "tracks", Common::String::format("name%d", track));
	if (key.empty())
		key = "unknown";
	Common::String name = _vm->getString(key);
	const RaceScoreState &st = raceScoreState();
	Common::String title;
	uint32 table;
	const ScoreEntry *entry = nullptr;
	const int *rows = nullptr;
	if (id == kButtonRaceTimes) {
		title = Common::String::format(_vm->getString("racemenu:HISCORE_RACETIME").c_str(), name.c_str(), laps);
		table = kLapsFlag[CLIP<int>(s.slider[1], 0, 2)] | (uint32)track;
		if (st.valid && st.track == track && st.laps == laps) {
			entry = &st.raceEntry;
			rows = st.raceRows;
		}
	} else {
		title = Common::String::format(_vm->getString("racemenu:HISCORE_LAPTIME").c_str(), name.c_str());
		table = (uint32)track | 0x80000000;
		if (st.valid && st.track == track) {
			entry = &st.lapEntry;
			rows = st.lapRows;
		}
	}
	showScoreTables(_vm, title, table, entry, rows, kScoreTime, "racing");
	refreshCursor();
}

// FUN_0047c400: the volumes of the race (value 0x29a of "racing", 0.75 by
// default), kept when the dialog closes with OK; the samples go.
void RacemenuScene::soundDialog() {
	float volumes[3] = { 0.75f, 0.75f, 0.75f };
	byte data[12];
	if (_vm->profile() && _vm->profile()->getRecord("racing", Profile::kRecordValue, 0x29a, data, sizeof(data))) {
		for (int i = 0; i < 3; i++)
			volumes[i] = READ_LE_FLOAT32(data + 4 * i);
	}
	RaceSoundDialog dlg(_vm, this, volumes);
	int result = dlg.run();
	if (result == 1 && _vm->profile()) {
		for (int i = 0; i < 3; i++)
			WRITE_LE_FLOAT32(data + 4 * i, volumes[i]);
		_vm->profile()->setRecord("racing", Profile::kRecordValue, 0x29a, data, sizeof(data));
		_vm->profileChanged();
	}
	for (int i = 0; i < 3; i++) {
		Anim *a = anim(kSoundClips[i]);
		if (a && a->isAdded())
			a->remove();
	}
	refreshCursor();
}

// FUN_0041d140(&controls, player): the "ctrlcfg" dialog edits the menu's
// copy of the player's controls (DAT_006cff84 / DAT_006cffe8), kept with
// the menu on close
void RacemenuScene::controlsDialog(int player) {
	ControlsDialog dlg(_vm, _controls[player], player);
	dlg.run();
	refreshCursor();
}

} // End of namespace Flaaklypa
