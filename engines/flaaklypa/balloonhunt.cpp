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
#include "audio/audiostream.h"
#include "audio/decoders/wave.h"
#include "common/config-manager.h"
#include "common/debug.h"
#include "common/memstream.h"
#include "common/system.h"
#include "common/textconsole.h"

#include "flaaklypa/balloonhunt.h"
#include "flaaklypa/dialog.h"
#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/hiscore.h"
#include "flaaklypa/resources.h"

namespace Flaaklypa {

// ---- constants of the original ---------------------------------------------

static const float kPi = 3.14159274f;          // 0x49d620
static const float kHalfPi = 1.57079637f;      // 0x49da38
static const float kQuarterPi = 0.785398185f;  // 0x49da88
static const double kTwoPi = 6.2831854820251465; // 0x49da28 (a double)
static const double kQuarterPiD = 0.7853981852531433; // 0x49dab8 (a double)
static const float kFov = 1.57079637f;         // 0x4ccafc
static const float kCentre = 6700.0f;          // 0x49da34: the middle of the play area
static const float kArea = 10000.0f;           // 0x49c8f8: beyond it the autopilot steers back
static const float kTurnRamp = 1.5f;           // 0x49d9e8: per second, the left / right ramps
static const float kClimbRamp = 3.0f;          // 0x49da20: per second, the up / down ramps
static const float kSpeedStep = 80.0f;         // 0x49da1c: A / Z, per second
static const float kZoomStep = 70.0f;          // 0x49da14: Page Up / Page Down, per second
static const float kMinSpeed = 50.0f, kMaxSpeed = 600.0f;   // 0x49d9f4, 0x49da18
static const float kMinAlt = 90.0f, kMaxAlt = 1200.0f;      // 0x49d9e0, 0x49d9dc
static const float kMinZoom = 35.0f, kMaxZoom = 250.0f;     // 0x49da10, 0x49da0c
static const float kCamBack = 3.5f;            // 0x49da40
static const float kCamUp = 0.7f;              // 0x49da3c
static const float kSkyHeight = -4250.0f;      // 0x49da54
static const float kFarClamp = 35000.0f;       // 0x49da50
static const float kTexStep = 102.4f;          // 0x49d9a0: 65536 / 640
static const float kFixed = 65536.0f;          // 0x49da4c
static const float kFar = 2000.0f;             // 0x60a1ec: sprites beyond are not drawn
static const float kFadeStart = 1300.0f;       // 0x6073f8: sprites fade out from here
static const float kCellScale = 0.0005f;       // 0x49d9a8: 2000 units per cell
static const float kMapScale = 4.0f;           // 0x49d9ac: the .txt coordinates
static const float kGasPerBalloon = 50.0f;     // 0x49d9f4
static const float kGasEmpty = 0.01f;          // 0x49da90

enum {
	kScreenW = 640,
	kScreenH = 480,
	kViewTop = 0x1e,          ///< the 3D view: rows 30..379 (FUN_0042d650)
	kViewBottom = 0x17c,
	kHorizon = 205,           ///< the first row of the ground (FUN_0042cd30)
	kMapSize = 1024,
	kSeaTile = 0xb4,

	kButtonStart = 99,
	kButtonHelp = 0x66,
	kButtonExit = 0x67,
	kButtonModeLeft = 0xc9,
	kButtonModeRight = 0xca,
	kSliderId = 0x11,         ///< the scroll bar, also the hotspot of its thumb
	kSliderLeft = 2000,       ///< its arrow buttons (DAT_004a6354)
	kSliderRight = 2001,

	kModeMenu = 0,
	kModePractice = 1,
	kModeNormal = 2,
	kModeSingle = 3,

	kSelNormal = 0,           ///< the start screen's choices (0x608e34)
	kSelPractice = 1,
	kSelSingle = 2
};

// Medal thresholds (0x49d7e8).
static const int kMedals[4] = { 2500, 5000, 10000, 20000 };

// The ambience (FUN_0042b190): the shore line (15 points, a closed
// polygon, 0x49d7f8), the woods (35 points, 0x49d870) and the sheep (3
// points, 0x49d988), as (x, z).
static const float kShore[15][2] = {
	{ 1312, 288 }, { 2256, 3232 }, { 80, 3712 }, { 2080, 5312 }, { 2160, 7328 },
	{ 3200, 8624 }, { 2976, 12304 }, { 5808, 13200 }, { 6928, 12608 }, { 11776, 7184 },
	{ 11824, 5728 }, { 9952, 4080 }, { 7808, 48 }, { 4736, 1728 }, { 3600, 528 }
};
static const float kForest[35][2] = {
	{ 2176, 1472 }, { 2176, 1856 }, { 2016, 2144 }, { 2304, 2464 }, { 2784, 1888 },
	{ 3232, 1792 }, { 7040, 1792 }, { 6688, 2368 }, { 6368, 2816 }, { 5824, 3744 },
	{ 5728, 4288 }, { 4960, 3680 }, { 4896, 4160 }, { 4832, 4736 }, { 3584, 5248 },
	{ 3456, 5856 }, { 2528, 6880 }, { 5312, 10656 }, { 5408, 11136 }, { 5920, 10784 },
	{ 6176, 11424 }, { 6656, 10560 }, { 7072, 11104 }, { 8800, 10528 }, { 9440, 9664 },
	{ 10336, 5984 }, { 7424, 7072 }, { 9216, 7648 }, { 3904, 10464 }, { 3264, 10624 },
	{ 3904, 11488 }, { 8448, 2240 }, { 2560, 6880 }, { 3520, 5440 }, { 6720, 8896 }
};
static const float kSheep[3][2] = {
	{ 7040, 9504 }, { 7040, 9952 }, { 6848, 9664 }
};

// The looping ambience clips (FUN_0042ad60).
static const char *const kAmbience[] = { "wind1", "seashore", "off limits1", "forest1", "sheep" };

// The sound effects, played as copies of the blank " " element (FUN_0042c420).
static const char *const kEffects[] = { "baloon-d", "wrong ballon 1", "gameover", "win" };

// Labels of a round (text areas at z 2, FUN_0042ad60): element, rectangle
// (0x4cd268 ff.), language.ini key of a fixed text.
static const struct {
	const char *elem;
	int x0, y0, x1, y1;
	const char *text;
} kRoundLabels[] = {
	{ "bh_score", 279, 11, 362, 26, nullptr },              // DAT_005c0cf8
	{ "bh_time", 162, 11, 228, 23, nullptr },               // DAT_00608ca0
	{ "bh_level", 411, 11, 477, 23, nullptr },              // DAT_005c5ce8
	{ "bh_gas", 21, 457, 77, 470, "balloonhunt:GAS" },      // DAT_00607350
	{ "bh_speed", 104, 457, 160, 470, "balloonhunt:SPEED" },// DAT_006071c8
	{ "bh_comp", 186, 457, 242, 470, "balloonhunt:COMP" },  // DAT_00608468
	{ "bh_gyro", 398, 458, 454, 471, "balloonhunt:GYRO" },  // DAT_005c5638
	{ "bh_alt", 480, 457, 536, 470, "balloonhunt:ALT" },    // DAT_00607288
	{ "bh_next", 562, 457, 618, 470, "balloonhunt:NEXT" }   // DAT_006066d0
};

// During a round the game draws its own cursor (cursor.bmp) and hides the
// system one (FUN_0040ef90(0)). The scene framework ignores clicks while its
// cursor is hidden, so an empty cursor goes on every hotspot instead.
static const CursorEntry kNoCursor[] = {
	{ 0, "" }, { 10, "" }, { kButtonHelp, "" }, { kButtonExit, "" }, { -1, nullptr }
};

// __ftol: the FPU truncates (chop mode); the low 32 bits of the 64 bit result.
static inline int32 ftol(double v) {
	return (int32)(uint32)(uint64)(int64)v;
}

// FUN_0042b5b0
static inline float clamp01(float v) {
	if (v < 0.0f)
		return 0.0f;
	if (1.0f < v)
		return 1.0f;
	return v;
}

// FUN_0042b4a0: the squared distance of two points.
static inline float dist2(float px, float pz, const float *q) {
	double dx = (double)px - q[0], dz = (double)pz - q[1];
	return (float)(dz * dz + dx * dx);
}

// FUN_0042b4d0: the distance of q to the line p1-p2 between its ends; before
// p1 and past p2 the distance to p1 (sic).
static float segmentDistance(const float *p1, const float *p2, float qx, float qz) {
	float ax = qx - p1[0], az = qz - p1[1];
	double d2 = (double)az * az + (double)ax * ax;
	if (d2 <= 0.0)
		return 0.0f;
	double d = sqrt(d2);
	float ex = p2[0] - p1[0], ez = p2[1] - p1[1];
	float inv = (float)(1.0 / sqrt((double)ez * ez + (double)ex * ex));
	float nx = inv * ex, nz = inv * ez;
	double t = (double)(inv * az) * nz + (double)(inv * ax) * nx;
	if (t <= 0.0)
		return (float)d;
	if (t < 1.0)
		return (float)fabs((double)-nx * az + (double)nz * ax);
	return (float)d;
}

// FUN_00469910: (value + delta) wrapped to 0..count-1.
static int wrapIndex(int value, int count, int delta) {
	value += delta;
	if (count < 2)
		return 0;
	if (value < 0)
		return count - (-value % count);
	return value % count;
}

static Graphics::PixelFormat format565() {
	return Graphics::PixelFormat(2, 5, 6, 5, 0, 11, 5, 0, 0);
}

// FUN_0042ee50 / FUN_0042eec0 for RGB565 and FUN_0042a9a0: a picture of
// half the size, each pixel the mean of 2 x 2.
static void halveImage(const uint16 *src, int w, int h, uint16 *dst) {
	int cols = (w + 1) >> 1, rows = (h + 1) >> 1;
	for (int y = 0; y < rows; y++) {
		const uint16 *a = src + 2 * y * w, *b = a + w;
		for (int x = 0; x < cols; x++) {
			uint16 p[4] = { a[2 * x], a[2 * x + 1], b[2 * x], b[2 * x + 1] };
			int r = 0, g = 0, bl = 0;
			for (int i = 0; i < 4; i++) {
				r += ((p[i] >> 11) & 0x1f) << 3;
				g += ((p[i] >> 5) & 0x3f) << 2;
				bl += (p[i] & 0x1f) << 3;
			}
			r >>= 2;
			g >>= 2;
			bl >>= 2;
			dst[y * cols + x] = (uint16)((((g >> 2) & 0x3f) << 5) | (((r >> 3) & 0x1f) << 11) | ((bl >> 3) & 0x1f));
		}
	}
}

// ---- construction --------------------------------------------------------

BalloonhuntScene::BalloonhuntScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def), _buttons(this),
	_tileCount(0), _key(0), _view(nullptr), _savedBackdrop(nullptr), _cursorAnim(nullptr),
	_radarSize(-1), _compassSize(-1), _mode(kModeMenu), _modeSel(0), _trackSel(0), _level(0),
	_trackCount(0), _next(0), _size(64.0f), _timeLimit(0), _bonusTime(0), _startTime(0), _lastHit(0), _endTime(0),
	_nextSpawn(0), _score(0), _scoreShown(-1), _gas(100.0f), _x(100.0f), _y(330.0f), _z(100.0f),
	_heading(0.0f), _speed(600.0f), _zoom(10.0f), _left(0), _right(0), _up2(0), _down2(0), _roll(0), _pitch(0),
	_autopilot(false), _camX(0), _camY(0), _camZ(0), _sinH(0), _cosH(1), _sinR(-1), _cosR(0), _proj(320.0f),
	_invCos(1.0f), _dt(0), _lastFrame(0), _lastPhysics(-1), _mouseX(0), _mouseY(0), _cursorOn(false),
	_cursorUntil(0), _pressed(0), _keyLeft(false), _keyRight(false), _keyUp(false), _keyDown(false), _keyA(false),
	_keyZ(false), _keyPgUp(false), _keyPgDn(false), _master(1.0f), _mixRate(22050), _mixPitch(1.0f), _mixVolume(255),
	_mixPaused(false), _warp(0) {
	memset(&_slider, 0, sizeof(_slider));
	memset(_turnTable, 0, sizeof(_turnTable));
	memset(_climbTable, 0, sizeof(_climbTable));
	memset(_radarSpan, 0, sizeof(_radarSpan));
	memset(_radarLeft, 0, sizeof(_radarLeft));
	memset(_radarRight, 0, sizeof(_radarRight));
	memset(_compassSpan, 0, sizeof(_compassSpan));
	memset(_compassLeft, 0, sizeof(_compassLeft));
	memset(_compassRight, 0, sizeof(_compassRight));
	memset((void *)_track, 0, sizeof(_track));
	memset((void *)_free, 0, sizeof(_free));
	for (int i = 0; i < kTrackBalloons; i++)
		_trackSmall[i] = nullptr;
	for (int i = 0; i < 4; i++)
		_marker[i].x = _marker[i].y = 0.0f;
}

BalloonhuntScene::~BalloonhuntScene() {
	g_system->getMixer()->stopHandle(_mixHandle);
	freeWorld();
	// the backdrop goes back so the scene frees it
	if (_savedBackdrop) {
		_backdrop = _savedBackdrop;
		_savedBackdrop = nullptr;
	}
	delete _view;
}

bool BalloonhuntScene::load() {
	if (!Scene::load())
		return false;
	_font.load("Amerigo BT_10_");
	return true;
}

// FUN_00413940 + FUN_00404650 / FUN_00404890: a bitmap of the scene in the
// 16 bit display format, its colour key the pixel at (0, 0).
bool BalloonhuntScene::loadImage(const Common::String &name, Image16 &img) {
	Graphics::ManagedSurface *s = resources()->loadBitmap(_name, "bitmap/" + name + ".bmp", format565());
	if (!s)
		return false;
	img.w = s->w;
	img.h = s->h;
	img.pix.resize(img.w * img.h);
	for (int y = 0; y < img.h; y++) {
		const uint16 *src = (const uint16 *)s->getBasePtr(0, y);
		memcpy(&img.pix[y * img.w], src, img.w * 2);
	}
	img.key = img.pix.empty() ? 0 : img.pix[0];
	delete s;
	return true;
}

// ---- init / close --------------------------------------------------------

// FUN_00429b10: the init handler.
void BalloonhuntScene::onInit(int arg) {
	_modeSel = 0;
	_trackSel = 0;
	_modeNames[kSelNormal] = _vm->getString("balloonhunt:NORMAL");
	_modeNames[kSelPractice] = _vm->getString("balloonhunt:PRACTICE");
	_modeNames[kSelSingle] = _vm->getString("balloonhunt:SINGLE");
	_mode = kModeMenu;
	if (resources()->loadIni(_name, "tracks.ini", _tracks)) {
		for (const auto &s : _tracks.getSections())
			_levels.push_back(s.name);
	} else {
		warning("Balloonhunt: tracks.ini not found");
	}
	// FUN_004017f0: 640x480; FUN_0040cb20 shows the loading box (not
	// ported); FUN_00401b30 limits the blits to the screen.
	_vm->setScreenSize(kScreenW, kScreenH);

	// The engine's sound volume (FUN_0040b3f0: the options times sound.ini's sound=)
	_master = _vm->channelVolume(FlaaklypaEngine::kVolumeSound);

	// RGB565 -> screen colours; the game's cursor element is keyed with a
	// colour the table never gives (RGB 1,1,1).
	const Graphics::PixelFormat &f = _vm->_screen->format;
	_lut.resize(65536);
	for (uint c = 0; c < 65536; c++) {
		uint r = c >> 11, g = (c >> 5) & 0x3f, b = c & 0x1f;
		_lut[c] = f.RGBToColor((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2));
	}
	_key = f.RGBToColor(1, 1, 1);
	_bb.resize(kScreenW * kScreenH);
	for (uint i = 0; i < _bb.size(); i++)
		_bb[i] = 0;

	loadImage("autopilot", _autopilotImg);
	loadImage("cursor", _cursorImg);
	loadImage("numbers", _numbers);
	buildDither();
	// skynew.bmp (stored twice per pixel in the original, written in pairs)
	{
		Image16 sky;
		if (loadImage("skynew", sky) && sky.w == kMapSize && sky.h == kMapSize)
			_sky = sky.pix;
		else
			warning("Balloonhunt: skynew.bmp is not 1024x1024");
		if (_sky.empty())
			_sky.resize(kMapSize * kMapSize, 0);
	}
	// FUN_0042c080: the projection and the tables of the control ramps
	double half = (double)kFov * 0.5f;
	_proj = (float)(320.0f / tan(half));
	_invCos = (float)(1.0 / cos(half));
	for (int i = 0; i < 100; i++) {
		_turnTable[i] = (float)((double)i * 0.01f);
		_climbTable[i] = (float)((double)i * 1.5f);
	}
	loadWorld();

	// Solan's gyrocopter: per roll step a group of up to three frames
	// (0x60a1f4): staylft, turnlft7..1, frwrd, turnrht1..7, stayrht
	loadImage("staylft1", _solan[0][0]);
	loadImage("staylft2", _solan[0][1]);
	loadImage("staylft3", _solan[0][2]);
	for (int i = 7; i > 0; i--)
		loadImage(Common::String::format("turnlft%d", i), _solan[8 - i][0]);
	loadImage("frwrd1", _solan[8][0]);
	loadImage("frwrd2", _solan[8][1]);
	loadImage("frwrd3", _solan[8][2]);
	for (int i = 1; i <= 7; i++)
		loadImage(Common::String::format("turnrht%d", i), _solan[8 + i][0]);
	loadImage("stayrht1", _solan[16][0]);
	loadImage("stayrht2", _solan[16][1]);
	loadImage("stayrht3", _solan[16][2]);
	loadImage("up1", _up[0]);
	loadImage("up2", _up[1]);
	loadImage("stayup1", _stayUp[0]);
	loadImage("stayup2", _stayUp[1]);
	loadImage("stayup3", _stayUp[2]);
	loadImage("down1", _down[0]);
	loadImage("down2", _down[1]);
	// DAT_00608450 holds staydown2, staydown1, staydown3 in this order
	loadImage("staydown2", _stayDown[0]);
	loadImage("staydown1", _stayDown[1]);
	loadImage("staydown3", _stayDown[2]);
	loadImage("uprht1", _upRight[0]);
	loadImage("uprht2", _upRight[1]);
	loadImage("downrht1", _downRight[0]);
	loadImage("downrht2", _downRight[1]);
	loadImage("uplft1", _upLeft[0]);
	loadImage("uplft2", _upLeft[1]);
	loadImage("downlft1", _downLeft[0]);
	loadImage("downlft2", _downLeft[1]);
	// the radar's map, the compass card and the gyroscope's pictures
	{
		Image16 map;
		if (loadImage("map3", map) && map.w == kMapSize && map.h == kMapSize)
			_radarMap = map.pix;
		else
			warning("Balloonhunt: map3.bmp is not 1024x1024");
		if (_radarMap.empty())
			_radarMap.resize(kMapSize * kMapSize, 0);
	}
	loadImage("compskive", _compassTex);
	if (_compassTex.pix.size() < 64 * 64)
		_compassTex.pix.resize(64 * 64, 0);
	for (int i = 0; i < 7; i++)
		for (int j = 0; j < 7; j++)
			loadImage(Common::String::format("%02d%02d", i, j), _gyro[i * 7 + j]);
	// FUN_0042b9f0: the 24 numbered balloons and their small pictures
	for (int i = 0; i < kBalloonTypes; i++) {
		_balloonImages[i].name = Common::String::format("%02d", i + 1);
		loadImage(_balloonImages[i].name, _balloonImages[i].image);
		loadImage(Common::String::format("small %02d", i + 1), _smallBalloons[i]);
	}
	loadMix();
	for (int i = 0; i < ARRAYSIZE(kEffects); i++)
		defineAnim(kEffects[i], true, false, 0, 0, 0, 0, false);

	// FUN_00406690 / FUN_00406ec0(3): the buttons at z 3
	_buttons.setFont(&_font);
	_buttons.setDefaultZ(3);

	// The original draws the view into the back buffer before the scene's
	// elements (the interface bitmaps, labels and buttons) and its
	// instruments and cursor after them (event 0x119). Here the view, the
	// interface bitmaps (elements of the original, z 1) and the instruments
	// go into a 16 bit buffer that becomes the scene's backdrop during a
	// round; the labels, buttons and the game's cursor stay elements.
	_view = new Graphics::ManagedSurface(kScreenW, kScreenH, _vm->_screen->format);
	_view->fillRect(Common::Rect(kScreenW, kScreenH), _lut[0]);
	loadImage("interface top", _ifcTop);
	loadImage("interface bottom", _ifcBottom);
	_cursorAnim = defineSurfaceAnim("bh_cursor", MAX(_cursorImg.w, 1), MAX(_cursorImg.h, 1), _key);
	for (int y = 0; y < _cursorImg.h; y++)
		for (int x = 0; x < _cursorImg.w; x++) {
			uint16 c = _cursorImg.pix[y * _cursorImg.w + x];
			_cursorAnim->surface()->setPixel(x, y, c == _cursorImg.key ? _key : _lut[c]);
		}

	// FUN_0042e040 (first call): the aircraft on the radar, an arrow at its
	// centre (321, 422) made of two triangles
	_marker[0].x = 321.0f;
	_marker[0].y = 422.0f;
	_marker[1].x = (float)(cos(-1.5707963705062866) * 10.0f + 321.0f);
	_marker[1].y = (float)(sin(-1.5707963705062866) * 10.0f + 422.0f);
	_marker[2].x = (float)(cos(0.5707963705062866) * 10.0f + 321.0f);
	_marker[2].y = (float)(sin(0.5707963705062866) * 10.0f + 422.0f);
	_marker[3].x = (float)(cos(2.570796489715576) * 10.0f + 321.0f);
	_marker[3].y = (float)(sin(2.570796489715576) * 10.0f + 422.0f);

	// Development aid: "balloonhunt_warp=<n>" puts Solan in front of the next
	// balloon at the start of a track and after each of the next n balloons
	// (in a practice flight in front of each of the next n new balloons).
	_warp = ConfMan.hasKey("balloonhunt_warp") ? ConfMan.getInt("balloonhunt_warp") : 0;

	// TODO: tournament mode (FUN_00419490): FUN_00419660, the round's track
	// (FUN_004194a0) and FUN_0042ad60(2, track) at once.
	showStartScreen();
}

// FUN_0042bdd0
void BalloonhuntScene::onClose() {
	g_system->getMixer()->stopHandle(_mixHandle);
	_buttons.removeAll();
	if (_savedBackdrop) {
		_backdrop = _savedBackdrop;
		_savedBackdrop = nullptr;
	}
	freeWorld();
	// FUN_004017f0(800, 600)
	_vm->setScreenSize(800, 600);
}

void BalloonhuntScene::freeWorld() {
	for (auto *img : _images)
		delete img;
	_images.clear();
	_sprites.clear();
	for (int i = 0; i < kCells * kCells; i++)
		_cells[i].clear();
	_launchSites.clear();
	for (int i = 0; i < 4; i++)
		_tiles[i].clear();
	_map.clear();
	_drawList.clear();
}

// FUN_0042a410: 16 masks of 128 x 128 bytes for the fading sprites; mask n
// lets a pixel through unless rand() % 16 < n + 1.
void BalloonhuntScene::buildDither() {
	_dither.resize(16 * 0x4000);
	for (int level = 0; level < 16; level++)
		for (int i = 0; i < 0x4000; i++)
			_dither[level * 0x4000 + i] = (int)_vm->getRandomNumber(15) < level + 1 ? 0 : 1;
}

// FUN_004139c0: mix2.wav, the engine sound.
void BalloonhuntScene::loadMix() {
	Common::SeekableReadStream *s = resources()->open(_name, "sound/mix2.wav");
	if (!s) {
		warning("Balloonhunt: sound/mix2.wav not found");
		return;
	}
	_mixData.resize(s->size());
	s->read(_mixData.data(), _mixData.size());
	delete s;
}

// ---- the world (FUN_0042a470("yo", "yo")) -------------------------------------

void BalloonhuntScene::loadWorld() {
	if (!loadTiles())
		warning("Balloonhunt: fulltile.bmp not found");
	loadMap("yo");
	loadSprites("yo");
}

// fulltile.bmp is cut into 64x64 tiles row by row; each gets its 32x32,
// 16x16 and 8x8 versions (FUN_0042a9a0).
bool BalloonhuntScene::loadTiles() {
	Image16 full;
	if (!loadImage("fulltile", full))
		return false;
	int cols = full.w / 64, rows = full.h / 64;
	_tileCount = cols * rows;
	static const int kSizes[4] = { 64, 32, 16, 8 };
	for (int l = 0; l < 4; l++)
		_tiles[l].resize(_tileCount * kSizes[l] * kSizes[l]);
	for (int r = 0; r < rows; r++) {
		for (int c = 0; c < cols; c++) {
			int t = r * cols + c;
			uint16 *t64 = &_tiles[0][t * 4096];
			for (int y = 0; y < 64; y++)
				memcpy(t64 + y * 64, &full.pix[(r * 64 + y) * full.w + c * 64], 128);
			halveImage(t64, 64, 64, &_tiles[1][t * 1024]);
			halveImage(&_tiles[1][t * 1024], 32, 32, &_tiles[2][t * 256]);
			halveImage(&_tiles[2][t * 256], 16, 16, &_tiles[3][t * 64]);
		}
	}
	return true;
}

// maps/<name>.map: int32 columns, int32 rows, then a tile number + 1 per
// cell (int32, row by row); 0 or less is the sea tile 0xb4, numbers past
// the tiles the last one. The map is 1024 x 1024 tiles, the rest is sea.
void BalloonhuntScene::loadMap(const Common::String &name) {
	_map.resize(kMapSize * kMapSize);
	for (uint i = 0; i < _map.size(); i++)
		_map[i] = kSeaTile;
	Common::SeekableReadStream *s = resources()->open(_name, "maps/" + name + ".map");
	if (!s) {
		warning("Balloonhunt: maps/%s.map not found", name.c_str());
		return;
	}
	int cols = s->readSint32LE(), rows = s->readSint32LE();
	for (int r = 0; r < rows && r < kMapSize; r++) {
		for (int c = 0; c < cols; c++) {
			int32 t = s->readSint32LE() - 1;
			if (t < 0)
				t = kSeaTile;
			if (_tileCount <= t)
				t = _tileCount - 1;
			if (c < kMapSize)
				_map[r * kMapSize + c] = (uint16)t;
		}
	}
	delete s;
	debug(1, "Balloonhunt: map %s %dx%d, %d tiles", name.c_str(), cols, rows, _tileCount);
}

const BalloonhuntScene::SpriteImage *BalloonhuntScene::findImage(const Common::String &name) {
	for (auto *img : _images)
		if (img->name == name)
			return img;
	// FUN_0042aba0
	SpriteImage *img = new SpriteImage();
	img->name = name;
	if (!loadImage(name, img->image)) {
		delete img;
		return nullptr;
	}
	_images.push_back(img);
	return img;
}

// maps/<name>.txt: a billboard per line, "\"<bitmap>\" x y z" (x and z
// times 4, y the height, 0 on the ground). Each is bottom aligned, 64 x 64
// units, kept in the cell of 2000 x 2000 units it stands in (8 x 8 cells).
// The "sprite4" ones (houses) are where the practice balloons rise.
void BalloonhuntScene::loadSprites(const Common::String &name) {
	Common::SeekableReadStream *s = resources()->open(_name, "maps/" + name + ".txt");
	if (!s) {
		warning("Balloonhunt: maps/%s.txt not found", name.c_str());
		return;
	}
	const SpriteImage *launch = nullptr;
	while (!s->eos()) {
		Common::String line = s->readLine();
		if (s->err())
			break;
		char buf[256];
		float a = 0, b = 0, c = 0;
		if (sscanf(line.c_str(), "%255s %f %f %f", buf, &a, &b, &c) != 4)
			continue;
		Common::String file(buf[0] == '"' ? buf + 1 : buf);
		uint q = file.findFirstOf('"');
		if (q != Common::String::npos)
			file = file.substr(0, q);
		const SpriteImage *img = findImage(file);
		if (!img)
			continue;
		if (!scumm_stricmp(file.c_str(), "sprite4"))
			launch = img;
		// FUN_0042ac80
		Sprite3D sp;
		memset((void *)&sp, 0, sizeof(sp));
		sp.image = img;
		sp.alignX = 1;
		sp.alignY = 2;
		sp.w = sp.h = 64.0f;
		sp.x = a * kMapScale;
		sp.y = b * kMapScale;
		sp.z = c * kMapScale;
		int cz = CLIP(ftol((double)sp.z * kCellScale), 0, 7);
		int cx = CLIP(ftol((double)sp.x * kCellScale), 0, 7);
		_cells[cx + cz * kCells].push_back(_sprites.size());
		_sprites.push_back(sp);
	}
	delete s;
	for (uint i = 0; i < _sprites.size(); i++)
		if (launch && _sprites[i].image == launch)
			_launchSites.push_back(i);
	debug(1, "Balloonhunt: %d sprites, %d images, %d launch sites", (int)_sprites.size(), (int)_images.size(), (int)_launchSites.size());
}

// ---- the start screen ------------------------------------------------------------

// FUN_0042ba40
void BalloonhuntScene::showStartScreen() {
	_buttons.setFont(&_font);
	_buttons.add(kButtonHelp, 0, 0, "help out", "help in", nullptr, nullptr, "", true);
	_buttons.add(kButtonExit, 0x247, 0, "exit out", "exit in", nullptr, nullptr, "", true);
	_buttons.add(kButtonStart, 0x11f, 0x199, "strbtnoff", "strbtnon", nullptr, nullptr, _vm->getString("balloonhunt:START"), false);
	_buttons.add(kButtonModeLeft, 0xd6, 0x159, "left arrow out", "left arrow", nullptr, nullptr, "", false);
	_buttons.add(kButtonModeRight, 0x199, 0x159, "right arrow out", "right arrow", nullptr, nullptr, "", false);
	showLabel("bh_mode", 232, 342, 175, 19, 4, "");
	addAnim("start button", Anim::kDefaultPos, Anim::kDefaultPos, 1);
	selectMode();
	// Solan flies by once (removed when done, FUN_0042f190)
	addAnim("solan", Anim::kDefaultPos, Anim::kDefaultPos, 4);
	playAnim("solan");
}

// FUN_0042bb70: the mode chosen with the arrows; the time trial adds the
// track selector (the "bane velger" bar with a scroll bar).
void BalloonhuntScene::selectMode() {
	_modeSel = wrapIndex(_modeSel, 3, 0);
	drawLabel("bh_mode", _modeNames[_modeSel]);
	if (_modeSel != kSelSingle) {
		removeTrackSelector();
		return;
	}
	addAnim("bane velger", Anim::kDefaultPos, Anim::kDefaultPos, 2);
	showLabel("bh_tracknum", 397, 377, 26, 17, 4, "");
	showLabel("bh_trackname", 209, 377, 44, 18, 4, _vm->getString("balloonhunt:TRACK2"));
	// The SCROLL module: FUN_00408c70, FUN_004094c0(5) (thumbs at z 5),
	// FUN_004090e0(0x11, 283, 379, 76, 14, "slider", 0x100: horizontal),
	// arrows 2000 / 2001 (FUN_004091f0) and a step of 1 (FUN_004090c0).
	_slider.active = true;
	_slider.x = 0x11b;
	_slider.y = 0x17b;
	_slider.w = 0x4c;
	_slider.h = 0xe;
	_slider.max = 0;
	_slider.value = 0;
	_slider.dragging = false;
	const Graphics::ManagedSurface *thumb = _buttons.bitmap("slider");
	_slider.thumbW = thumb ? thumb->w : 0;
	sliderSetValue(0);
	_buttons.add(kSliderLeft, 0x100, 0x17b, "left arrow out", "left arrow", nullptr, nullptr, "", false);
	_buttons.add(kSliderRight, 0x172, 0x17b, "right arrow out", "right arrow", nullptr, nullptr, "", false);
	// FUN_00408e20(0x11, 0, sections - 1), FUN_00408eb0(0x11, track)
	_slider.max = MAX((int)_levels.size() - 1, 0);
	sliderSetValue(_trackSel);
	sliderChanged(_trackSel);
}

// FUN_0042bcc0
void BalloonhuntScene::removeTrackSelector() {
	if (isAnimAdded("bane velger"))
		removeAnim("bane velger");
	if (!_slider.active)
		return;
	removeLabel("bh_tracknum");
	// FUN_004091d0 -> FUN_00408dc0: the thumb and the arrows go
	if (_slider.thumb && isAnimAdded("bh_thumb"))
		removeAnim("bh_thumb");
	_buttons.remove(kSliderLeft);
	_buttons.remove(kSliderRight);
	_slider.active = false;
	_slider.dragging = false;
	removeLabel("bh_trackname");
}

// FUN_0042bd20
void BalloonhuntScene::removeStartScreen() {
	_buttons.remove(kButtonStart);
	_buttons.remove(kButtonModeLeft);
	_buttons.remove(kButtonModeRight);
	removeLabel("bh_mode");
	_buttons.remove(kButtonHelp);
	_buttons.remove(kButtonExit);
	if (isAnimAdded("start button"))
		removeAnim("start button");
	if (isAnimAdded("bane velger"))
		removeAnim("bane velger");
	if (isAnimAdded("solan"))
		removeAnim("solan");
	if (_modeSel == kSelSingle)
		removeTrackSelector();
}

// FUN_00408f30 (horizontal): the thumb's centre follows the position within
// the track; the value is (pos - x) * range / width rounded, halves away
// from zero; a new value is reported (event 0x117).
void BalloonhuntScene::sliderSetPos(int pos) {
	if (!_slider.active)
		return;
	if (pos < 0)
		pos = 0;
	int right = _slider.x + _slider.w;
	if (pos > right)
		pos = right;
	if (pos < _slider.x)
		pos = _slider.x;
	double v = (double)((pos - _slider.x) * _slider.max) / (double)_slider.w;
	v = 0.0 < v ? v + 0.5f : v - 0.5f;
	int old = _slider.value;
	_slider.value = ftol(v);
	_slider.lastX = pos;
	// FUN_00412cf0: the thumb (opaque, hotspot 0x11, z 5) centred there
	const Graphics::ManagedSurface *thumb = _buttons.bitmap("slider");
	int tw = thumb ? thumb->w : 0, th = thumb ? thumb->h : 0;
	int cy = _slider.h / 2 + _slider.y;
	if (!_slider.thumb || !isAnimAdded("bh_thumb"))
		_slider.thumb = _buttons.showBitmap("bh_thumb", "slider", false, kSliderId, pos - tw / 2, cy - th / 2, 5) != nullptr;
	else
		anim("bh_thumb")->setPos(pos - tw / 2, cy - th / 2);
	if (_slider.value != old)
		sliderChanged(_slider.value);
}

// FUN_00408eb0
void BalloonhuntScene::sliderSetValue(int value) {
	value = CLIP(value, 0, _slider.max);
	if (_slider.max > 0)
		sliderSetPos(_slider.w * value / _slider.max + _slider.x);
	else
		sliderSetPos(0);
}

// FUN_004093f0 / FUN_004092d0: a press on the thumb drags it; one on the
// track (widened by half the thumb on each side) moves it there first.
void BalloonhuntScene::sliderMouseDown(int hotspot, int x, int y) {
	if (!_slider.active)
		return;
	if (hotspot == kSliderId) {
		_slider.dragging = true;
		return;
	}
	Common::Rect track(_slider.x - _slider.thumbW / 2, _slider.y, _slider.x - _slider.thumbW / 2 + _slider.w + _slider.thumbW, _slider.y + _slider.h);
	bool onThumb = _slider.thumb && isAnimAdded("bh_thumb") && anim("bh_thumb")->rect().contains(x, y);
	if (!track.contains(x, y) && !onThumb)
		return;
	sliderSetPos(x);
	_slider.dragging = true;
}

// FUN_004092b0 (event 0x10b): the dragged thumb snaps to its value.
void BalloonhuntScene::sliderRelease() {
	if (!_slider.active || !_slider.dragging)
		return;
	_slider.dragging = false;
	sliderSetValue(_slider.value);
}

// FUN_00429ab0 (event 0x117 of the scroll bar 0x11): the track number.
void BalloonhuntScene::sliderChanged(int value) {
	_trackSel = value;
	drawLabel("bh_tracknum", Common::String::format("%d", value + 1));
}

// ---- input -------------------------------------------------------------------

// FUN_0042f050 (event 0x105) and the BUTTON / SCROLL modules after it.
void BalloonhuntScene::onMouseDown(int hotspot, int x, int y) {
	// the interface bottom (hotspot 10 of its element) is part of the
	// backdrop here
	if (_mode != kModeMenu && hotspot == 0 && y >= 336 && x >= 0 && x < _ifcBottom.w && y - 336 < _ifcBottom.h &&
	    _ifcBottom.pix[(y - 336) * _ifcBottom.w + x] != _ifcBottom.key)
		hotspot = 10;
	if (hotspot != 0)
		_pressed = hotspot;
	sliderMouseDown(hotspot, x, y);
	_buttons.mouseDown(hotspot, x, y);
}

// FUN_0042f080 (event 0x106) and the buttons (event 0x116).
void BalloonhuntScene::onMouseUp(int hotspot, int x, int y) {
	_pressed = 0;
	sliderRelease();
	int id = _buttons.mouseUp();
	if (id)
		handleButton(id);
}

void BalloonhuntScene::onMouseMove(int hotspot, int x, int y) {
	_buttons.setHover(hotspot);
}

// FUN_0042f090 (event 0x116), FUN_00409450 for the scroll bar's arrows.
void BalloonhuntScene::handleButton(int id) {
	switch (id) {
	case kButtonStart:
		if (_modeSel == kSelNormal)
			startRound(kModeNormal, 0);
		else if (_modeSel == kSelPractice)
			startRound(kModePractice, 0);
		else if (_modeSel == kSelSingle)
			startRound(kModeSingle, _trackSel);
		break;
	case kButtonHelp:
		// FUN_0041e580
		_vm->showHelp();
		break;
	case kButtonExit:
		exitPressed();
		break;
	case kButtonModeLeft:
		_modeSel--;
		selectMode();
		break;
	case kButtonModeRight:
		_modeSel++;
		selectMode();
		break;
	case kSliderLeft:
		sliderSetValue(_slider.value - 1);
		break;
	case kSliderRight:
		sliderSetValue(_slider.value + 1);
		break;
	default:
		break;
	}
}

// The exit button and Escape (FUN_0042ef20): a practice flight goes back to
// the start screen, anything else leaves the scene (FUN_0040cc30, which
// asks first while a round runs).
void BalloonhuntScene::exitPressed() {
	if (_mode == kModePractice)
		endRound(0);
	else
		_vm->endGame();
}

// The original reads the flight keys with GetAsyncKeyState in its frame
// handler; the key handler (FUN_0042ef20) only acts on Escape and swallows it.
void BalloonhuntScene::onKey(const Common::KeyState &key) {
	switch (key.keycode) {
	case Common::KEYCODE_LEFT:
		_keyLeft = true;
		break;
	case Common::KEYCODE_RIGHT:
		_keyRight = true;
		break;
	case Common::KEYCODE_UP:
		_keyUp = true;
		break;
	case Common::KEYCODE_DOWN:
		_keyDown = true;
		break;
	case Common::KEYCODE_a:
		_keyA = true;
		break;
	case Common::KEYCODE_z:
		_keyZ = true;
		break;
	case Common::KEYCODE_PAGEUP:
		_keyPgUp = true;
		break;
	case Common::KEYCODE_PAGEDOWN:
		_keyPgDn = true;
		break;
	case Common::KEYCODE_ESCAPE:
		exitPressed();
		break;
	default:
		break;
	}
}

void BalloonhuntScene::onKeyUp(const Common::KeyState &key) {
	switch (key.keycode) {
	case Common::KEYCODE_LEFT:
		_keyLeft = false;
		break;
	case Common::KEYCODE_RIGHT:
		_keyRight = false;
		break;
	case Common::KEYCODE_UP:
		_keyUp = false;
		break;
	case Common::KEYCODE_DOWN:
		_keyDown = false;
		break;
	case Common::KEYCODE_a:
		_keyA = false;
		break;
	case Common::KEYCODE_z:
		_keyZ = false;
		break;
	case Common::KEYCODE_PAGEUP:
		_keyPgUp = false;
		break;
	case Common::KEYCODE_PAGEDOWN:
		_keyPgDn = false;
		break;
	default:
		break;
	}
}

bool BalloonhuntScene::handlesKey(const Common::KeyState &key) {
	return key.keycode == Common::KEYCODE_ESCAPE;
}

// FUN_0042f190 (event 0x110)
void BalloonhuntScene::onAnimFinished(Anim *a) {
	if (!strcmp(a->name(), "solan"))
		removeAnim("solan");
}

// Events 0x11b (FUN_0042ef50) / 0x11c (FUN_0042eff0): under a dialog the
// engine sound stops and starts again afterwards. (The original also copies
// the view into an element at z 2, since it draws the view itself every
// frame; here the view's element keeps it.)
void BalloonhuntScene::pauseAnims(bool pause) {
	Scene::pauseAnims(pause);
	if (pause) {
		if (_mode == kModeMenu)
			return;
		_mixPaused = false;
		if (isMixPlaying()) {
			stopMix();
			_mixPaused = true;
		}
	} else {
		if (_mixPaused) {
			playMix();
			setMixVolume(0.85f);
		}
		// The original polls the keys (GetAsyncKeyState); a key released
		// under the dialog never reaches the scene here.
		_keyLeft = _keyRight = _keyUp = _keyDown = _keyA = _keyZ = _keyPgUp = _keyPgDn = false;
	}
}

// ---- rounds --------------------------------------------------------------------

// FUN_0042ad60(mode, track): the start button.
void BalloonhuntScene::startRound(int mode, int track) {
	if (_mode != kModeMenu)
		return;
	for (int i = 0; i < ARRAYSIZE(kAmbience); i++) {
		playAnim(kAmbience[i]);
		anim(kAmbience[i])->setVolume(0);
	}
	ambience();
	// FUN_00413a40(mix2, loop) at the sound volume, then 85 % of it
	playMix();
	setMixVolume(0.85f);
	for (int i = 0; i < kFreeBalloons; i++)
		_free[i].type = 4;
	uint32 now = _vm->getGameMillis();
	_nextSpawn = now;
	_dt = 0.0f;
	_roll = 0.0f;
	_left = _right = 0.0f;
	_pitch = 0.0f;
	_up2 = _down2 = 0.0f;
	_gas = 100.0f;
	_x = 1000.0f;
	_y = 90.0f;
	_z = 1000.0f;
	_heading = 0.0f;
	_speed = 300.0f;
	_lastFrame = now;
	_lastPhysics = -1;
	_zoom = 120.0f;
	// FUN_00412840("interface bottom" / "interface top", z 1): drawn into
	// the back buffer here (afterDraw())
	for (int i = 0; i < ARRAYSIZE(kRoundLabels); i++) {
		const auto &l = kRoundLabels[i];
		showLabel(l.elem, l.x0, l.y0, l.x1 - l.x0, l.y1 - l.y0, 2, l.text ? _vm->getString(l.text) : "");
	}
	_autopilot = false;
	_mode = mode;
	_scoreShown = -1;
	_score = 0;
	_level = track;
	loadLevel();
	_vm->setGameRunning(true);
	// FUN_0040ef90(0) / FUN_0040f140(0): no system cursor; the game draws
	// its own after the mouse has moved
	setCursorTable(kNoCursor);
	Common::Point m = _vm->mousePos();
	_mouseX = m.x;
	_mouseY = m.y;
	_cursorOn = false;
	removeStartScreen();
	_buttons.add(kButtonHelp, 0, 0, "help-ut", "help-inn", nullptr, nullptr, "", true);
	_buttons.add(kButtonExit, 0x246, 0, "exit-ut", "exit-inn", nullptr, nullptr, "", true);
	// FUN_00412710(0): the view instead of the backdrop
	_savedBackdrop = _backdrop;
	_backdrop = _view;
	debug(1, "Balloonhunt: round in mode %d from level %d", _mode, _level);
	devWarp();
}

// FUN_0042b5f0: the next section of tracks.ini: the start, the balloons'
// size, the time, and its track file.
void BalloonhuntScene::loadLevel() {
	int count = (int)_levels.size();
	if (count <= 0)
		return;
	const Common::String &section = _levels[_level % count];
	_level++;
	auto getStr = [&](const char *key, const char *def) {
		Common::String v;
		if (_tracks.getKey(key, section, v)) {
			v = Resources::iniValue(v);
			v.trim();
			return v;
		}
		return Common::String(def);
	};
	Common::String track = getStr("track", "track1");
	_size = (float)atof(getStr("size", "64").c_str());
	_x = (float)atof(getStr("startx", "6700").c_str());
	_y = (float)atof(getStr("starty", "90").c_str());
	_z = (float)atof(getStr("startz", "6700").c_str());
	double angle = atof(getStr("startangle", "0").c_str());
	_speed = 300.0f;
	_heading = (float)(angle * kPi * 0.0055555557f);
	_timeLimit = atoi(getStr("time", "100").c_str()) * 1000;
	if (count < _level) {
		// later rounds: two thirds, then a third of the time, rounded up to
		// the next 5 s (5 s more when it is a multiple)
		int k = 3 - _level / count;
		if (k < 1)
			k = 1;
		int t = (_timeLimit * k) / 3;
		int add = 5000 - t % 5000;
		if (add < 0)
			add = 0;
		else if (5000 < add)
			add = 5000;
		_timeLimit = t + add;
	}
	loadTrack(track);
	int k = _trackCount - 3;
	if (k <= 1)
		k = 1;
	_bonusTime = _timeLimit / k;
	drawLabel("bh_level", Common::String::format("%d", _level));
	_startTime = _lastHit = _vm->getGameMillis();
	debug(1, "Balloonhunt: level %d [%s] track %s: %d balloons, size %g, %d ms, bonus within %d ms, start %g,%g,%g",
	      _level, section.c_str(), track.c_str(), _trackCount, _size, _timeLimit, _bonusTime, _x, _y, _z);
}

// FUN_0042b820: tracks/<name>.txt, up to 24 balloons in the order they are
// to be hit, "\"<name>\" x y z" (the name unused, x and z times 4, the
// height -4 times y within 90..1200), centred, of the level's size.
void BalloonhuntScene::loadTrack(const Common::String &name) {
	_next = 0;
	_trackCount = 0;
	Common::SeekableReadStream *s = resources()->open(_name, "tracks/" + name + ".txt");
	if (!s) {
		warning("Balloonhunt: tracks/%s.txt not found", name.c_str());
		return;
	}
	while (!s->eos() && _trackCount != kTrackBalloons) {
		Common::String line = s->readLine();
		if (s->err())
			break;
		char buf[256];
		float a = 0, b = 0, c = 0;
		if (sscanf(line.c_str(), "%255s %f %f %f", buf, &a, &b, &c) != 4)
			continue;
		float y = b * -4.0f;
		if (y < kMinAlt)
			y = kMinAlt;
		else if (kMaxAlt < y)
			y = kMaxAlt;
		int i = _trackCount, type = i % kBalloonTypes;
		_trackSmall[i] = &_smallBalloons[type];
		Sprite3D &sp = _track[i];
		memset((void *)&sp, 0, sizeof(sp));
		sp.image = &_balloonImages[type];
		sp.alignX = sp.alignY = 1;
		sp.x = a * kMapScale;
		sp.y = y;
		sp.z = c * kMapScale;
		sp.w = sp.h = _size;
		_trackCount++;
	}
	delete s;
}

// FUN_0042c580(reason): the end of a round; 1 out of fuel, 2 out of time.
void BalloonhuntScene::endRound(int reason) {
	if (_mode == kModeMenu)
		return;
	stopMix();
	_vm->setGameRunning(false);
	debug(1, "Balloonhunt: round over (mode %d, reason %d), %d points, level %d", _mode, reason, _score, _level);
	if (_mode == kModeNormal) {
		if (reason == 1)
			_vm->messageBox("interfaceh:GAMEOVER", "balloonhunt:OUTOFGAS", MessageBox::kButtonOk);
		else if (reason == 2)
			_vm->messageBox("interfaceh:GAMEOVER", "balloonhunt:OUTOFTIME", MessageBox::kButtonOk);
		// TODO: tournament mode (FUN_00419490): FUN_0041f9a0 hands the
		// points to the tournament, which goes on to its next game; the
		// round is not cleaned up then.
		// FUN_0041f9a0(0, points, 0x49d7e8, 0)
		registerScore(_vm, "", _score, kMedals);
	} else if (_mode == kModeSingle) {
		// FUN_0041f8c0("%s %d" of balloonhunt:TRACK and the track, key =
		// the track (1 based), the time in ms, no medals, 0x102)
		Common::String title = Common::String::format("%s %d", _vm->getString("balloonhunt:TRACK").c_str(), _level);
		registerScore(_vm, title, (int32)(_endTime - _startTime), nullptr, _level, kScoreTime);
	}
	_mode = kModeMenu;
	for (int i = 0; i < ARRAYSIZE(kAmbience); i++)
		removeAnim(kAmbience[i]);
	for (int i = 0; i < ARRAYSIZE(kRoundLabels); i++)
		removeLabel(kRoundLabels[i].elem);
	_buttons.remove(kButtonHelp);
	_buttons.remove(kButtonExit);
	// FUN_0040ef90(4): the system cursor again
	setCursorTable(_def->cursors);
	if (_cursorAnim->isAdded())
		_cursorAnim->remove();
	_cursorOn = false;
	// FUN_00412710(""): the backdrop again
	if (_savedBackdrop) {
		_backdrop = _savedBackdrop;
		_savedBackdrop = nullptr;
	}
	showStartScreen();
}

// ---- the frame -----------------------------------------------------------------

void BalloonhuntScene::onUpdate() {
	// the dragged thumb follows the mouse (event 0x10a)
	if (_slider.active && _slider.dragging) {
		Common::Point m = _vm->mousePos();
		if (m.x != _slider.lastX)
			sliderSetPos(m.x);
	}
	if (_mode == kModeMenu)
		return;
	// FUN_0042c110 (event 0x112)
	uint32 now = _vm->getGameMillis();
	_dt = (float)((double)(int32)(now - _lastFrame) * (double)0.001f);
	_lastFrame = now;
	physics();
	ambience();
	updateBalloons();
	render();
	// FUN_0042dcf0 (event 0x119, after the scene is drawn)
	afterDraw();
}

// FUN_0042c7a0: the flight. The arrow keys move four ramps between 0 and 1
// (1.5 / s for turning, 3 / s for climbing, back down when released); the
// heading turns by (left - right) rad/s, the height changes by (up - down)
// x 148.5 / s, both in steps of 1/99. Out of the area the autopilot turns
// back towards the middle and the keys only zoom.
void BalloonhuntScene::physics() {
	uint32 now = _vm->getGameMillis();
	if (_lastPhysics == -1)
		_lastPhysics = now;
	float dt = (float)((double)(int32)(now - (uint32)_lastPhysics) * (double)0.001f);
	double s = sin((double)_heading);
	_x = (float)(s * _speed * dt + _x);
	double c = cos((double)_heading);
	double zl = c * _speed * dt + _z;
	_z = (float)zl;
	double dz = zl - kCentre;
	float dx = (float)((double)_x - kCentre);
	double d = (double)dx * dx + dz * dz;
	if (0.0 < d)
		d = sqrt(d);
	if (d <= kArea) {
		_autopilot = false;
		float t = (float)((double)dt * kTurnRamp);
		_left = _keyLeft ? t + _left : _left - t;
		_right = _keyRight ? t + _right : _right - t;
	} else {
		_autopilot = true;
		double t = (double)dt * kTurnRamp;
		if (dz * s + -(double)dx * c <= 0.0) {
			_right = (float)(t + _right);
			_left = (float)(_left - t);
		} else {
			_left = (float)(t + _left);
			_right = (float)(_right - t);
		}
	}
	_left = clamp01(_left);
	_right = clamp01(_right);
	_roll = _right - _left;
	double h = (double)dt * _turnTable[-ftol((double)_left * -99.0f)] + _heading;
	h = h - (double)dt * _turnTable[-ftol((double)_right * -99.0f)];
	_heading = (float)fmod(h, kTwoPi);
	if (!_autopilot) {
		float t = (float)((double)dt * kClimbRamp);
		_up2 = (_keyUp && _y < kMaxAlt) ? t + _up2 : _up2 - t;
		_down2 = (_keyDown && kMinAlt < _y) ? t + _down2 : _down2 - t;
	}
	_up2 = clamp01(_up2);
	_down2 = clamp01(_down2);
	_pitch = _up2 - _down2;
	double y = (double)dt * _climbTable[-ftol((double)_up2 * -99.0f)] + _y;
	_y = (float)(y - (double)dt * _climbTable[-ftol((double)_down2 * -99.0f)]);
	if (!_autopilot) {
		if (_keyA)
			_speed = (float)((double)dt * kSpeedStep + _speed);
		if (_keyZ)
			_speed = (float)(_speed - (double)dt * kSpeedStep);
	}
	if (_speed < kMinSpeed)
		_speed = kMinSpeed;
	else if (kMaxSpeed < _speed)
		_speed = kMaxSpeed;
	if (_y < kMinAlt)
		_y = kMinAlt;
	else if (kMaxAlt < _y)
		_y = kMaxAlt;
	if (_keyPgDn)
		_zoom = (float)(_zoom - (double)dt * kZoomStep);
	if (_keyPgUp)
		_zoom = (float)((double)dt * kZoomStep + _zoom);
	if (_zoom < kMinZoom)
		_zoom = kMinZoom;
	else if (kMaxZoom < _zoom)
		_zoom = kMaxZoom;
	updateCamera();
	_lastPhysics = now;
}

// FUN_0042cc70: the camera 3.5 units behind Solan and 0.7 above him.
void BalloonhuntScene::updateCamera() {
	double a = (double)_heading + kPi;
	_camX = (float)(sin(a) * kCamBack + _x);
	_camZ = (float)(cos(a) * kCamBack + _z);
	_camY = (float)((double)_y + kCamUp);
	_sinH = (float)sin((double)_heading);
	_cosH = (float)cos((double)_heading);
	double b = (double)_heading - kHalfPi;
	_sinR = (float)sin(b);
	_cosR = (float)cos(b);
}

// FUN_0042b190: the volumes of the ambience clips by the distance to the
// shore, the woods and the sheep and the height, and the engine sound's
// pitch by the speed, the climb and the bank.
void BalloonhuntScene::ambience() {
	float best = 1e8f;
	for (int i = 0; i < 15;) {
		int j = ++i;
		float d = segmentDistance(kShore[i - 1], kShore[j % 15], _x, _z);
		if (d < best)
			best = d;
		if (best < 0.5f)
			break;
	}
	float shore = clamp01((float)(1.0 - (double)best * 0.0016666667f));
	float high = clamp01((float)(1.0 - (double)_y * 0.0016666667f));
	anim("seashore")->setVolume((int)(high * shore * 0.55f * 255.0f));
	float wind = clamp01((float)(((double)_y - 150.0f) * 0.0018181818f));
	anim("wind1")->setVolume((int)(wind * 0.65f * 255.0f));

	best = 3.6e9f;
	for (int i = 0; i < 35; i++) {
		float d = dist2(_x, _z, kForest[i]);
		if (d < best)
			best = d;
		if (d < 0.5f)
			break;
	}
	if (0.0f < best)
		best = sqrtf(best);
	float forest = clamp01((float)(1.0 - (double)best * 0.0025f));
	float low = clamp01((float)(1.0 - (double)_y * 0.0025f));
	anim("forest1")->setVolume((int)(low * forest * 255.0f));

	best = 3.6e9f;
	for (int i = 0; i < 3; i++) {
		float d = dist2(_x, _z, kSheep[i]);
		if (d < best)
			best = d;
		if (d < 0.5f)
			break;
	}
	if (0.0f < best)
		best = sqrtf(best);
	float sheep = clamp01((float)(1.0 - (double)best * 0.0025f));
	anim("sheep")->setVolume((int)(low * sheep * 255.0f));

	setMixPitch((float)(((double)fabsf(_roll) * 0.083f + 1.0) * (1.0 - (double)_pitch * 0.083f) *
	                    (((double)_speed - 300.0f) * 0.00083333335f + 1.0)));
	anim("off limits1")->setVolume(_autopilot ? 255 : 0);
}

// FUN_0042c160: the balloons.
void BalloonhuntScene::updateBalloons() {
	uint32 now = _vm->getGameMillis();
	if (_mode == kModePractice) {
		// one more every 6 s, rising at their type's speed (10 / s) from a
		// house and growing in their first second; a hit gives a point
		if ((int32)(now - _nextSpawn) > 0) {
			int slot = spawnBalloon(_vm->getRandomNumber(3));
			if (slot >= 0) {
				_nextSpawn = _vm->getGameMillis() + 6000;
				if (_warp > 0) {
					// development aid: Solan 300 units before it, low
					_warp--;
					_x = (float)(_free[slot].x - sin((double)_heading) * 300.0);
					_z = (float)(_free[slot].z - cos((double)_heading) * 300.0);
					_y = kMinAlt;
					debug(1, "Balloonhunt: warp to practice balloon %d", slot);
				}
			}
		}
		for (int i = 0; i < kFreeBalloons; i++) {
			FreeBalloon &b = _free[i];
			if (b.type == 4)
				continue;
			float t = (float)((double)(int32)(_vm->getGameMillis() - b.spawn) * 0.001f);
			if (t < 1.0f) {
				b.sprite.w = b.sprite.h = (float)((double)t * 64.0f);
				continue;
			}
			// the rise speed of the type (0x4cbdec + type * 0x90): 10 for all
			b.y = (float)((double)_dt * 10.0f + b.y);
			float dx = _x - b.x;
			double dy = (double)_y - ((double)b.y + 32.0f);
			double dz = (double)_z - b.z;
			if (dz * dz + dy * dy + (double)dx * dx < 784.0f) {
				playClone("baloon-d");
				_score++;
				b.type = 4;
				debug(1, "Balloonhunt: practice balloon %d hit, %d points", i, _score);
			}
			if (kMaxAlt < b.y)
				b.type = 4;
		}
		return;
	}
	if (_mode != kModeNormal && _mode != kModeSingle)
		return;
	// the track: the balloons grow back (5 / s) after a wrong hit; the
	// right one gives 11 points, 17 more within the bonus time, and 50 fuel
	for (int i = _next; i < _trackCount; i++) {
		Sprite3D &b = _track[i];
		if (b.w < _size)
			b.w = (float)((double)_dt * 5.0f + b.w);
		if (_size < b.w)
			b.w = _size;
		b.h = b.w;
		double dx = (double)_x - b.x, dy = (double)_y - b.y, dz = (double)_z - b.z;
		double r = (double)b.w * 0.8f;
		if (!(dz * dz + dy * dy + dx * dx < r * r))
			continue;
		if (i == _next) {
			playClone("baloon-d");
			_next++;
			debug(1, "Balloonhunt: balloon %d of %d hit at %g,%g,%g", _next, _trackCount, _x, _y, _z);
			if (_next == _trackCount)
				levelDone();
			_score += 11;
			if ((int32)(_vm->getGameMillis() - _lastHit) < _bonusTime)
				_score += 17;
			_lastHit = _vm->getGameMillis();
			_gas = _gas + kGasPerBalloon;
			if (100.0f < _gas)
				_gas = 100.0f;
			if (_warp > 0 && _mode != kModeMenu) {
				_warp--;
				devWarp();
			}
		} else {
			playClone("wrong ballon 1");
			b.w = b.h = 0.0f;
			debug(1, "Balloonhunt: wrong balloon %d (next %d)", i + 1, _next + 1);
		}
	}
}

// FUN_0042c460: a practice balloon of a type rises from a random house;
// the slot or -1 (the original returns 1 or 0).
int BalloonhuntScene::spawnBalloon(int type) {
	if (_launchSites.empty())
		return -1;
	for (int i = 0; i < kFreeBalloons; i++) {
		FreeBalloon &b = _free[i];
		if (b.type != 4)
			continue;
		b.sprite.image = &_balloonImages[type];
		b.sprite.alignX = 1;
		b.sprite.alignY = 2;
		b.sprite.w = b.sprite.h = 64.0f;
		const Sprite3D &site = _sprites[_launchSites[_vm->getRandomNumber(_launchSites.size() - 1)]];
		b.x = site.x;
		b.z = site.z;
		b.y = 60.0f;
		b.type = type;
		b.spawn = _vm->getGameMillis();
		debug(2, "Balloonhunt: practice balloon %d (type %d) rises at %g,%g", i, type, b.x, b.z);
		return i;
	}
	return -1;
}

// FUN_0042c510: the last balloon of the track: the time trial ends; a
// points round gets the time left / 200 (at most 300) and the next track.
void BalloonhuntScene::levelDone() {
	uint32 now = _vm->getGameMillis();
	_endTime = now;
	if (_mode == kModeNormal) {
		int bonus = ((int32)(_startTime - now) + _timeLimit) / 200;
		if (300 < bonus)
			bonus = 300;
		_score += bonus;
		debug(1, "Balloonhunt: level %d done, time bonus %d, %d points", _level, bonus, _score);
		playClone("win");
		loadLevel();
		return;
	}
	endRound(0);
}

// Development aid "balloonhunt_warp": Solan in front of the next balloon,
// at its height, flying towards it.
void BalloonhuntScene::devWarp() {
	if (_warp <= 0 || (_mode != kModeNormal && _mode != kModeSingle) || _next >= _trackCount)
		return;
	const Sprite3D &b = _track[_next];
	_x = (float)(b.x - sin((double)_heading) * 250.0);
	_z = (float)(b.z - cos((double)_heading) * 250.0);
	_y = CLIP(b.y, kMinAlt, kMaxAlt);
	debug(1, "Balloonhunt: warp to balloon %d", _next + 1);
}

// ---- the 3D view (FUN_0042cd00) ----------------------------------------------------

void BalloonhuntScene::render() {
	if (_mode == kModeMenu)
		return;
	renderSky();
	renderGround();
	renderSprites();
	// FUN_0042d8e0 computes the needles (drawn with the instruments)
	drawLabels();
}

// FUN_0042d060: the sky, a plane 4250 units up textured with skynew.bmp
// around (32768, 32768), turning with the heading only; 88 lines of
// 320 texels, each drawn as 2 x 2 pixels from row 30 down to row 205.
void BalloonhuntScene::renderSky() {
	double half = (double)kFov * 0.5f;
	double a = half + _heading;
	float sinA = (float)sin(a), cosA = (float)cos(a);
	double b = (double)_heading - half;
	float sinB = (float)sin(b), cosB = (float)cos(b);
	float k = (float)((double)_proj * _invCos * kSkyHeight);
	uint16 *row = &_bb[kViewTop * kScreenW];
	for (int j = 0xaf; j >= 0; j -= 2) {
		double d = -((double)k / (double)(j + 1));
		float lx = (float)(d * sinA + 32768.0f);
		float lz = (float)(d * cosA + 32768.0f);
		uint32 du = (uint32)ftol((d * sinB + 32768.0f - lx) * kTexStep);
		uint32 dv = (uint32)ftol((d * cosB + 32768.0f - lz) * kTexStep);
		uint32 u = (uint32)ftol((double)lx * kFixed);
		uint32 v = (uint32)ftol((double)lz * kFixed);
		for (int i = 0; i < kScreenW; i += 2) {
			uint16 c = _sky[((v >> 12) & 0xffc00) + ((u >> 22) & 0x3ff)];
			row[i] = row[i + 1] = c;
			row[kScreenW + i] = row[kScreenW + i + 1] = c;
			u += du;
			v += dv;
		}
		row += 2 * kScreenW;
	}
}

// FUN_0042cd30: the ground, rows 205..379: per row the distance along the
// frustum's edges (35000 - 5 x row beyond 35000), the texture coordinates
// in 16.16 fixed point, one unit a texel, 64 units a tile of the map; the
// 64, 32, 16 or 8 texel version by the distance (1000, 2000, 3000).
void BalloonhuntScene::renderGround() {
	double half = (double)kFov * 0.5f;
	double a = half + _heading;
	float sinA = (float)sin(a), cosA = (float)cos(a);
	double b = (double)_heading - half;
	float sinB = (float)sin(b), cosB = (float)cos(b);
	float k = (float)-((double)_proj * _camY * _invCos);
	if (_tiles[0].empty())
		return;
	const uint16 *map = _map.data();
	uint16 *row = &_bb[kHorizon * kScreenW];
	for (int i = 0; 5 * i < 0x36b; i++, row += kScreenW) {
		double d = -((double)k / (double)(i + 1));
		if (d > kFarClamp)
			d = (double)kFarClamp - (double)(5 * i);
		double lx = d * sinA + _camX;
		float lz = (float)(d * cosA + _camZ);
		uint32 du = (uint32)ftol((d * sinB + _camX - lx) * kTexStep);
		uint32 dv = (uint32)ftol((d * cosB + _camZ - lz) * kTexStep);
		uint32 u = (uint32)ftol(lx * kFixed);
		uint32 v = (uint32)ftol((double)lz * kFixed);
		if (d < 1000.0f) {
			const uint16 *t = _tiles[0].data();
			for (int x = 0; x < kScreenW; x++) {
				uint tile = map[((v >> 12) & 0xffc00) + ((u >> 22) & 0x3ff)];
				row[x] = t[tile * 4096 + ((v >> 10) & 0xfc0) + ((u >> 16) & 0x3f)];
				u += du;
				v += dv;
			}
		} else if (d < 2000.0f) {
			const uint16 *t = _tiles[1].data();
			for (int x = 0; x < kScreenW; x++) {
				uint tile = map[((v >> 12) & 0xffc00) + ((u >> 22) & 0x3ff)];
				row[x] = t[tile * 1024 + ((v >> 12) & 0x3e0) + ((u >> 17) & 0x1f)];
				u += du;
				v += dv;
			}
		} else if (d < 3000.0f) {
			const uint16 *t = _tiles[2].data();
			for (int x = 0; x < kScreenW; x++) {
				uint tile = map[((v >> 12) & 0xffc00) + ((u >> 22) & 0x3ff)];
				row[x] = t[tile * 256 + ((v >> 14) & 0xf0) + ((u >> 18) & 0xf)];
				u += du;
				v += dv;
			}
		} else {
			const uint16 *t = _tiles[3].data();
			for (int x = 0; x < kScreenW; x++) {
				uint tile = map[((v >> 12) & 0xffc00) + ((u >> 22) & 0x3ff)];
				row[x] = t[tile * 64 + ((v >> 16) & 0x38) + ((u >> 19) & 0x7)];
				u += du;
				v += dv;
			}
		}
	}
}

// FUN_0042d1d0: the billboards of the 3 x 3 cells around the camera (fading
// out from 1300 to 2000 units) and the balloons, sorted by their depth and
// drawn from the back.
void BalloonhuntScene::renderSprites() {
	int cx = ftol((double)_camX * kCellScale), cz = ftol((double)_camZ * kCellScale);
	int x1 = CLIP(cx + 1, 0, 7), z1 = CLIP(cz + 1, 0, 7);
	int x0 = CLIP(cx - 1, 0, 7), z0 = CLIP(cz - 1, 0, 7);
	_drawList.clear();
	for (int z = z0; z <= z1; z++)
		for (int x = x0; x <= x1; x++)
			for (int idx : _cells[x + z * kCells])
				project(_sprites[idx], 1);
	// FUN_0042d5c0: the balloons, never faded
	if (_mode == kModePractice) {
		for (int i = 0; i < kFreeBalloons; i++) {
			FreeBalloon &b = _free[i];
			if (b.type == 4)
				continue;
			b.sprite.x = b.x;
			b.sprite.y = b.y;
			b.sprite.z = b.z;
			project(b.sprite, 0);
		}
	} else if (_mode == kModeNormal || _mode == kModeSingle) {
		for (int i = _next; i < _trackCount; i++)
			project(_track[i], 0);
	}
	// FUN_0042d4c0
	if (_drawList.size() > 1)
		sortSprites(0, (int)_drawList.size() - 1);
	for (int i = (int)_drawList.size() - 1; i >= 0; i--) {
		const Sprite3D &s = *_drawList[i];
		int level = 0;
		if (s.fade) {
			level = ftol(((double)s.depth - kFadeStart) * 16.0f / (double)(kFar - kFadeStart));
			if (level < 0)
				level = 0;
			else if (level >= 15)
				level = 15;
		}
		if (s.image)
			drawSprite(s.sx, s.sy, s.sw, s.sh, s.image->image, level);
	}
	_drawList.clear();
}

// FUN_0042d390: a billboard into the draw list, projected 320 / depth with
// the horizon at row 205; the faded ones only nearer than 2000.
void BalloonhuntScene::project(Sprite3D &s, int fade) {
	float dx = s.x - _camX, dy = s.y - _camY, dz = s.z - _camZ;
	double depth = (double)_cosH * dz + (double)_sinH * dx;
	s.fade = fade;
	if (!(0.0 < depth))
		return;
	if (!(depth < kFar) && fade)
		return;
	float sc = (float)((double)_proj / depth);
	double x = ((double)_cosR * dz + (double)_sinR * dx) * sc + 320.0f;
	double y = 205.0f - (double)dy * sc;
	float sw = (float)((double)sc * s.w), sh = (float)((double)sc * s.h);
	if (s.alignX == 1)
		x -= (double)sw * 0.5f;
	else if (s.alignX == 2)
		x -= sw;
	if (s.alignY == 1)
		y -= (double)sh * 0.5f;
	else if (s.alignY == 2)
		y -= sh;
	s.sx = (float)x;
	s.sy = (float)y;
	s.depth = (float)depth;
	s.sw = sw;
	s.sh = sh;
	_drawList.push_back(&s);
}

// FUN_0042d4e0: quicksort on the whole depth, nearest first.
void BalloonhuntScene::sortSprites(int lo, int hi) {
	for (;;) {
		int pivot = ftol(_drawList[(lo + hi) >> 1]->depth);
		int i = lo, j = hi;
		while (i <= j) {
			while (ftol(_drawList[i]->depth) < pivot)
				i++;
			while (pivot < ftol(_drawList[j]->depth))
				j--;
			if (i > j)
				break;
			SWAP(_drawList[i], _drawList[j]);
			i++;
			j--;
		}
		if (lo < j)
			sortSprites(lo, j);
		if (hi <= i)
			return;
		lo = i;
	}
}

// FUN_0042d650: a 128 x 128 billboard scaled into the rectangle starting
// at the next whole pixel, clipped to the view (rows 30..379), its colour
// key the first pixel. Faded ones (level 1..15) are drawn through a random
// mask that keeps about (15 - level) / 16 of the pixels; the mask is
// looked up with the texture's byte offset of the row (row x 256), so
// rows from 64 on use the next mask (sic), and past the masks (level 15)
// the original reads other data: drawn here.
void BalloonhuntScene::drawSprite(float x, float y, float w, float h, const Image16 &img, int level) {
	if (img.w != 128 || img.h != 128)
		return;
	int hI = ftol(h), wI = ftol(w);
	float du = 128.0f / w, dv = 128.0f / h;
	int32 duF = ftol((double)du * kFixed), dvF = ftol((double)dv * kFixed);
	double cxx = ceil((double)x), cyy = ceil((double)y);
	float fx = (float)(cxx - x), fy = (float)(cyy - y);
	int X = ftol(cxx), Y = ftol(cyy);
	if (X >= 0x27f || Y >= 0x17b || wI + X < 0 || Y + hI < kViewTop)
		return;
	double u0 = (double)fx * du;
	float v0 = (float)((double)fy * dv);
	if (Y < kViewTop) {
		int skip = kViewTop - Y;
		hI = Y + hI - kViewTop;
		v0 = (float)((double)skip * dv + v0);
		Y = kViewTop;
	}
	if (X < 0) {
		u0 = u0 - (double)X * du;
		wI = wI + X;
		X = 0;
	}
	if (X + wI > 0x27f)
		wI = kScreenW - X;
	if (Y + hI > 0x17b)
		hI = kViewBottom - Y;
	if (wI <= 0 || hI <= 0)
		return;
	uint32 uStart = (uint32)ftol(u0 * kFixed);
	uint32 v = (uint32)ftol((double)v0 * kFixed);
	const uint16 *pix = img.pix.data();
	const uint16 key = pix[0];
	const byte *mask = _dither.data() + level * 0x4000;
	const int maskLeft = (16 - level) * 0x4000;  // bytes of the masks from this one on
	uint16 *dst = &_bb[Y * kScreenW + X];
	for (int r = 0; r < hI; r++, dst += kScreenW, v += (uint32)dvF) {
		int rowOff = (int)((v >> 9) & 0x3f80);
		uint32 u = uStart;
		for (int c = 0; c < wI; c++, u += (uint32)duF) {
			int t = rowOff + (int)(u >> 16);
			if (t >= 128 * 128)
				continue;
			if (level) {
				int m = rowOff * 2 + (int)(u >> 16);
				if (m < maskLeft && !mask[m])
					continue;
			}
			if (pix[t] != key)
				dst[c] = pix[t];
		}
	}
}

// FUN_0042dbf0: the points (when changed) and the time: left in a points
// round, else since the track started, as minutes:seconds:hundredths.
void BalloonhuntScene::drawLabels() {
	if (_score != _scoreShown) {
		drawLabel("bh_score", Common::String::format("%d", _score));
		_scoreShown = _score;
	}
	int32 t;
	if (_mode == kModeNormal) {
		t = (_timeLimit + (int32)_startTime) - (int32)_vm->getGameMillis();
		if (t < 0)
			t = 0;
	} else {
		t = (int32)(_vm->getGameMillis() - _startTime);
	}
	drawLabel("bh_time", Common::String::format("%02d:%02d:%02d", t / 60000, (t / 1000) % 60, (t / 10) % 100));
}

// ---- after the scene is drawn (FUN_0042dcf0) ---------------------------------------

void BalloonhuntScene::afterDraw() {
	if (_mode == kModeMenu)
		return;
	// FUN_0042dec0: Solan, the interface bitmaps (the original's elements,
	// whose parts over the view it blits again after Solan), the panel
	drawSolan();
	bbBlit(_ifcBottom, 0, 336, kScreenW, kScreenH - 336);
	bbBlit(_ifcTop, 0, -1);
	drawOverlay();
	// the buffer becomes the backdrop
	bool fast = _view->format.bytesPerPixel == 4;
	for (int y = 0; y < kScreenH; y++) {
		const uint16 *src = &_bb[y * kScreenW];
		if (fast) {
			uint32 *dst = (uint32 *)_view->getBasePtr(0, y);
			for (int x = 0; x < kScreenW; x++)
				dst[x] = _lut[src[x]];
		} else {
			for (int x = 0; x < kScreenW; x++)
				_view->setPixel(x, y, _lut[src[x]]);
		}
	}
	updateCursor();
	if (_mode == kModeNormal) {
		// the fuel burns at 2 / s; the time runs out
		_gas = _gas - (_dt + _dt);
		if (_gas < 0.0f)
			_gas = 0.0f;
		if ((int32)(_vm->getGameMillis() - _startTime) >= _timeLimit) {
			playClone("gameover");
			endRound(2);
		}
	}
	if (_gas < kGasEmpty) {
		playClone("gameover");
		endRound(1);
	}
}

// The game's cursor: cursor.bmp at the mouse for a second after it moved,
// and while a button is held.
void BalloonhuntScene::updateCursor() {
	if (_mode == kModeMenu)
		return;
	Common::Point m = _vm->mousePos();
	uint32 now = _vm->getGameMillis();
	if (m.x != _mouseX || m.y != _mouseY) {
		_cursorOn = true;
		_cursorUntil = now + 1000;
		_mouseX = m.x;
		_mouseY = m.y;
	}
	if (_cursorOn && (int32)(now - _cursorUntil) > 0 && _pressed == 0)
		_cursorOn = false;
	if (_cursorOn) {
		if (!_cursorAnim->isAdded())
			_cursorAnim->add(_mouseX, _mouseY, 200);
		_cursorAnim->setPos(_mouseX, _mouseY);
	} else if (_cursorAnim->isAdded()) {
		_cursorAnim->remove();
	}
}

// FUN_0042ea50: Solan's picture by the bank (17 steps, the level ones and
// the steepest banks animated, 70 ms a frame) and the climb (steps of 0.1
// and 0.3), the top 235 rows of it at (192, 95), keyed.
void BalloonhuntScene::drawSolan() {
	int idx = ftol((double)_roll * 8.0f + 8.0f);
	int frame = 0;
	bool animated;
	if (idx < 0) {
		idx = 0;
		animated = true;
	} else if (idx > 17) {
		idx = 17;
		animated = false;
	} else {
		animated = idx == 0 || idx == 8 || idx == 16;
	}
	if (animated)
		frame = (int)((int32)_vm->getGameMillis() / 70) % 3;
	// group 17 would be the compass card's pointer in the original
	if (idx > 16)
		idx = 16;
	const Image16 *img;
	if (_pitch < -0.1f && idx == 8) {
		int k = ftol((double)_pitch * -2.0f);
		img = k < 2 ? &_down[k] : &_stayDown[frame];
	} else if (0.1f < _pitch && idx == 8) {
		int k = ftol((double)_pitch + _pitch);
		img = k < 2 ? &_up[k] : &_stayUp[frame];
	} else if (0.3f < _pitch && idx == 7) {
		img = &_upLeft[0];
	} else if (0.1f < _pitch && idx == 6) {
		img = &_upLeft[1];
	} else if (0.3f < _pitch && idx == 9) {
		img = &_upRight[0];
	} else if (0.1f < _pitch && idx == 10) {
		img = &_upRight[1];
	} else if (_pitch < -0.3f && idx == 7) {
		img = &_downLeft[0];
	} else if (_pitch < -0.1f && idx == 6) {
		img = &_downLeft[1];
	} else if (_pitch < -0.3f && idx == 9) {
		img = &_downRight[0];
	} else if (_pitch < -0.1f && idx == 10) {
		img = &_downRight[1];
	} else {
		img = &_solan[idx][frame];
	}
	if (img->pix.empty())
		return;
	int w = MIN(img->w, 0x100), h = MIN(img->h, 0xeb);
	for (int y = 0; y < h; y++) {
		const uint16 *src = &img->pix[y * img->w];
		uint16 *dst = &_bb[(0x5f + y) * kScreenW + 0xc0];
		for (int x = 0; x < w; x++)
			if (src[x] != img->key)
				dst[x] = src[x];
	}
}

// FUN_00404a70 with flag 4 (FUN_00401ca0 with the first w x h pixels).
void BalloonhuntScene::bbBlit(const Image16 &img, int x, int y, int w, int h) {
	if (w < 0 || w > img.w)
		w = img.w;
	if (h < 0 || h > img.h)
		h = img.h;
	for (int j = 0; j < h; j++) {
		if ((uint)(y + j) >= (uint)kScreenH)
			continue;
		const uint16 *src = &img.pix[j * img.w];
		for (int i = 0; i < w; i++)
			if (src[i] != img.key)
				bbPut(x + i, y + j, src[i]);
	}
}

// The instruments, drawn over the panel (FUN_0042ea50's gyroscope, then
// FUN_0042e040, the next balloon and the autopilot sign of FUN_0042dec0).
void BalloonhuntScene::drawOverlay() {
	// the gyroscope: 7 x 7 pictures by the climb and the bank
	int k = ftol((double)_pitch * 3.0f + 3.0f);
	int j = ftol((double)_roll * -3.0f + 3.0f);
	if (k >= 0 && k < 7 && j >= 0 && j < 7)
		bbBlit(_gyro[k * 7 + j], 0x18a, 0x17f);
	// FUN_0042e040: the radar (275, 376) and the compass (183, 384)
	drawRadar(0x113, 0x178, 0x5c, _heading, _zoom);
	drawCompass(0xb7, 0x180, 0x40, _heading);
	// the aircraft on the radar, two black triangles
	fillTriangle(_marker[0], _marker[1], _marker[2], 0);
	fillTriangle(_marker[0], _marker[1], _marker[3], 0);
	// (two triangles in red and black at the never set points 0x5c1520
	// follow; they cover nothing)
	// FUN_0042d8e0: the needles, speed and height in white, fuel in black;
	// each turns through 3/2 pi from -pi/4
	drawNeedle((float)(((double)_speed - kMinSpeed) * 0.0085679796f - kQuarterPi + kPi), 132.5f, 421.0f, 3.0f, 18.0f, 0xffff);
	drawNeedle((float)(((double)_y - kMinAlt) * 0.0042453953f - kQuarterPi), 508.5f, 421.0f, 3.0f, 18.0f, 0xffff);
	drawNeedle((float)((double)_gas * 0.047123890f - kQuarterPi), 49.5f, 425.0f, 2.5f, 16.0f, 0);
	// the next balloon (FUN_0042dec0)
	if ((_mode == kModeNormal || _mode == kModeSingle) && _next < _trackCount && _trackSmall[_next])
		bbBlit(*_trackSmall[_next], 0x23e, 0x19a);
	// the autopilot sign blinks (sin(t x 0.0094245) > 0)
	if (_autopilot && 0.0 < sin((double)(int32)_vm->getGameMillis() * 0.0094245f))
		bbBlit(_autopilotImg, 0x11a, 0x19d);
}

// FUN_0042d8e0: a needle, a triangle from the hub (width x 2) to the tip.
void BalloonhuntScene::drawNeedle(float angle, float cx, float cy, float width, float length, uint16 colour) {
	double b = (double)angle + kHalfPi;
	float bx = (float)(cos(b) * width), by = (float)(sin(b) * width);
	Point2 p0 = { bx + cx, (float)(by + (double)cy) };
	Point2 p1 = { cx - bx, cy - by };
	Point2 p2 = { (float)(cx - cos((double)angle) * length), (float)(cy - sin((double)angle) * length) };
	fillTriangle(p0, p1, p2, colour);
}

// FUN_0045af64 (the racing code's): a flat triangle; rows from
// trunc(y + 0.999), spans [trunc(left), trunc(right)) of the edges.
void BalloonhuntScene::fillTriangle(const Point2 &pa, const Point2 &pb, const Point2 &pc, uint16 colour) {
	const Point2 *a = &pa, *b = &pb, *c = &pc;
	if (c->y < b->y)
		SWAP(b, c);
	if (c->y < a->y)
		SWAP(a, c);
	if (b->y < a->y)
		SWAP(a, b);
	int ya = (int)(a->y + 0.999f), yb = (int)(b->y + 0.999f), yc = (int)(c->y + 0.999f);
	float fa = (float)ya - a->y, fb = (float)yb - b->y;
	if (ya == yb && ya == yc)
		return;
	float abx = b->x - a->x, aby = b->y - a->y;
	float acx = c->x - a->x, acy = c->y - a->y;
	float bcx = c->x - b->x, bcy = c->y - b->y;
	float cross = abx * acy - acx * aby;
	if (cross == 0.0f)
		return;
	bool hasAB = aby > 0.0f, hasAC = acy > 0.0f;
	float sAB = hasAB ? abx / aby : aby;
	float sAC = hasAC ? acx / acy : acy;
	float sBC = bcy > 0.0f ? bcx / bcy : bcy;
	bool shortRight = hasAB ? sAB > sAC : !(abx < 0.0f);
	auto span = [&](int y, float xl, float xr) {
		int l = (int)xl, r = (int)xr;
		for (int x = l; x < r; x++)
			bbPut(x, y, colour);
	};
	float xLong = fa * sAC + a->x;
	if (yb > ya) {
		float xShort = fa * sAB + a->x;
		for (int y = ya; y < yb; y++) {
			if (shortRight)
				span(y, xLong, xShort);
			else
				span(y, xShort, xLong);
			xLong += sAC;
			xShort += sAB;
		}
	}
	if (yc > yb) {
		float xShort = fb * sBC + b->x;
		for (int y = yb; y < yc; y++) {
			if (shortRight)
				span(y, xLong, xShort);
			else
				span(y, xShort, xLong);
			xLong += sAC;
			xShort += sBC;
		}
	}
}

// FUN_0042e2e0: the radar, map3.bmp (32 units a pixel) turned with the
// heading in a circle of 92 pixels, zoom = the distance to the corners
// in map pixels (35..250, Page Up / Page Down); in a track round the
// remaining balloons' numbers (numbers.bmp) on it, the next one on top.
void BalloonhuntScene::drawRadar(int px, int py, int size, float heading, float zoom) {
	double k = 65536.0 / size;
	if (size != _radarSize) {
		int r = size / 2;
		float r2 = (float)(r * r);
		for (int i = 0; i < size && i < 128; i++) {
			double t = (double)r2 - (double)((i - r) * (i - r));
			if (t <= 0.0) {
				_radarSpan[i] = 0;
				continue;
			}
			int w = ftol(sqrt(t) * 2.0);
			_radarSpan[i] = w;
			_radarLeft[i] = r - w / 2 + px;
			_radarRight[i] = w / 2 + r + px;
		}
		_radarSize = size;
	}
	if (255.0f < zoom)
		zoom = 255.0f;
	else if (zoom < 10.0f)
		zoom = 10.0f;
	double X = (double)_x * 0.03125f;
	float Z = (float)((double)_z * 0.03125f);
	double a = (double)heading - kQuarterPi;
	float ax = (float)(sin(a) * zoom + X), az = (float)(cos(a) * zoom + Z);
	a += kHalfPi;
	float bx = (float)(sin(a) * zoom + X), bz = (float)(cos(a) * zoom + Z);
	float a3 = (float)(a + kHalfPi);
	int32 du = ftol(((double)ax - bx) * k), dv = ftol(((double)az - bz) * k);
	int32 rdu = ftol((sin((double)a3) * zoom + X - bx) * k);
	int32 rdv = ftol((cos((double)a3) * zoom + Z - bz) * k);
	int32 u0 = ftol((double)bx * kFixed), v0 = ftol((double)bz * kFixed);
	for (int i = 0; i < size; i++) {
		int w = _radarSpan[i];
		if (w > 0) {
			int off = (size - w) / 2;
			int32 u = off * du + u0, v = off * dv + v0;
			for (int n = 0; n < w; n++) {
				bbPut(px + off + n, py + i, _radarMap[((v >> 6) & 0xffc00) + ((u >> 16) & 0x3ff)]);
				u += du;
				v += dv;
			}
		}
		u0 += rdu;
		v0 += rdv;
	}
	if (_mode != kModeNormal && _mode != kModeSingle)
		return;
	int r = size / 2;
	float rf = (float)r;
	float scale = (float)((double)rf / (sin(kQuarterPiD) * zoom));
	if (_numbers.pix.empty())
		return;
	for (int i = _trackCount - 1; i >= _next; i--) {
		const Sprite3D &b = _track[i];
		double bx2 = ((double)b.x - _x) * 0.03125f * scale;
		double bz2 = ((double)b.z - _z) * 0.03125f * scale;
		int fwd = ftol(bz2 * _cosH + bx2 * _sinH);
		int side = ftol(bz2 * _cosR + bx2 * _sinR + rf);
		int sy = r - fwd + py, sx = side + px;
		const uint16 *glyph = &_numbers.pix[i * 10];
		for (int row = sy - 5; row < sy + 5; row++, glyph += _numbers.w) {
			if (!(py < row && row < py + size))
				continue;
			int idx = row - py;
			for (int col = sx - 5, n = 0; col < sx + 5; col++, n++)
				if (_radarLeft[idx] < col && col < _radarRight[idx] && glyph[n] != _numbers.pix[0])
					bbPut(col, row, glyph[n]);
		}
	}
}

// FUN_0042e7b0: the compass card (64 x 64) turned with the heading in a
// circle of 64 pixels.
void BalloonhuntScene::drawCompass(int px, int py, int size, float heading) {
	double k = 65536.0 / size;
	int r = size / 2;
	float rf = (float)r;
	if (size != _compassSize) {
		float r2 = (float)(r * r);
		for (int i = 0; i < size && i < 128; i++) {
			double t = (double)r2 - (double)((i - r) * (i - r));
			if (t <= 0.0) {
				_compassSpan[i] = 0;
				continue;
			}
			int w = ftol(sqrt(t) * 2.0);
			_compassSpan[i] = w;
			_compassLeft[i] = r - w / 2 + px;
			_compassRight[i] = w / 2 + r + px;
		}
		_compassSize = size;
	}
	float R = (float)((double)rf / sin(kQuarterPiD));
	double a = (double)heading - kQuarterPi;
	float ax = (float)(sin(a) * R + rf), az = (float)(cos(a) * R + rf);
	a += kHalfPi;
	float bx = (float)(sin(a) * R + rf), bz = (float)(cos(a) * R + rf);
	float a3 = (float)(a + kHalfPi);
	int32 du = ftol(((double)ax - bx) * k), dv = ftol(((double)az - bz) * k);
	int32 rdu = ftol((sin((double)a3) * R + rf - bx) * k);
	int32 rdv = ftol((cos((double)a3) * R + rf - bz) * k);
	int32 u0 = ftol((double)bx * kFixed), v0 = ftol((double)bz * kFixed);
	const uint16 *tex = _compassTex.pix.data();
	for (int i = 0; i < size; i++) {
		int w = _compassSpan[i];
		if (w > 0) {
			int off = (size - w) / 2;
			int32 u = off * du + u0, v = off * dv + v0;
			for (int n = 0; n < w; n++) {
				// (u >> 16 is not masked in the original)
				int t = (((v >> 10) & 0xfc0) + (u >> 16)) & 0xfff;
				bbPut(px + off + n, py + i, tex[t]);
				u += du;
				v += dv;
			}
		}
		u0 += rdu;
		v0 += rdv;
	}
}

// ---- labels --------------------------------------------------------------------

// FUN_004078c0 + FUN_004079c0: a text area (key coloured) at z with a
// centred text (0x101).
void BalloonhuntScene::showLabel(const char *elem, int x, int y, int w, int h, int z, const Common::String &text) {
	Anim *a = defineSurfaceAnim(elem, w, h, _vm->_screen->format.RGBToColor(0, 255, 0));
	if (a->isAdded())
		a->remove();
	a->add(x, y, z);
	drawLabel(elem, text);
}

void BalloonhuntScene::drawLabel(const char *elem, const Common::String &text) {
	Anim *a = anim(elem);
	if (!a || !a->surface() || !a->isAdded())
		return;
	Graphics::ManagedSurface *s = a->surface();
	s->fillRect(Common::Rect(s->w, s->h), s->format.RGBToColor(0, 255, 0));
	if (text.empty())
		return;
	int w = _font.stringWidth(text);
	_font.drawString(*s, (s->w - w) / 2, (s->h - _font.height()) / 2, text);
}

// FUN_00407840
void BalloonhuntScene::removeLabel(const char *elem) {
	if (isAnimAdded(elem))
		removeAnim(elem);
}

// ---- sounds --------------------------------------------------------------------

// FUN_0042c420 -> FUN_00412c20: the original plays a copy of the blank " "
// element with the clip's name, so the same sound can overlap; here it
// starts again.
void BalloonhuntScene::playClone(const char *name) {
	Anim *a = anim(name);
	if (!a)
		return;
	a->remove();
	a->play();
	a->setRemoveWhenDone(true);
}

// FUN_00413a40(mix2, 1): the engine sound loops at the sound volume.
void BalloonhuntScene::playMix() {
	if (_mixData.empty())
		return;
	Audio::Mixer *mixer = g_system->getMixer();
	if (mixer->isSoundHandleActive(_mixHandle))
		return;
	Audio::SeekableAudioStream *wav = Audio::makeWAVStream(new Common::MemoryReadStream(_mixData.data(), _mixData.size()), DisposeAfterUse::YES);
	if (!wav)
		return;
	_mixRate = wav->getRate();
	_mixVolume = CLIP((int)(_master * 255.0f), 0, 255);
	mixer->playStream(Audio::Mixer::kSFXSoundType, &_mixHandle, Audio::makeLoopingAudioStream(wav, 0), -1, _mixVolume);
	if (_mixPitch != 1.0f)
		mixer->setChannelRate(_mixHandle, CLIP<int>((int)((float)_mixRate * _mixPitch), 100, 100000));
}

// FUN_00413f70
void BalloonhuntScene::stopMix() {
	g_system->getMixer()->stopHandle(_mixHandle);
}

// FUN_00414240
bool BalloonhuntScene::isMixPlaying() const {
	return g_system->getMixer()->isSoundHandleActive(_mixHandle);
}

// FUN_00414080(mix2, -85 x volume): a share of the sound volume (the
// DirectSound attenuation curve taken as linear).
void BalloonhuntScene::setMixVolume(float factor) {
	_mixVolume = CLIP((int)(_master * factor * 255.0f), 0, 255);
	if (isMixPlaying())
		g_system->getMixer()->setChannelVolume(_mixHandle, _mixVolume);
}

// FUN_004141c0: the frequency as a factor of the sample rate.
void BalloonhuntScene::setMixPitch(float factor) {
	_mixPitch = factor;
	if (isMixPlaying())
		g_system->getMixer()->setChannelRate(_mixHandle, CLIP<int>((int)((float)_mixRate * factor), 100, 100000));
}

} // End of namespace Flaaklypa
