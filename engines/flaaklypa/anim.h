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
#ifndef FLAAKLYPA_ANIM_H
#define FLAAKLYPA_ANIM_H

#include "common/rect.h"
#include "common/str.h"
#include "graphics/managed_surface.h"

#include "flaaklypa/scenedata.h"

namespace Flaaklypa {

class Scene;
class SmackerClip;

/**
 * A scene element: a Smacker clip or a bitmap drawn at a position, mirroring
 * the original's animation structures (SCENE_AddAnim / SCENE_PlayAnim).
 *
 * Audio only clips (sound effects, narration, music) are animations too;
 * they are simply not drawn.
 */
class Anim {
public:
	enum { kDefaultPos = -1 };

	Anim(Scene *scene, const AnimDef *def);
	~Anim();

	const AnimDef *def() const { return _def; }
	const char *name() const { return _def->name; }

	bool isAdded() const { return _added; }
	bool isPlaying() const { return _playing; }
	bool isSmacker() const { return _def->smacker != 0; }
	/**
	 * Whether the element shows: the visible flag (+0x44) only matters for
	 * clips (audio only ones have no picture, FUN_00412840); bitmaps always
	 * show.
	 */
	bool isVisible() const { return !_def->smacker || _def->visible; }

	/** SCENE_AddAnim: loads the resources and puts the element on screen (bitmaps) or makes it ready (clips). */
	void add(int x = kDefaultPos, int y = kDefaultPos, int z = 0);
	/** SCENE_RemoveAnim: takes the element off screen and frees its resources. */
	void remove();
	/** SCENE_PlayAnim: adds the clip (default position and z order) if needed and starts it. */
	void play();
	void stop();
	/** Pauses the clip and its sound (scene freeze); a paused clip does not advance. */
	void pause(bool pause);

	/**
	 * Turns the element into a procedurally drawn bitmap of the given size:
	 * the scene draws into surface() and the element keeps it across
	 * add() / remove(). Pixels of the key colour are transparent.
	 */
	void createSurface(int w, int h, uint32 keyColor);
	Graphics::ManagedSurface *surface() { return _frame; }

	/** Shows frame n of a clip without playing it (SmackGoto of the original). */
	void showFrame(int n);
	int frameCount() const;
	/**
	 * Sets the volume of a clip's sound effects, 0..255, a factor to the
	 * engine's sound volume (FlaaklypaEngine::channelVolume()).
	 */
	void setVolume(int volume);
	/** Gives the clip's audio tracks the engine's sound and speech volumes. */
	void applyVolume();
	/** Forces the playback frame rate of a playing clip (SmackFrameRate of the original). */
	void setFrameRate(int fps);

	int x() const { return _x; }
	int y() const { return _y; }
	int z() const { return _z; }
	void setZ(int z);
	void setPos(int x, int y) { _x = x; _y = y; }

	/** Hotspot index reported for the element's pixels (0: none). Characters override the static one. */
	int hotspot() const { return _hotspot; }
	void setHotspot(int h) { _hotspot = h; }
	/**
	 * Restricts the hit test to the non-zero pixels of an 8 bit mask of the
	 * element's size (hotspot mode 4 of the original; the "<name>_hs.bmp"
	 * bitmaps). The mask stays owned by the caller.
	 */
	void setHitMask(const Graphics::Surface *mask) { _hitMask = mask; }
	/**
	 * Makes the element clickable within r pixels of its centre instead of
	 * on its opaque pixels (hotspot mode 1 of the original, FUN_00413670;
	 * the pee page's flying butterfly). 0 restores the pixel test.
	 */
	void setHitRadius(int r) { _hitRadius = r; }
	/** Character bit mask; animations sharing a bit cannot play at the same time. */
	uint32 group() const { return _group; }
	void setGroup(uint32 g) { _group = g; }

	/** Removes the element automatically once it has finished playing. */
	void setRemoveWhenDone(bool b) { _removeWhenDone = b; }
	bool removeWhenDone() const { return _removeWhenDone; }

	/**
	 * Advances the clip. Returns true once when the clip has just finished.
	 */
	bool update();

	void draw(Graphics::ManagedSurface &dst);

	/** Returns the hotspot index if (x, y) hits the element's opaque pixels, 0 otherwise, -1 when the element has no hotspot. */
	int hitTest(int x, int y) const;

	Common::Rect rect() const;

private:
	void load();
	void unload();
	void convertFrame(const Graphics::Surface *frame);

	Scene *_scene;
	const AnimDef *_def;

	SmackerClip *_video;
	int _volume;                          ///< setVolume(), 0..255
	Graphics::ManagedSurface *_frame;     ///< current frame (clip) or the bitmap, in screen format
	bool _hasKey;
	uint32 _keyColor;

	int _x, _y, _z;
	int _hotspot;
	const Graphics::Surface *_hitMask;
	int _hitRadius;
	uint32 _group;
	bool _added, _playing, _removeWhenDone;
	bool _ownSurface;        ///< _frame was made by createSurface() and survives unload()
	uint32 _lastFrameTime;   ///< when the last frame was decoded (to hold it for its duration)
	bool _lastFrameShown;
};

} // End of namespace Flaaklypa

#endif
