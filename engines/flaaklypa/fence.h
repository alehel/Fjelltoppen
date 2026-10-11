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
#ifndef FLAAKLYPA_FENCE_H
#define FLAAKLYPA_FENCE_H

#include "common/array.h"
#include "common/formats/ini-file.h"
#include "common/hash-str.h"
#include "common/hashmap.h"
#include "common/rect.h"
#include "common/str.h"
#include "graphics/surface.h"

#include "flaaklypa/button.h"
#include "flaaklypa/font.h"
#include "flaaklypa/scene.h"

namespace Common {
class SeekableReadStream;
class WriteStream;
}

namespace Flaaklypa {

class SoundPlayer;

/**
 * "Musikkgjerdet" (activity, started from pee; handler FUN_00466250): the
 * 18 boards of the fence are the white keys of a xylophone (A to D two
 * octaves up), the five power lines a staff. The player plays freely,
 * follows a tune note by note, composes one (each board pressed adds a
 * crotchet after the current note; notes are dragged up and down the
 * staff, thrown away below the clothes line and change their length with
 * the right button), plays it back at 60..240 beats per minute with one of
 * the instruments of instrument.ini (WAV notes mixed at will), and loads
 * and saves tunes (.tun files). Birds fly away with the notes of the old
 * tune and land as the notes of the new one.
 */
class FenceScene : public Scene {
public:
	FenceScene(FlaaklypaEngine *vm, const SceneDef *def);
	~FenceScene() override;

	void onInit(int arg) override;
	void onClose() override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onMouseUp(int hotspot, int x, int y) override;
	void onMouseMove(int hotspot, int x, int y) override;
	void onRightClick(int x, int y) override;
	void onKey(const Common::KeyState &key) override;
	void onKeyUp(const Common::KeyState &key) override;
	void onTimer(int id, int data) override;
	void onUpdate() override;

private:
	enum {
		kPitches = 18,       ///< A1..D3, one per board
		kLengths = 8,        ///< quaver .. dotted semibreve
		kSlots = 16,         ///< notes per page of the staff
		kMaxNotes = 0x400,
		kMaxBirds = 16,
		kMaxPoints = 21,     ///< room for the flight path (+0xb4..+0x15b of a bird)
		kNameSize = 0x400    ///< the tune's name in a .tun file
	};

	/** DAT_006b6228 */
	enum Mode {
		kModeFlyIn = 0,      ///< birds land with the notes of the new tune
		kModeFlyAway = 1,    ///< birds take the notes of the old tune away
		kModeFree = 2,       ///< "Fri": play freely or follow the tune
		kModePlay = 3,       ///< "Spill": the tune plays
		kModeRecord = 4      ///< "Komp.": compose / edit
	};

	enum DragKind {
		kDragNone,
		kDragNote,           ///< FUN_0040c1f0(note, 0)
		kDragSock            ///< FUN_0040c1f0(thumb, 0x21), the scroll bar
	};

	/** A note of a tune, as stored in a .tun file (two little endian int32). */
	struct Note {
		int32 pitch;         ///< 0..17 (A1 .. D3)
		int32 length;        ///< 0..7, see kLengthNames
	};

	/** A bird carrying a note (0x16c bytes at 0x6b04f8: an animation and its flight). */
	struct Bird {
		Anim *anim;          ///< the element showing its current clip or bitmap
		int pitch, length;   ///< +0xa8, +0xac
		int n;               ///< +0xb0: the path has n + 1 points
		Common::Point points[kMaxPoints];  ///< +0xb4
		float t;             ///< +0x15c: position on the path, 0..1
		float speed;         ///< +0x160: per second
		float delay;         ///< +0x164: seconds before it starts
		int state;           ///< +0x168, see birdUpdate()
	};

	/** Where a tune file comes from (see listTunes()). */
	enum TuneSource {
		kTuneShipped,        ///< tunes/<file> in the scene's containers
		kTuneLoose,          ///< data/fence/tunes/<file> in the game directory
		kTuneSaved           ///< the save file <target>-fence-<file>
	};

	struct TuneFile {
		Common::String file;   ///< "tune3.tun"
		Common::String path;   ///< the container path, game file or save file name
		TuneSource source;
		Common::String title;
	};

	// modes (FUN_00467480 ff.)
	void leaveMode();
	void enterFree();
	void enterPlay();
	void enterRecord();
	void showModeLight(int hotspot);
	void handleClick(int hotspot);
	void handleButton(int id);
	void boardPressed(int pitch);
	void playNote(int pitch);
	bool playStep();
	bool advance();
	void setTempo(int tempo);
	void toggleLetters();

	// the staff (FUN_004665a0 ff.)
	void showPage();
	void showNote(int slot, int index, int z);
	void showExtras(int slot, int pitch, int x);
	void removeSlot(int slot, bool note);
	void showCursorMark();
	void scrollToCurrent();
	void removeMarker();
	void centreAnim(Anim *a, int x, int y);
	Common::String slotName(int slot) const;
	/** Shows bitmap/<file> in an element of the scene's own (SceneButtons::showBitmap). */
	Anim *showElement(const Common::String &elem, const Common::String &file, int hotspot, int x, int y, int z);
	void removeElement(const Common::String &elem);
	/** An element made by showElement(), nullptr before. */
	Anim *findAnimOrNull(const char *name);

	// dragging notes (FUN_00468070, FUN_00466870, FUN_004663c0)
	void startDrag(DragKind kind);
	void dragMove(int x, int y);
	void drop();

	// the sock on the clothes line (SCROLL module, scroll bar 0x21)
	void updateSock();
	bool sockSetValue(int value);
	bool sockSetPos(int pos);
	void sockMouseDown(int hotspot, int x, int y);
	void scrolled();

	// birds (FUN_00467060 ff.)
	void flyAway();
	void flyIn();
	void birdTakeOff(int x, int y, const Note &note);
	void birdLand(int x, int y, const Note &note);
	void birdClip(int index, const Common::String &clip, bool smacker, bool loop);
	bool birdUpdate(int index, float dt);
	void updateBirds(float dt);
	float randomUnit();

	// tunes and instruments
	void changeTune(const TuneFile *tune);
	void listTunes(Common::Array<TuneFile> &list);
	Common::SeekableReadStream *openTune(const TuneFile &tune);
	bool readTune(Common::SeekableReadStream &s);
	void writeTune(Common::WriteStream &out, const Common::String &title) const;
	void chooseTune();
	void saveTune();
	void loadInstrument(int n);
	void chooseInstrument();
	Graphics::ManagedSurface *textItem(const Common::String &text);

	static Note _notes[kMaxNotes];  ///< DAT_006b6230, kept across visits like the original's globals
	static int _count;              ///< DAT_006b8230

	SceneButtons _buttons;
	BitmapFont _font10;             ///< DAT_006b8640: labels and button labels
	BitmapFont _font14;             ///< DAT_006b1c68: the list dialogs' rows
	SoundPlayer *_sounds;
	Common::INIFile _instrumentIni; ///< DAT_006aa430
	Common::String _noteSounds[kPitches];  ///< DAT_006b2c98: sound/<instrument>-<pitch>, "" when missing
	int _instrument;                ///< DAT_006b0444
	int _birdsChannel;              ///< DAT_006b3760: sound/birds while birds fly
	Graphics::Surface _rectMask;    ///< hit mask for the notes' rectangles (hotspot mode 2)
	Common::HashMap<Common::String, bool> _defined;  ///< the elements showElement() made
	Anim *_tempoLabel;              ///< DAT_006b1bc0

	Common::String _tuneName;       ///< DAT_006b8238
	int _tempo;                     ///< DAT_006b8234, beats per minute
	Mode _mode;
	Mode _prevMode;                 ///< DAT_006b3814: the mode the playback returns to
	int _current;                   ///< DAT_006b1bb8: the current note
	int _page;                      ///< DAT_006b3820: the first note shown
	int _dragSlot;                  ///< DAT_006b8638: the note dragged, -1 for none
	bool _letters;                  ///< DAT_006b381c: note names shown
	Common::Point _slotPos[kSlots]; ///< +0x6c/+0x70 of the note elements
	uint32 _lastTick;               ///< DAT_006b3810

	DragKind _drag;
	Common::Point _dragMouse;

	bool _sockCreated;
	int _sockMax, _sockValue;
	int _sockW, _sockH;             ///< the thumb's size

	Bird _birds[kMaxBirds];         ///< 0x6b04f8
	int _birdCount;                 ///< DAT_006b043c

	bool _leftHeld, _rightHeld;     ///< GetAsyncKeyState(VK_LEFT / VK_RIGHT)
};

} // End of namespace Flaaklypa

#endif
