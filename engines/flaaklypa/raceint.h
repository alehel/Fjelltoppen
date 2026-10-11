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
#ifndef FLAAKLYPA_RACEINT_H
#define FLAAKLYPA_RACEINT_H

#include "audio/mixer.h"
#include "common/array.h"
#include "common/ptr.h"
#include "common/str.h"

#include "flaaklypa/racing.h"

// The race's internals shared by racing.cpp, racetrack.cpp, racecar.cpp
// and raceai.cpp.

namespace Flaaklypa {

// ---- constants of the original ---------------------------------------------

static const float kPi = 3.14159274f;          // 0x49d620
static const float kHalfPi = 1.57079637f;      // 0x49da38
static const float kDegToRad = 0.0174532924f;  // 0x49f650
static const float kFourPi = 12.566371f;       // 0x4a0114

/** The terrain without a [terrain<n>] section (0x50aa20). */
static const RacingScene::Terrain kDefaultTerrain = { 1.0f, 1.0f, 1.0f, 0.0f };

enum {
	kRaceW = 640,
	kRaceH = 480,

	kStateRace = 0,
	kStateFinished = 1,     ///< a car crossed the line (the race goes on)
	kStateFinishClip = 2,   ///< a player crossed it: the finish clip plays
	kStateCountdown = 3,
	kStateWrecked = 4,
	kStateAborted = 5,

	// the sounds of a car (FUN_00473ad0)
	kSoundCrash = 0,
	kSoundCrashCar = 1,
	kSoundPickup = 2,
	kSoundSkid = 3,
	kSoundSpikeHit = 4,
	kSoundBrake = 5,
	kSoundTeleport = 6,
	kSoundTrick = 7,
	kSoundHorn = 8,
	kSoundEngine = 9,

	// the animations (0x50aa30)
	kAnimCrash = 0,
	kAnimDamage = 1,
	kAnimOil = 2,
	kAnimSmoke = 3,
	kAnimSpark = 4,
	kAnimSpikes = 5,

	// the pickups (pickup1..6)
	kPickupOil = 1,
	kPickupSpikes = 2,
	kPickupSmoke = 3,
	kPickupTurbo = 4,
	kPickupRepair = 5,
	kPickupCarPart = 6
};

// __ftol: the FPU truncates (chop mode); the low 32 bits of the result.
static inline int32 ftol(double v) {
	return (int32)(uint32)(uint64)(int64)v;
}

// The rounding the original writes as "x + (0 < x ? 0.5 : -0.5)" and __ftol.
static inline int32 rnd(double v) {
	return ftol(0.0 < v ? v + 0.5 : v - 0.5);
}

/**
 * The qsort of the original's C library (MSVC 6: insertion of the largest
 * for up to 8 elements, else quicksort with the middle element as pivot),
 * so equal elements end up where they did.
 */
template<typename T, typename Cmp>
void msvcQsort(T *base, uint num, Cmp cmp) {
	if (num < 2)
		return;
	auto shortsort = [&cmp](T *lo, T *hi) {
		while (hi > lo) {
			T *max = lo;
			for (T *p = lo + 1; p <= hi; p++)
				if (cmp(*p, *max) > 0)
					max = p;
			SWAP(*max, *hi);
			hi--;
		}
	};
	T *lostk[30], *histk[30];
	int stkptr = 0;
	T *lo = base, *hi = base + (num - 1);
	for (;;) {
		uint size = (uint)(hi - lo) + 1;
		if (size <= 8) {
			shortsort(lo, hi);
		} else {
			T *mid = lo + size / 2;
			SWAP(*mid, *lo);
			T *loguy = lo, *higuy = hi + 1;
			for (;;) {
				do {
					loguy++;
				} while (loguy <= hi && cmp(*loguy, *lo) <= 0);
				do {
					higuy--;
				} while (higuy > lo && cmp(*higuy, *lo) >= 0);
				if (higuy < loguy)
					break;
				SWAP(*loguy, *higuy);
			}
			SWAP(*lo, *higuy);
			if (higuy - 1 - lo >= hi - loguy) {
				if (lo + 1 < higuy) {
					lostk[stkptr] = lo;
					histk[stkptr] = higuy - 1;
					stkptr++;
				}
				if (loguy < hi) {
					lo = loguy;
					continue;
				}
			} else {
				if (loguy < hi) {
					lostk[stkptr] = loguy;
					histk[stkptr] = hi;
					stkptr++;
				}
				if (lo + 1 < higuy) {
					hi = higuy - 1;
					continue;
				}
			}
		}
		if (--stkptr < 0)
			return;
		lo = lostk[stkptr];
		hi = histk[stkptr];
	}
}

/**
 * A DirectSound buffer of the original (SOUND module, FUN_004139c0 ..
 * FUN_00414280): a WAV of the race played with its own volume, pan and
 * frequency; copies (FUN_00413ec0) share the data and play at the same
 * time.
 */
class RacingScene::Voice {
public:
	Voice();
	~Voice();

	Common::String name;
	Common::SharedPtr<Common::Array<byte> > data;

	bool isLoaded() const { return data && !data->empty(); }
	/** FUN_00413f20: Play; a playing buffer goes on (its loop flag changes). */
	void play(bool loop);
	/** FUN_00413f70 */
	void stop();
	/** FUN_00414240 */
	bool isPlaying() const;
	/**
	 * FUN_00414080(buffer, -percent): the volume as the original's percent
	 * (0..100), through its DirectSound curve (FUN_00414030).
	 */
	void setVolume(int percent);
	/** FUN_004140d0: -1 left .. 1 right. */
	void setPan(float pan);
	/** FUN_004141c0: the frequency as a factor of the sample's. */
	void setPitch(float factor);
	/** The mixer's sound for a pause under a dialog. */
	void pause(bool pause);

private:
	void apply();

	Audio::SoundHandle _handle;
	uint32 _rate;
	int _volume;    ///< mixer volume
	int8 _balance;
	float _pitch;
	bool _loop;
};

} // End of namespace Flaaklypa

#endif
