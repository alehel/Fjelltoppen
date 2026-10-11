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
#ifndef FLAAKLYPA_SCENE_H
#define FLAAKLYPA_SCENE_H

#include "common/array.h"
#include "common/events.h"
#include "common/keyboard.h"
#include "common/list.h"
#include "common/str.h"
#include "graphics/managed_surface.h"
#include "graphics/surface.h"

#include "flaaklypa/anim.h"
#include "flaaklypa/scenedata.h"

namespace Flaaklypa {

class FlaaklypaEngine;
class Resources;
class Subtitles;

/** Profile key of the fact pages found in a scene (a bit per fact.ini section). */
enum {
	kProfileKeyFacts = 100
};

/** A NULL terminated list of animation names (an "anim list" of the original). */
typedef const char *const *AnimList;

/**
 * Base class of all screens: story pages, the menu, sub games.
 *
 * Mirrors the SCENE / EVENT / CHAR modules of the original: a backdrop with
 * an 8 bit hotspot mask, a z-sorted list of animations, a cursor table
 * mapping hotspot indices to cursors, animation sequences, characters with
 * idle/bored/reaction animation lists, and timers. Subclasses implement the
 * per scene logic in the on*() callbacks.
 */
class Scene {
public:
	Scene(FlaaklypaEngine *vm, const SceneDef *def);
	virtual ~Scene();

	const SceneDef *def() const { return _def; }
	const Common::String &name() const { return _name; }
	Resources *resources() const;

	/** Loads backdrop and hotspot mask. */
	virtual bool load();

	// Callbacks, in the order the original dispatches them
	virtual void onInit(int arg) {}
	virtual void onClose() {}
	virtual void onMouseDown(int hotspot, int x, int y) {}
	virtual void onRightClick(int x, int y) {}
	virtual void onMouseUp(int hotspot, int x, int y) {}
	virtual void onMouseMove(int hotspot, int x, int y) {}
	virtual void onKey(const Common::KeyState &key) {}
	virtual void onKeyUp(const Common::KeyState &key) {}
	virtual void onAnimStarted(Anim *anim) {}
	virtual void onAnimFinished(Anim *anim) {}
	virtual void onTimer(int id, int data) {}
	virtual void onSequenceDone() {}
	/** Called once per frame after the animations have been advanced. */
	virtual void onUpdate() {}
	/**
	 * Returns true when the scene consumes the key (the original's handler
	 * returning 1): the default space bar handling and the global keys
	 * (FlaaklypaEngine::globalKey()) are skipped.
	 */
	virtual bool handlesKey(const Common::KeyState &key) { return false; }

	void handleEvent(const Common::Event &event);
	/** Pauses or resumes the clips of all added elements (scene freeze). */
	virtual void pauseAnims(bool pause);
	void update();
	void draw(Graphics::ManagedSurface &dst);

	// ---- animations
	Anim *anim(const char *name);
	void addAnim(const char *name, int x = Anim::kDefaultPos, int y = Anim::kDefaultPos, int z = 0);
	void removeAnim(const char *name);
	void playAnim(const char *name);
	bool isAnimAdded(const char *name);
	bool isAnimPlaying(const char *name);
	/**
	 * Defines an element that has no entry in the scene tables (buttons and
	 * other bitmaps the original loads through other modules).
	 */
	Anim *defineAnim(const char *name, bool smacker, bool transparent, int hotspot, int x, int y, int z = 0, bool visible = true, bool loop = false);
	/** Defines a procedurally drawn element, see Anim::createSurface(). */
	Anim *defineSurfaceAnim(const char *name, int w, int h, uint32 keyColor, int hotspot = 0);

	// ---- sequences (the "anim list" player of the CHAR module)
	/** Plays the animation as a one element sequence (cursor hidden while it runs). */
	void playSingle(const char *name);
	void playSequence(AnimList list);
	bool isSequencePlaying() const { return _seqList != nullptr; }
	/** Aborts the running sequence and resets the characters to idle (space bar). */
	void stopSequence();

	// ---- characters (CHAR module)
	/** Creates a character (FUN_0040a2b0); does nothing if it already exists. */
	void addCharacter(int id, int hotspot);
	void setCharacterZ(int id, int z);
	void setCharacterAnims(int id, AnimList idle, AnimList bored, AnimList reaction);
	/** Replaces one of a character's lists (FUN_0040a360: 1 idle, 2 bored, 3 reaction, 4 own). */
	void setCharacterList(int id, int which, AnimList list);
	/**
	 * Switches a character to a list (FUN_0040a790): 1 idle, 2 bored, 3 reaction,
	 * 4 its own list, 5 the running sequence. The next clip of that list starts
	 * at once when the character's group is free, else after the current clip.
	 */
	void setCharacterState(int id, int state);
	/**
	 * Gives the character an own list and starts it (FUN_0040a4e0). The list
	 * cycles like the idle list until setCharacterState() switches away.
	 */
	void playCharacterList(int id, AnimList list);
	/**
	 * Removes the character's current clip and deletes the character
	 * (FUN_0040a310). Not to be called from onAnimStarted(), which can run
	 * while the scene walks its characters.
	 */
	void stopCharacter(int id);
	/** True while the character's current clip is on screen (FUN_0040a240). */
	bool isCharacterActive(int id);
	/** Puts every character into its idle loop. */
	void resetCharacters();

	// ---- cursor
	void setCursorTable(const CursorEntry *table);
	void showWaitCursor();
	void hideCursor();
	void showCursor();
	/** Re-applies the cursor of the current mode and hotspot (after a dialog). */
	void refreshCursor() { _lastHotspot = -1; updateCursor(); }

	// ---- timers
	void setTimer(int id, uint32 delayMs, int data = 0);
	void killTimer(int id);

	// ---- the hidden items of the story pages
	/** The page's hidden car part was found (the page's profile value 0). */
	bool carPartFound() const;
	/**
	 * The player clicked the hidden car part: the page remembers it in value
	 * 0, the award dialog hands it out (FUN_0041c150) and "carpart" goes.
	 */
	void takeCarPart();
	/** Hidden 3D scene n (the 3D glasses) was found (FUN_0041b8c0). */
	bool found3DScene(int n) const;
	/** The player clicked the 3D glasses: award dialog (FUN_0041c1b0) and "3dglasses" goes. */
	void take3DGlasses(int n);

	// ---- fact pages (FACT module)
	/**
	 * Loads the scene's fact.ini (FUN_0041dbf0, story page init): a click on
	 * a section's hotspot shows its text in a message box.
	 */
	void initFacts();

	// ---- misc
	int hotspotAt(int x, int y) const;
	/**
	 * FUN_004161d0 / FUN_00415680 (goodbye, birthday): a random entry of the
	 * list that is not playing (up to 100 draws from the first randomRange
	 * entries), else the first of the count entries that is not playing;
	 * -1 when all are busy.
	 */
	int pickIdleAnim(AnimList list, int count, int randomRange);
	/**
	 * How often the player has entered this scene, this visit included
	 * (FUN_00411370 on the current scene, FUN_00411ed0); 0 without a
	 * profile. Pages play some narration only while it is below 2.
	 */
	int32 visits() const;
	void playMusic(const char *name);

	// used by Anim
	void animAdded(Anim *anim);
	void animRemoved(Anim *anim);
	void animStarted(Anim *anim);

protected:
	FlaaklypaEngine *_vm;
	const SceneDef *_def;
	Common::String _name;

	Graphics::ManagedSurface *_backdrop;
	Graphics::Surface *_mask;

private:
	enum CursorMode { kCursorHidden, kCursorWait, kCursorNormal };

	enum {
		kListIdle = 1,
		kListBored = 2,
		kListReaction = 3,
		kListOwn = 4,
		kListSequence = 5
	};

	/** A cyclic list of animations with a play position. */
	struct AnimQueue {
		AnimList list;
		int count;
		int pos;
		AnimQueue() : list(nullptr), count(0), pos(0) {}
		void set(AnimList l);
		const char *current() const { return count ? list[pos] : nullptr; }
		const char *previous() const;
		bool advance();
	};

	struct Character {
		int id;                 ///< bit in the animation group masks
		int hotspot;            ///< hotspot index of its pixels
		int z;
		AnimQueue lists[5];     ///< unused, idle, bored, reaction, own
		int state;              ///< 0 inactive, else the list to play next from (kList*)
		int mode;               ///< list the current clip came from
		Anim *current;
		uint32 boredTime;
		Character() : id(0), hotspot(0), z(0), state(0), mode(0), current(nullptr), boredTime(0) {}
	};

	struct Timer {
		int id;
		int data;
		uint32 fireTime;
	};

	Anim *findAnim(const char *name);
	void sortAnims();
	void updateCursor();
	void setCursorMode(CursorMode mode);
	void characterClicked(int hotspot);
	void characterAnimFinished(Character &c, Anim *anim);
	Character *findCharacter(int id);
	void characterPlayNext(Character &c, int list);
	void sequenceAnimFinished(Anim *anim);
	void factClicked(int hotspot);
	void sequenceClear();
	bool groupBusy(uint32 group) const;
	void setGroupState(uint32 group, int state);
	void takeAnim(Anim *anim, int mode);
	void scheduleBored(Character &c);
	void dispatchAnimFinished(Anim *anim);

	Common::Array<Anim *> _anims;        ///< all animation objects of the scene (owned)
	Common::Array<AnimDef *> _dynDefs;   ///< definitions created by defineAnim()
	Common::List<Common::String> _dynNames;
	const char *_singleList[2];
	Common::Array<Anim *> _active;       ///< added animations, sorted by z
	Common::Array<Character> _characters;
	Common::List<Timer> _timers;
	Common::Array<Anim *> _finished;     ///< animations that finished this frame

	const CursorEntry *_cursorTable;
	CursorMode _cursorMode;
	int _lastHotspot;

	AnimList _seqList;
	AnimQueue _seq;

	Subtitles *_subtitles;

	struct Fact {
		Common::String title;
		int hotspot;
		Common::String text;
	};
	Common::Array<Fact> _facts;
	bool _factsActive;
	uint32 _factsFound;  ///< bit per fact.ini section, profile key 100
};

} // End of namespace Flaaklypa

#endif
