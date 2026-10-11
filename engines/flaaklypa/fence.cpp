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
#include "common/archive.h"
#include "common/config-manager.h"
#include "common/debug.h"
#include "common/events.h"
#include "common/savefile.h"
#include "common/system.h"
#include "common/util.h"

#include "flaaklypa/dialog.h"
#include "flaaklypa/entrydlg.h"
#include "flaaklypa/fence.h"
#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/listdlg.h"
#include "flaaklypa/music.h"
#include "flaaklypa/resources.h"
#include "flaaklypa/sound.h"

namespace Flaaklypa {

// Activity "Musikkgjerdet" (handler FUN_00466250): 0x103 init FUN_00466b90,
// 0x104 close FUN_00467650, 0x105 mouse down FUN_00467790 (returns 1 while
// the tune plays), 0x107 right button FUN_004680a0, 0x10a drag move
// FUN_00466870, 0x10b drop FUN_004663c0, 0x10c key FUN_00468330, 0x111
// timers FUN_00468380, 0x112 frame tick FUN_004683c0 (the birds), 0x116
// buttons FUN_00468130, 0x117 scroll bar FUN_00466b70.

enum {
	kHotspotCarPart = -1,        ///< the "carpart" element's hotspot
	kHotspotBoard = 1,           ///< the 18 fence boards, 1..18 = pitch 0..17
	kHotspotFree = 0x13,         ///< the mode buttons of the backdrop
	kHotspotPlay = 0x14,
	kHotspotRecord = 0x15,
	kHotspotStand = 0x17,        ///< the music stand: load a tune
	kHotspotInstrument = 0x18,   ///< the guitar and the instrument button
	kHotspotBlackboard = 0x19,   ///< the note names
	kHotspotPowerBox = 0x1a,     ///< the power box: a new tune
	kButtonTempoUp = 0x1b,
	kButtonTempoDown = 0x1c,
	kButtonLoad = 0x1d,
	kButtonSave = 0x1e,
	kButtonHelp = 0x1f,
	kButtonExit = 0x20,
	kHotspotSock = 0x21,         ///< scroll bar 0x21, its thumb
	kHotspotNote = 0x22,         ///< the 16 notes of the page, 0x22..0x31

	kNoteX = 0x32,               ///< the notes' columns: x = slot * 0x2b + 0x32
	kNoteSpacing = 0x2b,

	kZCarPart = -10,
	kZButtons = 0,
	kZSock = 0,                  ///< DAT_0053a1b0
	kZCursor = 1,
	kZLedger = 2,
	kZLights = 2,                ///< mode light, mike, player, letters, tempo
	kZDragged = 2,
	kZLetter = 3,
	kZLabels = 3,
	kZNote = 4,
	kZMarker = 5,
	kZTrash = 5,
	kZBirds = 6,                 ///< + the bird's number

	kTimerPlay = 0,              ///< DAT_006b3818: the next note
	kTimerMarker = 1,            ///< DAT_006b0448: the marker goes
	kMarkerTime = 250,

	kTempoDefault = 0x78,
	kTempoMin = 0x3c,
	kTempoMax = 0xf0,
	kTempoStep = 10,
	kUnitsPerBeat = 8,           ///< DAT_0049fea0: a crotchet

	kNewNoteLength = 2,          ///< a crotchet

	// The scroll bar (FUN_004090e0(0x21, 0x28, 0xe6, 0x2cb, 0x23, "sock",
	// 0x101)): horizontal (flag 0x10 clear), a transparent thumb
	kSockX = 0x28,
	kSockY = 0xe6,
	kSockW = 0x2cb,
	kSockH = 0x23,

	// FUN_00420690: the list dialog's box, which cuts the text rows
	kListW = 258 - 20,
	kListH = 410 - 61,

	kMaxInstruments = 0x18,
	kTitleLength = 0x20,         ///< the save dialog's limit
	kTitleFlags = 0x19,          ///< letters, digits and spaces
	kNoteBitmapW = 44,
	kNoteBitmapH = 84
};

// The labels (FUN_00407a30, Amerigo BT_10_ centred, z 3) and the tempo
// (FUN_00407930, z 2)
#define kFreeRect Common::Rect(0x111, 0x223, 0x14d, 0x247)    // 0x49fe58
#define kPlayRect Common::Rect(0x153, 0x223, 0x18f, 0x247)    // 0x49fe68
#define kRecordRect Common::Rect(0x194, 0x223, 0x1d0, 0x247)  // 0x49fe78
#define kTempoRect Common::Rect(0xd4, 0x224, 0xf3, 0x242)     // 0x49fe88

// The note lengths (0x508b84): bitmap/<length>-<up|down>
static const char *const kLengthNames[8] = {
	"quaver", "quaver-dot", "crotchet", "crotchet-dot", "minim", "minim-dot", "semibreve", "semibreve-dot"
};
// Their length in demisemiquavers... rather eighths of a beat (0x49fe98)
static const int kLengthUnits[8] = { 4, 6, 8, 12, 16, 24, 32, 48 };
// The clip a landing bird ends with (0x509248): animation/<clip>-<up|down>
static const char *const kLandClips[8] = {
	"crotchet", "crotchet", "crotchet", "crotchet", "minim", "minim", "semibreve", "semibreve"
};
// The note sounds (0x508ba4): sound/<instrument>-<pitch>.wav
static const char *const kPitchNames[18] = {
	"A1", "B1", "C1", "D1", "E1", "F1", "G1", "A2", "B2", "C2", "D2", "E2", "F2", "G2", "A3", "B3", "C3", "D3"
};
// The note names (0x508bec): bitmap/<letter> (lang container, B shows H)
static const char *const kPitchLetters[18] = {
	"A", "B", "C", "D", "E", "F", "G", "A", "B", "C", "D", "E", "F", "G", "A", "B", "C", "D"
};

// Where a bird's clips are drawn relative to the next one (0x509228):
// [up, down][x, y]. land-init -> land, then land -> the note clip.
static const int kLandOffset[2][2] = { { 0, 9 }, { 0, 3 } };      // 0x509228, 0x509230
static const int kNoteOffset[2][2] = { { 9, -7 }, { -9, -1 } };   // 0x509238, 0x509240

// FUN_00466810 / FUN_00466840: the y of a note bitmap; the wires sag 6
// pixels in the middle, each pitch is half a wire spacing (8.5) higher.
static int noteY(int x, int pitch) {
	const double u = (x - 395.0) / 367.0;
	return (int)(u * u * -6.0 + 182.0 - pitch * 8.5 - 42.0);
}

FenceScene::Note FenceScene::_notes[FenceScene::kMaxNotes];
int FenceScene::_count = 0;

FenceScene::FenceScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def), _buttons(this),
	_sounds(nullptr), _instrument(0), _birdsChannel(-1), _tempoLabel(nullptr), _tempo(kTempoDefault), _mode(kModeFree),
	_prevMode(kModeFree), _current(0), _page(0), _dragSlot(-1), _letters(false), _lastTick(0),
	_drag(kDragNone), _sockCreated(false), _sockMax(0), _sockValue(0), _sockW(0),
	_sockH(0), _birdCount(0), _leftHeld(false), _rightHeld(false) {
	for (int i = 0; i < kMaxBirds; i++) {
		_birds[i].anim = nullptr;
		_birds[i].pitch = _birds[i].length = 0;
		_birds[i].n = 0;
		_birds[i].t = _birds[i].speed = _birds[i].delay = 0.0f;
		_birds[i].state = 0;
	}
	// FUN_00413670(note, 2): the notes are hit on their whole rectangle
	_rectMask.create(kNoteBitmapW, kNoteBitmapH, Graphics::PixelFormat::createFormatCLUT8());
	memset(_rectMask.getPixels(), 1, kNoteBitmapW * kNoteBitmapH);
}

FenceScene::~FenceScene() {
	delete _sounds;
	_rectMask.free();
}

// ---- init / close ---------------------------------------------------------------

// FUN_00466b90
void FenceScene::onInit(int arg) {
	_vm->_music->stop();
	// The sounds play at the engine's sound volume (sound.ini included).
	_sounds = new SoundPlayer(_name);
	_birdsChannel = -1;
	_birdCount = 0;
	_lastTick = _vm->getGameMillis();
	_page = 0;
	_current = 0;
	_letters = false;
	_dragSlot = -1;
	_drag = kDragNone;
	_leftHeld = _rightHeld = false;
	resources()->loadIni(_name, "instrument.ini", _instrumentIni);
	loadInstrument(0);

	// Free mode at once: the scroll bar does not exist yet, the notes of
	// the tune left from the last visit show (and fly away below)
	enterFree();

	_font10.load("Amerigo BT_10_");
	_font14.load("Amerigo BT_14_");
	_buttons.setFont(&_font10);
	_buttons.setDefaultZ(kZButtons);
	_buttons.add(kButtonHelp, 0, 0, "help_0", "help_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonExit, 0x2d8, 0, "exit_0", "exit_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonTempoUp, 0xf9, 0x222, "bpminc_0", "bpminc_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonTempoDown, 0xbf, 0x224, "bpmdec_0", "bpmdec_1", nullptr, nullptr, "", true);
	_buttons.add(kHotspotInstrument, 0x1e3, 0x224, "instruments_0", "instruments_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonLoad, 0x229, 0x222, "load_0", "load_1", nullptr, nullptr, _vm->getString("fence:LOAD"), true);
	_buttons.add(kButtonSave, 0x229, 0x23d, "save_0", "save_1", nullptr, nullptr, _vm->getString("fence:SAVE"), true);

	// FUN_00408c70 / FUN_004090e0: the sock, range 0..0, at the left end
	const Graphics::ManagedSurface *sock = _buttons.bitmap("sock");
	_sockW = sock ? sock->w : 0;
	_sockH = sock ? sock->h : 0;
	_sockCreated = true;
	_sockMax = _sockValue = 0;
	sockSetValue(0);
	updateSock();

	const uint32 key = _vm->_screen->format.RGBToColor(0, 0xff, 0);
	const struct {
		const char *name;
		Common::Rect rect;
		const char *text;
		int z;
	} labels[] = {
		{ "lblfree", kFreeRect, "fence:FREE", kZLabels },
		{ "lblplay", kPlayRect, "fence:PLAY", kZLabels },
		{ "lblrecord", kRecordRect, "fence:RECORD", kZLabels },
		{ "lbltempo", kTempoRect, nullptr, kZLights }
	};
	for (const auto &l : labels) {
		Anim *a = defineSurfaceAnim(l.name, l.rect.width(), l.rect.height(), key);
		if (l.text)
			_font10.drawStringCentred(*a->surface(), Common::Rect(l.rect.width(), l.rect.height()), _vm->getString(l.text));
		else
			_tempoLabel = a;
		a->add(l.rect.left, l.rect.top, l.z);
	}

	changeTune(nullptr);

	if (!carPartFound())
		addAnim("carpart", Anim::kDefaultPos, Anim::kDefaultPos, kZCarPart);
}

// FUN_00467650 / FUN_004676b0(1): every sound stops
void FenceScene::onClose() {
	delete _sounds;
	_sounds = nullptr;
	_birdsChannel = -1;
}

// ---- modes ------------------------------------------------------------------------

// FUN_004674c0: the light of a mode button (bitmap/<mode>_1, z 2); other
// values only switch it off
void FenceScene::showModeLight(int hotspot) {
	Anim *a = findAnimOrNull("modelight");
	if (a && a->isAdded())
		a->remove();
	if (hotspot == kHotspotFree)
		showElement("modelight", "free_1", 0, 0x112, 0x224, kZLights);
	else if (hotspot == kHotspotPlay)
		showElement("modelight", "play_1", 0, 0x153, 0x225, kZLights);
	else if (hotspot == kHotspotRecord)
		showElement("modelight", "edit_1", 0, 0x192, 0x223, kZLights);
}

// FUN_004675b0 / FUN_004675d0 / FUN_00467620
void FenceScene::leaveMode() {
	switch (_mode) {
	case kModeFree:
		_prevMode = kModeFree;
		showModeLight(-1);
		break;
	case kModePlay:
		showModeLight(-1);
		if (isAnimAdded("player"))
			removeAnim("player");
		killTimer(kTimerPlay);
		break;
	case kModeRecord:
		_prevMode = kModeRecord;
		showModeLight(-1);
		if (isAnimAdded("mike"))
			removeAnim("mike");
		break;
	default:
		break;
	}
}

// FUN_00467480 + FUN_004674a0: back to the first note
void FenceScene::enterFree() {
	debug(1, "Fence: free mode");
	_mode = kModeFree;
	showModeLight(kHotspotFree);
	_page = 0;
	_current = 0;
	updateSock();
	showPage();
	showCursorMark();
}

// FUN_00467f00: the play button stops a running tune
void FenceScene::enterPlay() {
	if (_mode == kModePlay) {
		if (_prevMode == kModeFree) {
			enterFree();
			return;
		}
		if (_prevMode == kModeRecord) {
			enterRecord();
			return;
		}
	}
	debug(1, "Fence: playing %d notes at %d bpm", _count, _tempo);
	_mode = kModePlay;
	showModeLight(kHotspotPlay);
	if (!isAnimAdded("player"))
		addAnim("player", Anim::kDefaultPos, Anim::kDefaultPos, kZLights);
	_page = 0;
	_current = 0;
	updateSock();
	showPage();
	showCursorMark();
	_current = 0;
	playStep();
}

// FUN_00468020
void FenceScene::enterRecord() {
	debug(1, "Fence: record mode");
	_mode = kModeRecord;
	showModeLight(kHotspotRecord);
	if (!isAnimAdded("mike"))
		addAnim("mike", Anim::kDefaultPos, Anim::kDefaultPos, kZLights);
	scrollToCurrent();
	showPage();
	showCursorMark();
}

// FUN_004673b0: 60..240 beats per minute, shown centred
void FenceScene::setTempo(int tempo) {
	if (tempo < kTempoMax) {
		if (tempo < kTempoMin + 1)
			tempo = kTempoMin;
	} else {
		tempo = kTempoMax;
	}
	_tempo = tempo;
	if (!_tempoLabel || !_tempoLabel->surface())
		return;
	Graphics::ManagedSurface *s = _tempoLabel->surface();
	s->fillRect(Common::Rect(s->w, s->h), _vm->_screen->format.RGBToColor(0, 0xff, 0));
	_font10.drawStringCentred(*s, Common::Rect(s->w, s->h), Common::String::format("%d", tempo));
}

// FUN_00467790 case 0x19: the note names on the boards and the staff
void FenceScene::toggleLetters() {
	_letters = !_letters;
	if (_letters)
		addAnim("letters", Anim::kDefaultPos, Anim::kDefaultPos, kZLights);
	else if (isAnimAdded("letters"))
		removeAnim("letters");
	showPage();
}

// FUN_00467790 (event 0x105)
void FenceScene::handleClick(int hotspot) {
	if (_mode < kModeFree)
		return;
	const bool editable = _mode == kModeFree || _mode == kModeRecord;
	if (hotspot >= kHotspotBoard && hotspot < kHotspotBoard + kPitches) {
		boardPressed(hotspot - kHotspotBoard);
		return;
	}
	if (hotspot >= kHotspotNote && hotspot < kHotspotNote + kSlots) {
		// FUN_00468070: a note of the page is picked up
		if (_mode == kModeRecord) {
			const int slot = hotspot - kHotspotNote;
			_dragSlot = slot;
			startDrag(kDragNote);
			_current = _page + slot;
			showCursorMark();
		}
		return;
	}
	switch (hotspot) {
	case kHotspotFree:
	case kHotspotPlay:
	case kHotspotRecord:
		leaveMode();
		if (hotspot == kHotspotFree)
			enterFree();
		else if (hotspot == kHotspotPlay)
			enterPlay();
		else
			enterRecord();
		break;
	case kHotspotStand:
		if (editable)
			chooseTune();
		break;
	case kHotspotInstrument:
		if (editable)
			chooseInstrument();
		break;
	case kHotspotBlackboard:
		if (editable)
			toggleLetters();
		break;
	case kHotspotPowerBox:
		if (editable)
			changeTune(nullptr);
		break;
	case kButtonTempoUp:
		setTempo(_tempo + kTempoStep);
		break;
	case kButtonTempoDown:
		setTempo(_tempo - kTempoStep);
		break;
	case kHotspotCarPart:
		takeCarPart();
		break;
	default:
		break;
	}
}

// FUN_00468130 (event 0x116)
void FenceScene::handleButton(int id) {
	if (_mode != kModeFree && _mode != kModeRecord)
		return;
	switch (id) {
	case kButtonLoad:
		chooseTune();
		break;
	case kButtonSave:
		saveTune();
		break;
	case kButtonHelp:
		_vm->showHelp();
		break;
	case kButtonExit:
		_vm->endGame();
		break;
	default:
		break;
	}
}

// FUN_00467bc0: a board (or a digit key) pressed
void FenceScene::boardPressed(int pitch) {
	if (_mode < kModeFree)
		return;
	if (_mode == kModeFree) {
		// The player follows the tune: the right note moves the cursor on
		playNote(pitch);
		if (_current < _count && pitch == _notes[_current].pitch)
			advance();
	} else if (_mode == kModePlay) {
		playNote(pitch);
	} else if (_mode == kModeRecord && _count < kMaxNotes) {
		// A crotchet after the current note
		if (_count < 1)
			_current = 0;
		else
			_current++;
		scrollToCurrent();
		const int at = _current;
		for (int i = _count; i > at; i--)
			_notes[i] = _notes[i - 1];
		_count++;
		_notes[at].pitch = pitch;
		_notes[at].length = kNewNoteLength;
		playNote(pitch);
		showPage();
		showCursorMark();
	}
}

// FUN_00467dc0: the note sounds (each one in a channel of its own, so they
// ring on and mix) and a marker shows the pitch in the current note's column
void FenceScene::playNote(int pitch) {
	if (!_noteSounds[pitch].empty() && _sounds)
		_sounds->play(_noteSounds[pitch]);
	debug(2, "Fence: note %s t=%u", kPitchNames[pitch], _vm->getGameMillis());
	removeMarker();
	const int column = _current > _page ? _current - _page : 0;
	const int x = column * kNoteSpacing + kNoteX;
	showElement("marker", pitch > 7 ? "marker-down" : "marker-up", 0, x, noteY(x, pitch), kZMarker);
	setTimer(kTimerMarker, kMarkerTime);
}

// FUN_00467f70: plays the current note and waits its length
bool FenceScene::playStep() {
	const Note &note = _notes[_current];
	playNote(note.pitch);
	const int ms = kLengthUnits[note.length] * 60000 / kUnitsPerBeat / _tempo;
	if (advance()) {
		setTimer(kTimerPlay, ms);
		return true;
	}
	if (_mode == kModePlay) {
		leaveMode();
		if (_prevMode == kModeFree)
			enterFree();
		else
			enterRecord();
	}
	return false;
}

// FUN_00467cb0: the next note; false at the end of the tune
bool FenceScene::advance() {
	_current++;
	if (_current >= _count) {
		removeMarker();
		return false;
	}
	if (_current >= _page + kSlots) {
		_page += kSlots;
		if (_page >= _count) {
			_page = 0;
			_current = 0;
		}
		Anim *marker = findAnimOrNull("marker");
		if (marker && marker->isAdded()) {
			if (_current - _page == 0) {
				removeMarker();
			} else {
				// The original takes the pitch of the note before (0x6b6228 + current * 8)
				const int x = (_current - _page) * kNoteSpacing + 7;
				marker->setPos(x, noteY(x, _notes[_current - 1].pitch));
			}
		}
		updateSock();
		showPage();
		showCursorMark();
		return _page != 0;
	}
	showCursorMark();
	return true;
}

// ---- input ---------------------------------------------------------------------------

void FenceScene::onMouseDown(int hotspot, int x, int y) {
	// The car part's hotspot is -1, which the engine's hit test skips; it
	// lies over the mask only.
	Anim *carPart = anim("carpart");
	if (carPart->isAdded() && carPart->hitTest(x, y) == kHotspotCarPart && _mask &&
	    x >= 0 && y >= 0 && x < _mask->w && y < _mask->h && hotspot == *(const byte *)_mask->getBasePtr(x, y))
		hotspot = kHotspotCarPart;
	handleClick(hotspot);
	// While the tune plays the handler swallows the press: the BUTTON and
	// SCROLL modules (priority -10) do not see it.
	if (_mode == kModePlay)
		return;
	_buttons.mouseDown(hotspot, x, y);
	sockMouseDown(hotspot, x, y);
}

void FenceScene::onMouseUp(int hotspot, int x, int y) {
	// The last move before the release (event 0x10a)
	if (_drag != kDragNone && Common::Point(x, y) != _dragMouse) {
		_dragMouse = Common::Point(x, y);
		dragMove(x, y);
		if (_drag == kDragSock && sockSetPos(x))
			scrolled();
	}
	// Event 0x106: the BUTTON module reports a click (0x116)
	int id = _buttons.mouseUp();
	if (id)
		handleButton(id);
	// Then the EVENT module ends the drag (0x10b): the fence first, then
	// the scroll bar's thumb snaps to its value (FUN_004092b0)
	if (_drag != kDragNone) {
		const DragKind kind = _drag;
		_drag = kDragNone;
		drop();
		if (kind == kDragSock && sockSetValue(_sockValue))
			scrolled();
	}
}

void FenceScene::onMouseMove(int hotspot, int x, int y) {
	_buttons.setHover(hotspot);
}

// FUN_004680a0: the right button changes a note's length while composing;
// with Shift it toggles the dot, else it doubles (a semibreve becomes a quaver)
void FenceScene::onRightClick(int x, int y) {
	const int hotspot = hotspotAt(x, y);
	if (_mode != kModeRecord || hotspot < kHotspotNote || hotspot >= kHotspotNote + kSlots)
		return;
	_current = _page + hotspot - kHotspotNote;
	Note &note = _notes[_current];
	if (g_system->getEventManager()->getModifierState() & Common::KBD_SHIFT)
		note.length += (note.length & 1) ? -1 : 1;
	else
		note.length = (note.length + 2) & 7;
	showPage();
	showCursorMark();
}

// FUN_00468330: the digits play boards 3..11; with the left arrow held
// 1..9, with the right one 10..18
void FenceScene::onKey(const Common::KeyState &key) {
	if (key.keycode == Common::KEYCODE_LEFT)
		_leftHeld = true;
	else if (key.keycode == Common::KEYCODE_RIGHT)
		_rightHeld = true;
	if (_mode < kModeFree)
		return;
	const int base = _leftHeld ? 0 : (_rightHeld ? 9 : 2);
	if (key.keycode >= Common::KEYCODE_1 && key.keycode <= Common::KEYCODE_9)
		boardPressed(base + key.keycode - Common::KEYCODE_1);
}

void FenceScene::onKeyUp(const Common::KeyState &key) {
	if (key.keycode == Common::KEYCODE_LEFT)
		_leftHeld = false;
	else if (key.keycode == Common::KEYCODE_RIGHT)
		_rightHeld = false;
}

// FUN_00468380
void FenceScene::onTimer(int id, int data) {
	if (id == kTimerPlay)
		playStep();
	else if (id == kTimerMarker)
		removeMarker();
}

// FUN_004683c0 (event 0x112) and the drags (event 0x10a on each move)
void FenceScene::onUpdate() {
	const uint32 now = _vm->getGameMillis();
	if (_mode < kModeFree)
		updateBirds((float)(now - _lastTick) * 0.001f);
	_lastTick = now;

	const Common::Point m = _vm->mousePos();
	if (_drag != kDragNone && m != _dragMouse) {
		_dragMouse = m;
		dragMove(m.x, m.y);
		if (_drag == kDragSock && sockSetPos(m.x))
			scrolled();
	}
}

// ---- the staff ---------------------------------------------------------------------

Common::String FenceScene::slotName(int slot) const {
	return Common::String::format("note%d", slot);
}

Anim *FenceScene::findAnimOrNull(const char *name) {
	return _defined.contains(name) ? anim(name) : nullptr;
}

Anim *FenceScene::showElement(const Common::String &elem, const Common::String &file, int hotspot, int x, int y, int z) {
	_defined.setVal(elem, true);
	return _buttons.showBitmap(elem, file, true, hotspot, x, y, z);
}

void FenceScene::removeElement(const Common::String &elem) {
	Anim *a = findAnimOrNull(elem.c_str());
	if (a && a->isAdded())
		a->remove();
}

void FenceScene::removeMarker() {
	removeElement("marker");
}

// FUN_00412cf0: an element centred on a point
void FenceScene::centreAnim(Anim *a, int x, int y) {
	const Common::Rect r = a->rect();
	a->setPos(x - r.width() / 2, y - r.height() / 2);
}

// The note of a slot (bitmap/<length>-<up|down>, stem down from the middle
// line up) at its column, hit tested on its rectangle
void FenceScene::showNote(int slot, int index, int z) {
	const Note &note = _notes[index];
	const int x = slot * kNoteSpacing + kNoteX;
	const int y = noteY(x, note.pitch);
	Anim *a = showElement(slotName(slot), Common::String::format("%s-%s", kLengthNames[note.length], note.pitch > 7 ? "down" : "up"),
		kHotspotNote + slot, x, y, z);
	if (a)
		a->setHitMask(&_rectMask);
	_slotPos[slot] = Common::Point(x, y);
}

// The note name (z 3) and the ledger lines (bitmap/line, z 2) of a slot
void FenceScene::showExtras(int slot, int pitch, int x) {
	if (_letters)
		showElement(Common::String::format("letter%d", slot), kPitchLetters[pitch], 0, x, noteY(x, pitch), kZLetter);
	if (pitch > 13) {
		showElement(Common::String::format("ledger%d_0", slot), "line", 0, x, noteY(x, 14), kZLedger);
		if (pitch > 15)
			showElement(Common::String::format("ledger%d_1", slot), "line", 0, x, noteY(x, 16), kZLedger);
	}
	if (pitch < 3) {
		showElement(Common::String::format("ledger%d_0", slot), "line", 0, x, noteY(x, 2), kZLedger);
		if (pitch < 1)
			showElement(Common::String::format("ledger%d_1", slot), "line", 0, x, noteY(x, 0), kZLedger);
	}
}

void FenceScene::removeSlot(int slot, bool note) {
	removeElement(Common::String::format("ledger%d_0", slot));
	removeElement(Common::String::format("ledger%d_1", slot));
	removeElement(Common::String::format("letter%d", slot));
	if (note)
		removeElement(slotName(slot));
}

// FUN_004665a0: the notes of the page
void FenceScene::showPage() {
	for (int slot = 0; slot < kSlots; slot++)
		removeSlot(slot, true);
	const int end = MIN(_page + (int)kSlots, _count);
	for (int i = _page; i < end; i++) {
		const int slot = i - _page;
		showNote(slot, i, kZNote);
		showExtras(slot, _notes[i].pitch, slot * kNoteSpacing + kNoteX);
	}
}

// FUN_004664f0: the frame round the current note, when it is on the page
void FenceScene::showCursorMark() {
	if (_mode == kModeFree || _mode == kModeRecord) {
		const int end = MIN(_page + (int)kSlots, _count);
		if (_page <= _current && _current < end) {
			const Common::Point &p = _slotPos[_current - _page];
			if (!isAnimAdded("cursor"))
				addAnim("cursor", p.x, p.y, kZCursor);
			else
				anim("cursor")->setPos(p.x, p.y);
			return;
		}
	}
	if (isAnimAdded("cursor"))
		removeAnim("cursor");
}

// FUN_00466460: the page of the current note
void FenceScene::scrollToCurrent() {
	if (_current < _page)
		_page -= ((_page - _current + 15) >> 4) * kSlots;
	while (_page + kSlots <= _current)
		_page += kSlots;
	if (_page < 1)
		_page = 0;
	updateSock();
}

// ---- dragging notes -----------------------------------------------------------------

// FUN_0040c1f0
void FenceScene::startDrag(DragKind kind) {
	_drag = kind;
	_dragMouse = _vm->mousePos();
}

// FUN_00466870 (event 0x10a): the note follows the mouse up and down the
// staff; below the lowest place it goes to the bin
void FenceScene::dragMove(int x, int y) {
	if (_dragSlot < 0)
		return;
	const int index = _page + _dragSlot;
	Note &note = _notes[index];
	const int nx = _dragSlot * kNoteSpacing + kNoteX;
	Anim *a = anim(slotName(_dragSlot).c_str());
	const int top = y - a->rect().height() / 2;
	int pitch = 0;
	while (pitch < kPitches && noteY(nx, pitch) >= top)
		pitch++;
	if (pitch > 16)
		pitch = 17;
	note.pitch = pitch;
	a->remove();
	showNote(_dragSlot, index, kZDragged);
	a = anim(slotName(_dragSlot).c_str());
	if (noteY(nx, -1) < top) {
		if (!isAnimAdded("trash"))
			addAnim("trash", x, y, kZTrash);
		centreAnim(a, x, y);
		_slotPos[_dragSlot] = Common::Point(a->x(), a->y());
		centreAnim(anim("trash"), x, y);
	} else {
		removeSlot(_dragSlot, false);
		if (isAnimAdded("trash"))
			removeAnim("trash");
		showExtras(_dragSlot, pitch, nx);
	}
}

// FUN_004663c0 (event 0x10b, for any drag): a note dropped in the bin is
// deleted
void FenceScene::drop() {
	if (isAnimAdded("trash")) {
		_count--;
		for (int i = _dragSlot + _page; i < _count; i++)
			_notes[i] = _notes[i + 1];
		if (_current < 1)
			_current = 0;
		else if (_current >= _count)
			_current = _count - 1;
		scrollToCurrent();
		removeAnim("trash");
	}
	_dragSlot = -1;
	showPage();
	showCursorMark();
}

// ---- the sock on the clothes line ---------------------------------------------------
//
// The SCROLL module's bar 0x21 (FUN_004090e0 with flags 0x101: horizontal,
// a transparent thumb with the bar's id as its hotspot). Its value is the
// page; the original posts event 0x117 whenever the value changes, which
// scrolled() handles (FUN_00466b70).

// FUN_004664b0: one more page than the notes fill
void FenceScene::updateSock() {
	if (!_sockCreated)
		return;
	// FUN_00408e20(0x21, 0, max)
	_sockMax = MAX((_count - 16) / 16 + 1, 0);
	if (sockSetValue(_page / 16))
		scrolled();
}

// FUN_00408eb0
bool FenceScene::sockSetValue(int value) {
	value = CLIP(value, 0, _sockMax);
	if (_sockMax > 0)
		return sockSetPos(kSockW * value / _sockMax + kSockX);
	return sockSetPos(0);
}

// FUN_00408f30: the thumb's centre at pos on the line, the value rounded
bool FenceScene::sockSetPos(int pos) {
	pos = CLIP(MAX(pos, 0), (int)kSockX, (int)(kSockX + kSockW));
	const double v = (double)((pos - kSockX) * _sockMax) / kSockW;
	const int value = (int)(v > 0.0 ? v + 0.5 : v - 0.5);
	Anim *a = findAnimOrNull("sock");
	if (!a || !a->isAdded())
		a = showElement("sock", "sock", kHotspotSock, 0, 0, kZSock);
	if (a)
		centreAnim(a, pos, kSockH / 2 + kSockY);
	const bool changed = value != _sockValue;
	_sockValue = value;
	return changed;
}

// FUN_004093f0 / FUN_004092d0: a press on the thumb drags it, one on the
// line (widened by half the thumb) moves it there first
void FenceScene::sockMouseDown(int hotspot, int x, int y) {
	if (!_sockCreated)
		return;
	if (hotspot == kHotspotSock) {
		startDrag(kDragSock);
		return;
	}
	const Common::Rect bar(kSockX - _sockW / 2, kSockY, kSockX - _sockW / 2 + kSockW + _sockW, kSockY + kSockH);
	Anim *thumb = findAnimOrNull("sock");
	if (!bar.contains(x, y) && !(thumb && thumb->isAdded() && thumb->rect().contains(x, y)))
		return;
	if (sockSetPos(x))
		scrolled();
	startDrag(kDragSock);
}

// FUN_00466b70 (event 0x117)
void FenceScene::scrolled() {
	_page = _sockValue << 4;
	showPage();
	showCursorMark();
}

// ---- birds ---------------------------------------------------------------------------

// (0.5 - rand() / 32767): -0.5..0.5
float FenceScene::randomUnit() {
	return 0.5f - (float)_vm->getRandomNumber(32767) * (1.0f / 32767.0f);
}

// Each bird keeps its own elements: "fbird<kind><n>/../<clip>" resolves to
// animation/<clip>.smk or bitmap/<clip>.bmp (the original renames the
// bird's animation structure); kind b = bitmap, s = clip, l = looping clip.
void FenceScene::birdClip(int index, const Common::String &clip, bool smacker, bool loop) {
	Bird &b = _birds[index];
	if (b.anim && b.anim->isAdded())
		b.anim->remove();
	const Common::String name = Common::String::format("fbird%c%d/../%s", smacker ? (loop ? 'l' : 's') : 'b', index, clip.c_str());
	b.anim = defineAnim(name.c_str(), smacker, true, 0, 0, 0, 0, true, loop);
}

// FUN_004670e0: a note of the page turns into a bird that waits 0..3 s
// and flies off to the left (stem up) or right along a parabola
void FenceScene::birdTakeOff(int x, int y, const Note &note) {
	const int index = _birdCount;
	Bird &b = _birds[index];
	const bool up = note.pitch < 8;
	b.n = _vm->getRandomNumber(9) + 10;
	for (int i = 0; i <= b.n; i++) {
		const float s = (float)i / (float)(b.n - 1);
		const int k = up ? 825 : -825;   // 0x49fed8
		b.points[i].x = x - (int)(pow((double)s, 2.0) * k);
		b.points[i].y = y - (int)(randomUnit() * 10.0f + 200.0f * s);
	}
	b.points[0] = Common::Point(x, y);
	b.pitch = note.pitch;
	b.length = note.length;
	b.t = 0.0f;
	b.speed = randomUnit() * 0.2f + 0.4f;
	b.delay = randomUnit() * 3.0f + 1.5f;
	b.state = 5;
	birdClip(index, Common::String::format("%s-%s", kLengthNames[note.length], up ? "up" : "down"), false, false);
	b.anim->add(b.points[0].x, b.points[0].y, kZBirds + index);
	_birdCount++;
}

// FUN_004685a0: a bird that waits 0..4 s and comes flying from the right
// (stem up) or left, from above, to land as the note at (x, y)
void FenceScene::birdLand(int x, int y, const Note &note) {
	const int index = _birdCount;
	Bird &b = _birds[index];
	const int ampX = (int)(randomUnit() * (400 / 2)) + 400;   // 0x49fed0
	const int ampY = (int)(randomUnit() * (300 / 2)) + 300;   // 0x49fed4
	const int u = note.pitch < 8 ? 0 : 1;
	const int ex = x + kNoteOffset[u][0] + kLandOffset[u][0];
	const int ey = y + kNoteOffset[u][1] + kLandOffset[u][1];
	b.n = _vm->getRandomNumber(9) + 10;
	for (int i = 0; i < b.n; i++) {
		const float s = 1.0f - (float)i / (float)(b.n - 1);
		b.points[i].x = ex - (int)((float)(u == 0 ? -ampX : ampX) * s);
		b.points[i].y = ey - (int)(pow((double)s, 2.0) * ampY);
	}
	b.points[b.n] = b.points[b.n - 1] = Common::Point(ex, ey);
	b.pitch = note.pitch;
	b.length = note.length;
	b.t = 0.0f;
	b.speed = randomUnit() * 0.2f + 0.4f;
	b.delay = randomUnit() * 4.0f + 2.0f;
	b.state = 0;
	birdClip(index, u == 0 ? "fly-up" : "fly-down", true, true);
	b.anim->add(b.points[0].x, b.points[0].y, kZBirds + index);
	b.anim->play();
	_birdCount++;
}

// FUN_00468870: one bird. States: 0 waiting to land, 1 flying in, 2
// about to land (land-init), 3 landing, 4 settling as the note; 5 waiting
// to leave (the note bitmap), 6 taking off, 7 flying away. Returns false
// once the bird is gone.
bool FenceScene::birdUpdate(int index, float dt) {
	Bird &b = _birds[index];
	if (!b.anim || !b.anim->isAdded())
		return false;
	const int toEnd = ABS(b.anim->x() - b.points[b.n - 1].x);
	const int fromStart = ABS(b.points[0].x - b.anim->x());
	const int u = b.pitch < 8 ? 0 : 1;
	switch (b.state) {
	case 0:
		b.delay -= dt;
		if (b.delay <= 0.0f)
			b.state = 1;
		break;
	case 1:
		if (b.t < 1.0f)
			b.t += dt * b.speed;
		if (toEnd < 150) {
			birdClip(index, u == 0 ? "land-init-up" : "land-init-down", true, false);
			b.anim->play();
			b.state = 2;
		}
		break;
	case 2:
		if (b.t < 1.0f)
			b.t += dt * b.speed;
		if (toEnd == 0) {
			birdClip(index, u == 0 ? "land-up" : "land-down", true, false);
			b.points[b.n - 1].x -= kLandOffset[u][0];
			b.points[b.n - 1].y -= kLandOffset[u][1];
			b.points[b.n] = b.points[b.n - 1];
			b.anim->play();
			b.state = 3;
		}
		break;
	case 3:
		if (toEnd == 0 && !b.anim->isPlaying()) {
			birdClip(index, Common::String::format("%s-%s", kLandClips[b.length], u == 0 ? "up" : "down"), true, false);
			b.points[b.n - 1].x -= kNoteOffset[u][0];
			b.points[b.n - 1].y -= kNoteOffset[u][1];
			b.points[b.n] = b.points[b.n - 1];
			b.anim->play();
			b.state = 4;
		}
		break;
	case 4:
		return true;
	case 5:
		b.delay -= dt;
		if (b.delay <= 0.0f) {
			birdClip(index, u == 0 ? "land-init-up" : "land-init-down", true, true);
			b.points[0].x -= kNoteOffset[u][0] + kLandOffset[u][0];
			b.points[0].y -= kNoteOffset[u][1] + kLandOffset[u][1];
			b.anim->play();
			b.state = 6;
		}
		break;
	case 6:
		if (b.t < 1.0f)
			b.t += dt * b.speed;
		if (fromStart > 100) {
			birdClip(index, u == 0 ? "fly-up" : "fly-down", true, true);
			b.anim->play();
			b.state = 7;
		}
		break;
	case 7:
		if (b.t < 1.0f)
			b.t += dt * b.speed;
		if (b.t >= 1.0f) {
			b.anim->remove();
			return false;
		}
		break;
	default:
		break;
	}
	// The position on the path: straight lines between its points
	const float f = (float)(b.n - 1) * b.t;
	const int seg = CLIP((int)f, 0, b.n - 1);
	const double frac = fmod((double)f, 1.0);
	const Common::Point &p0 = b.points[seg], &p1 = b.points[seg + 1];
	b.anim->setPos((int)((p1.x - p0.x) * frac) + p0.x, (int)((p1.y - p0.y) * frac) + p0.y);
	return true;
}

// FUN_00468420: when no bird is busy, the landed ones become the notes
// (free mode), or the new tune's birds come once the old ones are gone
void FenceScene::updateBirds(float dt) {
	int busy = 0;
	for (int i = 0; i < _birdCount; i++) {
		Bird &b = _birds[i];
		const bool always = b.state == 2 || b.state == 3 || b.state == 5;
		if ((always || (b.anim && b.anim->isPlaying())) && birdUpdate(i, dt))
			busy++;
	}
	if (_birdCount > 0 && busy)
		return;
	if (_mode == kModeFlyIn) {
		for (int i = 0; i < _birdCount; i++)
			if (_birds[i].anim && _birds[i].anim->isAdded())
				_birds[i].anim->remove();
		_birdCount = 0;
		showCursor();
		if (_sounds && _sounds->isPlaying(_birdsChannel))
			_sounds->stop(_birdsChannel);
		_birdsChannel = -1;
		enterFree();
	} else if (_mode == kModeFlyAway) {
		flyIn();
	}
}

// FUN_00467060: the notes of the page fly away (wait cursor, bird song)
void FenceScene::flyAway() {
	_birdCount = 0;
	for (int slot = 0; slot < kSlots; slot++) {
		Anim *a = findAnimOrNull(slotName(slot).c_str());
		if (a && a->isAdded())
			birdTakeOff(_slotPos[slot].x, _slotPos[slot].y, _notes[_page + slot]);
	}
	showWaitCursor();
	if (_sounds && !_sounds->isPlaying(_birdsChannel))
		_birdsChannel = _sounds->play("birds", true);
	_mode = kModeFlyAway;
}

// FUN_00468520: the first 16 notes of the tune come flying
void FenceScene::flyIn() {
	_birdCount = 0;
	for (int i = 0; i < _count && i < kSlots; i++) {
		const int x = i * kNoteSpacing + kNoteX;
		birdLand(x, noteY(x, _notes[i].pitch), _notes[i]);
	}
	if (_sounds && !_sounds->isPlaying(_birdsChannel))
		_birdsChannel = _sounds->play("birds", true);
	_mode = kModeFlyIn;
}

// ---- tunes ----------------------------------------------------------------------------
//
// A .tun file (FUN_004672e0 / FUN_00468260): u8 0xff, char title[0x400]
// (NUL terminated; the original writes the rest of its stack buffer), u8
// beats per minute, u8 number of notes, then per note int32 pitch (0..17)
// and int32 length (0..7), little endian. The count is a byte, so a tune
// of more than 255 notes loses notes when it is loaded again (original
// behaviour).
//
// The original lists data/fence/tunes/*.tun on disk and saves to the first
// free tunes/tune<n>.tun there. The port lists the tunes of the scene's
// containers (tune0..9, the language container's before the data one's),
// loose files in data/fence/tunes of the game directory (an installed
// original with its players' tunes) and the save files
// <target>-fence-tune<n>.tun, which is where it saves.

// FUN_00466ee0: the notes of the page fly away; nullptr starts an empty
// tune ("fence:DEFTUNE", 120 bpm), else the tune is loaded
void FenceScene::changeTune(const TuneFile *tune) {
	leaveMode();
	flyAway();
	if (!tune) {
		_count = 0;
		setTempo(kTempoDefault);
		_tuneName = _vm->getString("fence:DEFTUNE");
	} else {
		Common::SeekableReadStream *s = openTune(*tune);
		const bool ok = s && readTune(*s);
		delete s;
		debug(1, "Fence: tune '%s' (%s): %s, %d notes at %d bpm", tune->title.c_str(), tune->path.c_str(),
			ok ? "loaded" : "failed", _count, _tempo);
		if (!ok) {
			_vm->messageBox("fence:IOERR_TITLE", Common::String::format(_vm->getString("fence:IOERR").c_str(), tune->path.c_str()),
				MessageBox::kButtonOk);
			return;
		}
	}
	_current = 0;
	_page = 0;
	updateSock();
	for (int slot = 0; slot < kSlots; slot++)
		removeSlot(slot, true);
	removeMarker();
	if (isAnimAdded("cursor"))
		removeAnim("cursor");
}

// FUN_004672e0
bool FenceScene::readTune(Common::SeekableReadStream &s) {
	if (s.readByte() != 0xff || s.eos())
		return false;
	char name[kNameSize + 1];
	if (s.read(name, kNameSize) != kNameSize)
		return false;
	name[kNameSize] = 0;
	_tuneName = name;
	const int tempo = s.readByte();
	const int count = s.readByte();
	if (s.eos())
		return false;
	_count = 0;
	_tempo = kTempoDefault;
	for (int i = 0; i < count; i++) {
		const int32 pitch = s.readSint32LE();
		const int32 length = s.readSint32LE();
		if (s.eos() || s.err())
			return false;
		// Not checked by the original
		_notes[i].pitch = CLIP<int32>(pitch, 0, kPitches - 1);
		_notes[i].length = CLIP<int32>(length, 0, kLengths - 1);
	}
	_count = count;
	setTempo(tempo);
	return true;
}

// FUN_00468260 (the title is padded with zeros)
void FenceScene::writeTune(Common::WriteStream &out, const Common::String &title) const {
	out.writeByte(0xff);
	char name[kNameSize];
	memset(name, 0, sizeof(name));
	Common::strlcpy(name, title.c_str(), sizeof(name));
	out.write(name, sizeof(name));
	out.writeByte(_tempo);
	out.writeByte(_count & 0xff);
	for (int i = 0; i < _count; i++) {
		out.writeSint32LE(_notes[i].pitch);
		out.writeSint32LE(_notes[i].length);
	}
}

Common::SeekableReadStream *FenceScene::openTune(const TuneFile &tune) {
	switch (tune.source) {
	case kTuneShipped:
		return resources()->open(_name, tune.path);
	case kTuneLoose:
		return SearchMan.createReadStreamForMember(Common::Path(tune.path, '/'));
	case kTuneSaved:
		return g_system->getSavefileManager()->openForLoading(tune.path);
	default:
		return nullptr;
	}
}

// The tunes in file name order (the original's _findfirst on NTFS); a
// loose file or a save file replaces a shipped one of the same name
void FenceScene::listTunes(Common::Array<TuneFile> &list) {
	list.clear();
	auto addTune = [&list](const Common::String &file, const Common::String &path, TuneSource source) {
		for (auto &t : list) {
			if (t.file.equalsIgnoreCase(file)) {
				t.path = path;
				t.source = source;
				return;
			}
		}
		TuneFile t;
		t.file = file;
		t.path = path;
		t.source = source;
		list.push_back(t);
	};

	Common::StringArray shipped;
	resources()->listFiles(_name, "tunes/*.tun", shipped);
	for (const auto &f : shipped)
		addTune(f.substr(f.findLastOf('/') + 1), f, kTuneShipped);

	Common::ArchiveMemberList loose;
	SearchMan.listMatchingMembers(loose, Common::Path("data/fence/tunes/*.tun", '/'));
	for (const auto &m : loose)
		addTune(m->getFileName(), m->getPathInArchive().toString('/'), kTuneLoose);

	const Common::String prefix = ConfMan.getActiveDomainName() + "-fence-";
	Common::StringArray saves = g_system->getSavefileManager()->listSavefiles(prefix + "*.tun");
	for (const auto &f : saves)
		addTune(f.substr(prefix.size()), f, kTuneSaved);

	Common::sort(list.begin(), list.end(), [](const TuneFile &a, const TuneFile &b) {
		return a.file.compareToIgnoreCase(b.file) < 0;
	});

	// FUN_00467a20: the title inside each file (the original shows whatever
	// its buffer held for a file without the 0xff mark; the port shows the
	// file name)
	for (auto &t : list) {
		t.title = t.file;
		Common::SeekableReadStream *s = openTune(t);
		if (s && s->readByte() == 0xff) {
			char name[kNameSize + 1];
			if (s->read(name, kNameSize) == kNameSize) {
				name[kNameSize] = 0;
				t.title = name;
			}
		}
		delete s;
	}
}

// FUN_004087a0 / FUN_00408530: a row of the list dialog's text variant
// (flag 0x100): the text in the list font, Amerigo BT_14_ (set by
// FUN_00408990 at init), on the colour key, cut to the box
Graphics::ManagedSurface *FenceScene::textItem(const Common::String &text) {
	const int w = CLIP(_font14.stringWidth(text), 1, (int)kListW);
	const int h = CLIP(_font14.height(), 1, (int)kListH);
	Graphics::ManagedSurface *s = new Graphics::ManagedSurface(w, h, _vm->_screen->format);
	s->fillRect(Common::Rect(w, h), s->format.RGBToColor(0, 0xff, 0));
	_font14.drawString(*s, 0, 0, text);
	return s;
}

// FUN_00467a20: "fence:TUNES", OK only
void FenceScene::chooseTune() {
	Common::Array<TuneFile> tunes;
	listTunes(tunes);
	if (tunes.empty()) {
		_vm->messageBox("fence:NOTUNES_TITLE", "fence:NOTUNES", MessageBox::kButtonOk);
		return;
	}
	Common::Array<Graphics::ManagedSurface *> items;
	for (const auto &t : tunes)
		items.push_back(textItem(t.title));
	const int row = runListDialog(_vm, "fence:TUNES", items, Common::Array<int>(), 0);
	for (auto *item : items)
		delete item;
	if (row >= 0 && row < (int)tunes.size())
		changeTune(&tunes[row]);
}

// FUN_00468130 case 0x1e: the title (the tune's name to start with, up to
// 32 letters, digits and spaces) and the first free tune<n>.tun; the
// tune keeps its name
void FenceScene::saveTune() {
	Common::String title = _tuneName;
	if (runTextEntry(_vm, 0xf9, 0xf7, _vm->getString("fence:SAVETITLE"), title, kTitleLength, kTitleFlags) != 1 || title.empty())
		return;
	Common::Array<TuneFile> tunes;
	listTunes(tunes);
	Common::String file;
	for (int n = 0;; n++) {
		file = Common::String::format("tune%d.tun", n);
		bool used = false;
		for (const auto &t : tunes)
			used |= t.file.equalsIgnoreCase(file);
		if (!used)
			break;
	}
	const Common::String saveName = ConfMan.getActiveDomainName() + "-fence-" + file;
	Common::OutSaveFile *out = g_system->getSavefileManager()->openForSaving(saveName, false);
	if (!out) {
		warning("Fence: cannot save %s", saveName.c_str());
		return;
	}
	writeTune(*out, title);
	out->finalize();
	const bool ok = !out->err();
	delete out;
	debug(1, "Fence: tune '%s' saved as %s (%d notes)", title.c_str(), saveName.c_str(), _count);
	if (!ok)
		return;
	_vm->messageBox("fence:SAVETITLE", Common::String::format(_vm->getString("fence:SAVED").c_str(), saveName.c_str()),
		MessageBox::kButtonOk);
}

// ---- instruments ----------------------------------------------------------------------

// FUN_00466e50: the 18 notes of instrument.ini's resource<n> (piano by
// default); the add-on sets bring more instruments
void FenceScene::loadInstrument(int n) {
	_instrument = n;
	Common::String resource = "piano";
	Common::String value;
	if (_instrumentIni.getKey(Common::String::format("resource%d", n), "instrument", value))
		resource = Resources::iniValue(value);
	for (int i = 0; i < kPitches; i++) {
		const Common::String name = resource + "-" + kPitchNames[i];
		_noteSounds[i] = resources()->exists(_name, "sound/" + name + ".wav") ? name : "";
	}
	debug(1, "Fence: instrument %d '%s'", n, resource.c_str());
}

// FUN_00467990: "fence:INSTRUMENTS" with the language.ini names of name<n>
void FenceScene::chooseInstrument() {
	Common::Array<Graphics::ManagedSurface *> items;
	for (int i = 0; i < kMaxInstruments; i++) {
		Common::String key;
		if (!_instrumentIni.getKey(Common::String::format("name%d", i), "instrument", key))
			break;
		items.push_back(textItem(_vm->getString(Resources::iniValue(key))));
	}
	const int row = runListDialog(_vm, "fence:INSTRUMENTS", items, Common::Array<int>(), 0);
	for (auto *item : items)
		delete item;
	if (row >= 0)
		loadInstrument(row);
}

} // End of namespace Flaaklypa
