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
#ifndef FLAAKLYPA_BOUQUET_H
#define FLAAKLYPA_BOUQUET_H

#include "flaaklypa/button.h"
#include "flaaklypa/font.h"
#include "flaaklypa/scene.h"

namespace Flaaklypa {

/**
 * "Ludvigs blomsterbinderi" (activity, started from the desk page): the
 * player picks flowers from the shelf and arranges up to 30 of them in one
 * of 18 vases; each of the 22 kinds has five places in the vase, one per
 * vertical zone over it. The bouquet is kept in the profile.
 */
class BouquetScene : public Scene {
public:
	BouquetScene(FlaaklypaEngine *vm, const SceneDef *def);

	void onInit(int arg) override;
	void onClose() override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onMouseUp(int hotspot, int x, int y) override;
	void onMouseMove(int hotspot, int x, int y) override;
	void onAnimFinished(Anim *anim) override;
	void onTimer(int id, int data) override;
	void onUpdate() override;

private:
	enum {
		kMaxFlowers = 30,
		kKinds = 22,
		kPlaces = 5,
		kVases = 18
	};

	struct Flower {
		bool used;
		int kind;
		int place;     ///< 0..4, -1 while held outside the vase (the small picture)
		int order;     ///< stacking order (FUN_0045e5c0)
		Common::String elem;
	};

	void showVase(int vase);
	Common::Point flowerPos(const Flower &f) const;
	int flowerZ(const Flower &f) const;
	void placeFlower(Flower &f, int place, int x, int y);
	Flower *newFlower(int kind, int place, int x, int y);
	void removeFlower(Flower &f);
	void toFront(Flower &f);
	int placeAt(int x, int y) const;
	void dragTo(int x, int y);
	Common::Array<Flower *> placedFlowers();
	void print();

	SceneButtons _buttons;
	BitmapFont _titleFont;
	Flower _flowers[kMaxFlowers];
	Flower *_held;        ///< DAT_006a1968: the flower being dragged
	int _nextOrder;       ///< DAT_0069f048
	int _vase;            ///< DAT_006a197c
	int _boat;            ///< DAT_006a0080: 0 sailboat, 1 raceboat next
	Common::Point _lastMouse;
};

} // End of namespace Flaaklypa

#endif
