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
#ifndef FLAAKLYPA_BALLOONHUNT_H
#define FLAAKLYPA_BALLOONHUNT_H

#include "audio/mixer.h"
#include "common/array.h"
#include "common/formats/ini-file.h"
#include "graphics/managed_surface.h"

#include "flaaklypa/button.h"
#include "flaaklypa/font.h"
#include "flaaklypa/scene.h"

namespace Flaaklypa {

/**
 * "Solans ballongjakt" (the balloonhunt sub game of add-on set 2, started
 * from the "outtent" page, 640x480): Solan flies his gyrocopter over an
 * island, drawn by a small software 3D renderer of the original (a sky and
 * a ground plane textured from a 1024x1024 tile map, billboards for the
 * trees, houses and balloons). The arrow keys steer and climb, A and Z
 * change the speed, Page Up / Page Down zoom the radar. Three modes:
 * "Ballongjakt" (points: the balloons of the 20 tracks in order, against
 * the clock and the fuel gauge, a high score with medals), "Søndagstur"
 * (practice: balloons rise from the houses, no limits) and
 * "Baneutfordring" (time trial of one track, its time goes into the
 * track's high score table).
 *
 * Mirrors the handler at 0x429960 of the original executable (code
 * 0x429910..0x42f1af); see NOTES.md for the data formats.
 */
class BalloonhuntScene : public Scene {
public:
	BalloonhuntScene(FlaaklypaEngine *vm, const SceneDef *def);
	~BalloonhuntScene() override;

	bool load() override;
	void onInit(int arg) override;
	void onClose() override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onMouseUp(int hotspot, int x, int y) override;
	void onMouseMove(int hotspot, int x, int y) override;
	void onKey(const Common::KeyState &key) override;
	void onKeyUp(const Common::KeyState &key) override;
	bool handlesKey(const Common::KeyState &key) override;
	void onAnimFinished(Anim *anim) override;
	void onUpdate() override;
	/** Events 0x11b / 0x11c: a dialog opens or closes over a round. */
	void pauseAnims(bool pause) override;

	/** A 16 bit (RGB565) bitmap like the original's DirectDraw surfaces. */
	struct Image16 {
		int w, h;
		Common::Array<uint16> pix;
		uint16 key;           ///< the colour key, pixel (0, 0)
		Image16() : w(0), h(0), key(0) {}
	};

	/** A billboard bitmap (FUN_0042aba0, 0x4c bytes): name, size, pixels. */
	struct SpriteImage {
		Common::String name;
		Image16 image;
	};

	/** A billboard in the world (0x40 bytes, FUN_0042ad40). */
	struct Sprite3D {
		int alignX, alignY;   ///< +0 / +4: 0 left/top, 1 centred, 2 right/bottom
		float x, y, z;        ///< +0x08 world position (y up)
		float sx, sy, depth;  ///< +0x14 projected (FUN_0042d390)
		float w, h;           ///< +0x28 size in world units
		float sw, sh;         ///< +0x30 projected size
		const SpriteImage *image; ///< +0x38
		int fade;             ///< +0x3c fades out with the distance (the map's sprites)
	};

	struct Point2 {
		float x, y;
	};

private:
	enum {
		kTrackBalloons = 0x18,  ///< at most 24 balloons per track (FUN_0042b820)
		kBalloonTypes = 24,     ///< the balloon table at 0x4cbd68
		kFreeBalloons = 50,     ///< the practice balloons (0x608fe8)
		kCells = 8,             ///< the sprites are kept in 8 x 8 cells of 2000 units
		kSolanGroups = 17
	};

	/** A practice balloon (0x58 bytes at 0x608fe8). */
	struct FreeBalloon {
		Sprite3D sprite;
		int type;             ///< +0x40: the balloon type 0..3, 4 = free
		uint32 spawn;         ///< +0x44: when it appeared
		float x, y, z;        ///< +0x48
	};

	/** The horizontal track slider of the start screen (SCROLL module, id 0x11). */
	struct Slider {
		bool active;
		int x, y, w, h;       ///< the track (FUN_004090e0)
		int max;              ///< values 0..max
		int value;            ///< +0x20
		int thumbW;
		bool dragging;
		int lastX;
		bool thumb;           ///< the thumb's element exists
	};

	// ---- init and data (FUN_00429b10, FUN_0042a470)
	bool loadImage(const Common::String &name, Image16 &img);
	void loadWorld();
	bool loadTiles();
	void loadMap(const Common::String &name);
	void loadSprites(const Common::String &name);
	const SpriteImage *findImage(const Common::String &name);
	void freeWorld();
	void buildDither();
	void loadMix();

	// ---- start screen (FUN_0042ba40 ff.)
	void showStartScreen();
	void selectMode();
	void removeTrackSelector();
	void removeStartScreen();
	void startRound(int mode, int track);
	void loadLevel();
	void loadTrack(const Common::String &name);
	void endRound(int reason);
	void handleButton(int id);
	void exitPressed();
	void sliderSetPos(int pos);
	void sliderSetValue(int value);
	void sliderMouseDown(int hotspot, int x, int y);
	void sliderRelease();
	void sliderChanged(int value);

	// ---- the round
	void physics();
	void updateCamera();
	void ambience();
	void updateBalloons();
	int spawnBalloon(int type);
	void levelDone();
	void render();
	void renderSky();
	void renderGround();
	void renderSprites();
	void project(Sprite3D &s, int fade);
	void sortSprites(int lo, int hi);
	void drawSprite(float x, float y, float w, float h, const Image16 &img, int level);
	void drawLabels();
	void afterDraw();
	void drawSolan();
	void drawOverlay();
	void drawRadar(int px, int py, int size, float heading, float zoom);
	void drawCompass(int px, int py, int size, float heading);
	void fillTriangle(const Point2 &a, const Point2 &b, const Point2 &c, uint16 colour);
	void drawNeedle(float angle, float cx, float cy, float width, float length, uint16 colour);
	/** A bitmap with its colour key into the back buffer (FUN_00404a70 with flag 4). */
	void bbBlit(const Image16 &img, int x, int y, int w = -1, int h = -1);
	void bbPut(int x, int y, uint16 c) {
		if ((uint)x < 640 && (uint)y < 480)
			_bb[y * 640 + x] = c;
	}
	void updateCursor();
	void devWarp();

	// ---- labels (text areas of the original)
	void showLabel(const char *elem, int x, int y, int w, int h, int z, const Common::String &text);
	void drawLabel(const char *elem, const Common::String &text);
	void removeLabel(const char *elem);

	// ---- sounds
	void playClone(const char *name);
	void playMix();
	void stopMix();
	bool isMixPlaying() const;
	void setMixVolume(float factor);
	void setMixPitch(float factor);

	SceneButtons _buttons;
	BitmapFont _font;
	Common::INIFile _tracks;
	Common::Array<Common::String> _levels;   ///< tracks.ini sections
	Common::String _modeNames[3];            ///< 0x608e34

	// ---- world data
	int _tileCount;                          ///< DAT_00607344
	Common::Array<uint16> _tiles[4];         ///< 64x64, 32x32, 16x16 and 8x8 versions of each tile
	Common::Array<uint16> _map;              ///< 1024 x 1024 tile numbers (DAT_005c5e60)
	Common::Array<uint16> _sky;              ///< skynew.bmp, 1024 x 1024 (DAT_00608460)
	Common::Array<uint16> _radarMap;         ///< map3.bmp, 1024 x 1024 (DAT_00608fe4)
	Image16 _compassTex;                     ///< compskive.bmp, 64 x 64 (DAT_0060a2c0)
	Image16 _numbers;                        ///< numbers.bmp, 24 glyphs of 10 x 10 (DAT_0060a118)
	Image16 _autopilotImg, _cursorImg;
	Image16 _solan[kSolanGroups][3];         ///< 0x60a1f4: staylft, turnlft7..1, frwrd, turnrht1..7, stayrht
	Image16 _up[2], _stayUp[3], _down[2], _stayDown[3];
	Image16 _upLeft[2], _upRight[2], _downLeft[2], _downRight[2];
	Image16 _gyro[49];                       ///< "0000".."0606" (DAT_005c5d90)
	Common::Array<SpriteImage *> _images;    ///< the map's billboard bitmaps (DAT_00608e44)
	Common::Array<Sprite3D> _sprites;        ///< the map's billboards (DAT_005bd63c)
	Common::Array<int> _cells[kCells * kCells]; ///< sprites per cell (0x5c1540 / 0x60a458)
	Common::Array<int> _launchSites;         ///< the "sprite4" sprites (0x608d6c)
	SpriteImage _balloonImages[kBalloonTypes]; ///< "01".."24" (0x4cbdf0)
	Image16 _smallBalloons[kBalloonTypes];   ///< "small 01".. (0x4cbdf4)
	Common::Array<byte> _dither;             ///< 16 masks of 128 x 128 (0x5c5f2c)
	float _turnTable[100], _climbTable[100]; ///< 0x606780, 0x60a2c4

	// ---- buffers
	Common::Array<uint16> _bb;               ///< the 640x480 16 bit back buffer of a round
	Common::Array<uint32> _lut;              ///< RGB565 -> screen colour
	uint32 _key;                             ///< a colour the LUT never gives
	Graphics::ManagedSurface *_view;         ///< _bb in the screen format: the backdrop during a round
	Graphics::ManagedSurface *_savedBackdrop;
	Image16 _ifcTop, _ifcBottom;             ///< "interface top" / "interface bottom"
	Anim *_cursorAnim;
	Common::Array<Sprite3D *> _drawList;     ///< DAT_005c1ec0

	// ---- radar and compass circles (FUN_0042e2e0 / FUN_0042e7b0)
	int _radarSize, _compassSize;
	int _radarSpan[128], _radarLeft[128], _radarRight[128];
	int _compassSpan[128], _compassLeft[128], _compassRight[128];
	Point2 _marker[4];                       ///< 0x608d48: the aircraft on the radar

	// ---- state
	int _mode;                               ///< DAT_00608fe0: 0 start screen, 1 practice, 2 points, 3 time trial
	int _modeSel;                            ///< DAT_00605f48: 0 NORMAL, 1 PRACTICE, 2 SINGLE
	int _trackSel;                           ///< DAT_00608464
	int _level;                              ///< DAT_005c5f28 (the next section, 1 based after loading)
	Slider _slider;
	Sprite3D _track[kTrackBalloons];         ///< 0x5c56e8
	const Image16 *_trackSmall[kTrackBalloons]; ///< DAT_006069e0
	int _trackCount, _next;                  ///< DAT_00607340, DAT_00606a40
	FreeBalloon _free[kFreeBalloons];
	float _size;                             ///< DAT_005c165c
	int32 _timeLimit;                        ///< DAT_005c5e54 (ms)
	int32 _bonusTime;                        ///< DAT_005c0cf0
	uint32 _startTime, _lastHit, _endTime;   ///< DAT_00608fdc, DAT_00606910, DAT_005c168c
	uint32 _nextSpawn;                       ///< DAT_0060843c
	int _score, _scoreShown;                 ///< DAT_00608c98, DAT_0060a1c8
	float _gas;                              ///< DAT_005c1688
	float _x, _y, _z;                        ///< DAT_004ccaec, DAT_004ccae8 (altitude), DAT_004ccaf0
	float _heading, _speed, _zoom;           ///< DAT_00648c58, DAT_004ccaf8, DAT_004ccaf4
	float _left, _right, _up2, _down2;       ///< the control ramps 0..1
	float _roll, _pitch;                     ///< DAT_00608444, DAT_00608438
	bool _autopilot;                         ///< DAT_005c0cec
	float _camX, _camY, _camZ;               ///< DAT_006069d0, DAT_006069d4, DAT_006069dc
	float _sinH, _cosH, _sinR, _cosR;        ///< DAT_005c1648/50, DAT_005c164c/54
	float _proj, _invCos;                    ///< DAT_005c56e0, DAT_005c1660
	float _dt;                               ///< DAT_005bd61c
	uint32 _lastFrame;                       ///< DAT_005c1644
	int64 _lastPhysics;                      ///< DAT_004cbd60, -1 before the first frame
	// the cursor drawn by the game (FUN_0042dcf0)
	int _mouseX, _mouseY;
	bool _cursorOn;
	uint32 _cursorUntil;
	int _pressed;                            ///< DAT_00606918
	// keys (GetAsyncKeyState)
	bool _keyLeft, _keyRight, _keyUp, _keyDown, _keyA, _keyZ, _keyPgUp, _keyPgDn;
	// sounds
	float _master;                           ///< sound.ini "sound=" (FUN_0040b3f0)
	Common::Array<byte> _mixData;
	Audio::SoundHandle _mixHandle;
	uint32 _mixRate;
	float _mixPitch;
	int _mixVolume;
	bool _mixPaused;                         ///< DAT_00606778
	int _warp;                               ///< development aid "balloonhunt_warp"
};

} // End of namespace Flaaklypa

#endif
