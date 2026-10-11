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
#include "common/textconsole.h"

#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/racing.h"
#include "flaaklypa/raceint.h"

namespace Flaaklypa {

/*
 * The cars (CAR module, FUN_00473410 .. FUN_0047a350): driving, the
 * engine, damage, the collision data and the collision response (COL
 * module, FUN_00470340 .. FUN_00471f50), the pickups and dirty tricks,
 * the animations on the track and the cars' sounds. The arithmetic is the
 * original's: floats stored where it stores them, the comparisons of its
 * FPU code.
 */

// FUN_00473810: (x, y) turned by -angle into (across, along) the car, or
// back with -angle.
static inline void rotate(float x, float y, float angle, float *ox, float *oy) {
	double c = cos((double)angle), s = sin((double)angle);
	if (ox)
		*ox = (float)(c * x - s * y);
	if (oy)
		*oy = (float)(c * y + s * x);
}

// FUN_00470ca0: the vector made unit, its square length at least 0.0001
static inline void unitVec(float *out, const float *v) {
	double l2 = (double)v[1] * v[1] + (double)v[0] * v[0];
	if (l2 <= (double)0.0001f)
		l2 = (double)0.0001f;
	double f = 1.0 / sqrt(l2);
	float x = (float)(f * v[0]), y = (float)(f * v[1]);
	out[0] = x;
	out[1] = y;
}

// FUN_00470070: made unit without a guard (a zero vector gives NaNs)
static inline void normalize(float *v) {
	float l = (float)sqrt((double)v[1] * v[1] + (double)v[0] * v[0]);
	v[0] = v[0] / l;
	v[1] = v[1] / l;
}

// FUN_00470cf0: the unit normal of a segment, (dy, -dx)
static inline void segNormal(float *out, float x1, float y1, float x2, float y2) {
	float n[2] = { y2 - y1, -(x2 - x1) };
	unitVec(out, n);
}

// FUN_00471c00: (p1, p2) projected on the direction of (x, y)
static inline void project(float *p1, float *p2, float x, float y) {
	float n[2] = { x, y };
	normalize(n);
	float d = n[1] * *p2 + n[0] * *p1;
	*p1 = d * n[0];
	*p2 = d * n[1];
}

// ---- the segment tests (COL module) --------------------------------------------

// FUN_00470a20: the point lies on the segment's extent (by x, or by y for
// a vertical one)
static bool onSegment(const float *s, const float *p) {
	float t, d = s[2] - s[0];
	if (d == 0.0f)
		t = (p[1] - s[1]) / (s[3] - s[1]);
	else
		t = (p[0] - s[0]) / d;
	return t <= 1.0f && t >= 0.0f;
}

// FUN_00470880: where two segments cross, through their slopes (a vertical
// segment's slope counts as 0, so a vertical and a horizontal one never
// cross, as in the original); out gets the point rounded.
static bool crossing(const float *a, const float *b, int32 *out) {
	float dxa = a[2] - a[0];
	float dxb = b[2] - b[0];
	float ma = dxa == 0.0f ? 0.0f : (a[3] - a[1]) / dxa;
	float mb = dxb == 0.0f ? 0.0f : (b[3] - b[1]) / dxb;
	if (ma == mb)
		return false;
	float ba = a[1] - ma * a[0];
	float bb = b[1] - mb * b[0];
	float p[2];
	if (dxa == 0.0f) {
		p[0] = a[0];
		p[1] = p[0] * mb + bb;
	} else {
		p[0] = dxb == 0.0f ? b[0] : (bb - ba) / (ma - mb);
		p[1] = p[0] * ma + ba;
	}
	if (!onSegment(a, p) || !onSegment(b, p))
		return false;
	if (out) {
		out[0] = rnd(p[0]);
		out[1] = rnd(p[1]);
	}
	return true;
}

static inline const float *segPts(const RacingScene::Segment &s) {
	return &s.x1;
}

// FUN_00470a80: the segment crosses an edge of the rectangle
static bool crossesRect(const float *s, const int *r) {
	const float l = (float)r[0], t = (float)r[1], rr = (float)r[2], b = (float)r[3];
	const float e0[4] = { l, t, rr, t };
	const float e1[4] = { rr, t, rr, b };
	const float e2[4] = { rr, b, l, b };
	const float e3[4] = { l, b, l, t };
	return crossing(s, e0, nullptr) || crossing(s, e1, nullptr) || crossing(s, e2, nullptr) || crossing(s, e3, nullptr);
}

// FUN_00470780
static bool rectsMeet(const int *a, const int *b) {
	return a[0] <= b[2] && a[1] <= b[3] && b[0] <= a[2] && b[1] <= a[3];
}

// ---- the cars' shapes --------------------------------------------------------------

// FUN_00470340: the corners of a w x h rectangle around the car, its middle
// a twelfth of the frame's width behind the position, turned by
// -(angle + pi)
void RacingScene::cornersOf(const Car *car, float w, float h, float pts[4][2]) const {
	float hw = w * 0.5f;
	float c = (float)car->sprites[car->sprite].w * 0.0833333358f;
	float local[8] = { -hw, -(h * 0.5f) - c, hw, -(h * 0.5f) - c, hw, h * 0.5f - c, -hw, h * 0.5f - c };
	double a = -((double)car->angle + (double)kPi);
	double ca = cos(a), sa = sin(a);
	for (int i = 0; i < 4; i++) {
		float x = local[i * 2], y = local[i * 2 + 1];
		float rx = (float)(x * ca - y * sa);
		float ry = (float)(y * ca + x * sa);
		pts[i][0] = rx + car->x;
		pts[i][1] = car->y + ry;
	}
}

// FUN_004707c0 / FUN_00470850: the four sides (the smaller rectangle of
// the wheels is 10 x 20 pixels less)
void RacingScene::sidesOf(Car *car, Segment segs[4], bool small) {
	float pts[4][2];
	if (small)
		cornersOf(car, car->scaledW - 10.0f, car->scaledH - 20.0f, pts);
	else
		cornersOf(car, car->scaledW, car->scaledH, pts);
	for (int i = 0; i < 4; i++) {
		Segment &s = segs[i];
		s.x1 = pts[i][0];
		s.y1 = pts[i][1];
		s.x2 = pts[(i + 1) & 3][0];
		s.y2 = pts[(i + 1) & 3][1];
		s.type = -3;
		s.id = car->id;
		s.car = car;
	}
}

// FUN_00470490: the rounded rectangle around the corners
void RacingScene::carBox(const Car *car, int r[4]) {
	float pts[4][2];
	cornersOf(car, car->scaledW, car->scaledH, pts);
	float l = pts[0][0], t = pts[0][1], rr = pts[0][0], b = pts[0][1];
	for (int i = 1; i < 4; i++) {
		if (!(l <= pts[i][0]))
			l = pts[i][0];
		if (!(t <= pts[i][1]))
			t = pts[i][1];
		if (rr < pts[i][0])
			rr = pts[i][0];
		if (b < pts[i][1])
			b = pts[i][1];
	}
	r[0] = rnd(l);
	r[1] = rnd(t);
	r[2] = rnd(rr);
	r[3] = rnd(b);
}

// FUN_00471f50: no side of the car crosses a wall or another car
bool RacingScene::canBeAt(Car *car) {
	Segment sides[4];
	sidesOf(car, sides, false);
	for (const auto &s : _segs) {
		if (s.type >= 0 || s.id == car->id)
			continue;
		for (int i = 0; i < 4; i++)
			if (crossing(segPts(sides[i]), segPts(s), nullptr))
				return false;
	}
	return true;
}

// FUN_004716d0: the car's sides into the collision data, after the walls
// (the counter starts again each frame, FUN_00470c10)
void RacingScene::addCarSides(Car *car) {
	Segment sides[4];
	sidesOf(car, sides, false);
	for (int i = 0; i < 4; i++) {
		uint idx = _staticSegs + _carSegs;
		if (idx < _segs.size())
			_segs[idx] = sides[i];
		_carSegs++;
	}
}

// ---- the collision response (FUN_00471740) -------------------------------------

namespace {

/** The quad a side of the car sweeps (FUN_00471030, 0xd8 bytes; the parts used). */
struct Swept {
	float p[4][2];
	float origin[2];      ///< [0x20]
	float e2[2];          ///< [0x14] p3 - p0
	float negE2[2];       ///< [0x18]
	float negE1[2];       ///< [0x1a]
	float n2[2];          ///< [0x1c] the normal of e2
	float n1[2];          ///< [0x1e] the normal of e1
};

/** FUN_004712b0's result: the point in the quad's frame, the plane it was clipped by. */
struct UV {
	float u, v;
	int idx;
};

struct Result {
	int32 wall;           ///< local_45c
	int32 trigger;        ///< local_458
	float frac;           ///< local_454
	float point[2];       ///< local_450
	float dir[2];         ///< local_448
	const RacingScene::Segment *seg; ///< local_440
	int32 triggerType;    ///< local_43c
};

void makeSwept(Swept &q) {
	float e1[2] = { q.p[1][0] - q.p[0][0], q.p[1][1] - q.p[0][1] };
	q.e2[0] = q.p[3][0] - q.p[0][0];
	q.e2[1] = q.p[3][1] - q.p[0][1];
	q.origin[0] = q.p[0][0];
	q.origin[1] = q.p[0][1];
	segNormal(q.n1, 0.0f, 0.0f, e1[0], e1[1]);
	segNormal(q.n2, 0.0f, 0.0f, q.e2[0], q.e2[1]);
	q.negE2[0] = -q.e2[0];
	q.negE2[1] = -q.e2[1];
	q.negE1[0] = -e1[0];
	q.negE1[1] = -e1[1];
}

// FUN_004712b0
void toUV(UV &out, const float *p, const Swept &q) {
	float d[2] = { p[0] - q.origin[0], p[1] - q.origin[1] };
	float d1[2] = { d[0] + q.negE1[0], d[1] + q.negE1[1] };
	double a = (double)q.n2[0] * d[0] + (double)q.n2[1] * d[1];
	double b = (double)q.n2[0] * d1[0] + (double)q.n2[1] * d1[1];
	float fa = (float)a;
	out.v = (float)((double)-fa / (b - (double)fa));
	float d2[2] = { d[0] + q.negE2[0], d[1] + q.negE2[1] };
	a = (double)q.n1[0] * d[0] + (double)q.n1[1] * d[1];
	b = (double)q.n1[0] * d2[0] + (double)q.n1[1] * d2[1];
	fa = (float)a;
	out.u = (float)((double)-fa / (b - (double)fa));
}

// FUN_00471440: the segment is not entirely beyond one side of the unit square
bool meetsUnit(const UV &a, const UV &b) {
	if (a.u < 0.0f && b.u < 0.0f)
		return false;
	if (1.0f < a.u && 1.0f < b.u)
		return false;
	if (a.v < 0.0f && b.v < 0.0f)
		return false;
	if (1.0f < a.v && 1.0f < b.v)
		return false;
	return true;
}

// FUN_00471360: a's end clipped to the unit square (planes 0x50a770: u >=
// 0, v <= 1, u <= 1, v >= 0) along a-b, the world points with it
void clipUV(UV &a, const UV &b, float *pa, const float *pb) {
	static const float kPlanes[4][3] = { { 1, 0, 0 }, { 0, -1, -1 }, { -1, 0, -1 }, { 0, 1, 0 } };
	for (int i = 0; i < 4; i++) {
		const float *pl = kPlanes[i];
		float da = (float)((double)a.u * pl[0] + (double)a.v * pl[1] - pl[2]);
		if (!(da <= 0.0f))
			continue;
		double db = (double)b.u * pl[0] + (double)b.v * pl[1] - pl[2];
		if (db <= 0.0)
			return;
		double t = -((double)da / (db - (double)da));
		a.u = (float)(((double)b.u - a.u) * t + a.u);
		a.v = (float)(((double)b.v - a.v) * t + a.v);
		pa[0] = (float)(((double)pb[0] - pa[0]) * t + pa[0]);
		pa[1] = (float)(((double)pb[1] - pa[1]) * t + pa[1]);
		a.idx = i;
	}
}

// FUN_004714e0: a segment against a swept quad; the hit nearest along the
// motion (v) wins
void testSegment(Result &res, const RacingScene::Segment &s, const Swept &q) {
	if (s.type == 0)
		return;
	float p1[2] = { s.x1, s.y1 }, p2[2] = { s.x2, s.y2 };
	UV a, b;
	toUV(a, p1, q);
	toUV(b, p2, q);
	a.idx = b.idx = -1;
	if (!meetsUnit(a, b))
		return;
	clipUV(a, b, p1, p2);
	clipUV(b, a, p2, p1);
	if (!meetsUnit(a, b))
		return;
	const UV *pick = &a;
	const float *pp = p1;
	if (b.v <= a.v) {
		pick = &b;
		pp = p2;
	}
	if (!(pick->v < res.frac))
		return;
	if (s.type < 0) {
		res.seg = &s;
		res.frac = pick->v;
		res.point[0] = pp[0];
		res.point[1] = pp[1];
		res.wall = 1;
	} else {
		res.trigger = 1;
		res.triggerType = s.type;
	}
	if (pick->idx == -1) {
		res.dir[0] = q.e2[0];
		res.dir[1] = q.e2[1];
	} else {
		res.dir[0] = s.x1 - s.x2;
		res.dir[1] = s.y1 - s.y2;
	}
	unitVec(res.dir, res.dir);
}

} // End of anonymous namespace

// FUN_00471740: the car's motion of this frame against the walls, the
// triggers and the other cars: the sides facing the motion sweep quads;
// the nearest wall hit moves the car up to 80 % of the way there (all
// of it below 25 pixels) and bounces it; up to four rounds. Returns true
// when no wall was hit.
bool RacingScene::collide(Car *car) {
	car->hitCar = 0;
	car->hitWall = 0;
	car->trigger = 0;
	for (int iter = 0;; iter++) {
		Result res;
		res.wall = 0;
		res.trigger = 0;
		res.frac = 3.40282347e+38f;
		res.point[0] = res.point[1] = 0.0f;
		res.dir[0] = res.dir[1] = 0.0f;
		res.seg = nullptr;
		res.triggerType = 0;
		float m[2];
		m[0] = (float)(((double)car->vx + (double)car->sx) * (double)_dt);
		m[1] = (float)(((double)car->vy + (double)car->sy) * (double)_dt);
		float dir[2];
		unitVec(dir, m);
		if (dir[0] == 0.0f && dir[1] == 0.0f)
			return true;
		Segment sides[4];
		sidesOf(car, sides, false);
		Swept quads[4];
		int n = 0;
		for (int i = 0; i < 4; i++) {
			float nrm[2];
			segNormal(nrm, sides[i].x1, sides[i].y1, sides[i].x2, sides[i].y2);
			double dot = (double)dir[0] * nrm[0] + (double)dir[1] * nrm[1];
			if (!((double)0.001f < dot))
				continue;
			Swept &q = quads[n++];
			q.p[0][0] = sides[i].x1;
			q.p[0][1] = sides[i].y1;
			q.p[1][0] = sides[i].x1 + m[0];
			q.p[1][1] = sides[i].y1 + m[1];
			q.p[2][0] = sides[i].x2 + m[0];
			q.p[2][1] = sides[i].y2 + m[1];
			q.p[3][0] = sides[i].x2;
			q.p[3][1] = sides[i].y2;
			makeSwept(q);
		}
		// FUN_00471190 / FUN_00471260: the segments near the quads
		float minX = 0, minY = 0, maxX = 0, maxY = 0;
		if (n > 0) {
			minX = maxX = quads[0].p[0][0];
			minY = maxY = quads[0].p[0][1];
		}
		for (int i = 0; i < n; i++)
			for (int k = 0; k < 4; k++) {
				float x = quads[i].p[k][0], y = quads[i].p[k][1];
				if (x < minX)
					minX = x;
				if (maxX < x)
					maxX = x;
				if (y < minY)
					minY = y;
				if (maxY < y)
					maxY = y;
			}
		_cands.clear();
		for (const auto &s : _segs) {
			if (s.id == car->id)
				continue;
			if (!(s.x1 >= minX) && !(s.x2 >= minX))
				continue;
			if (!(s.y1 >= minY) && !(s.y2 >= minY))
				continue;
			if (!(-s.x1 >= -maxX) && !(-s.x2 >= -maxX))
				continue;
			if (!(-s.y1 >= -maxY) && !(-s.y2 >= -maxY))
				continue;
			_cands.push_back(&s);
		}
		if (!_cands.empty()) {
			for (int i = 0; i < n; i++)
				for (const auto *s : _cands)
					testSegment(res, *s, quads[i]);
			if (res.trigger) {
				car->trigger = 1;
				car->hitType = res.triggerType;
			}
			if (!res.wall)
				break;
			if (!car->hitWall) {
				car->hitX = res.point[0];
				car->hitY = res.point[1];
				car->hitType = res.seg->type;
				car->hitWall = 1;
			}
			float frac = res.frac;
			if (625.0f <= m[0] * m[0] + m[1] * m[1]) {
				frac = frac * 0.8f;
				if (1.0f < frac)
					break;
			} else {
				frac = 0.0f;
			}
			car->x = m[0] * frac + car->x;
			car->y = m[1] * frac + car->y;
			switch (res.seg->type) {
			case -3:
				car->hitCar = 1;
				carBounce(car, res.seg->car, res.dir);
				break;
			case -2:
				wallBounce(car, res.dir, 0.000125f);
				break;
			case -1:
				wallBounce(car, res.dir, 0.00025f);
				break;
			default:
				car->vx = car->vy = 0.0f;
				car->sx = car->sy = 0.0f;
				break;
			}
		}
		if (!res.wall || iter > 2)
			break;
	}
	// FUN_004732c0: what is left of slow motion goes
	auto tiny = [](float &v, double lim) {
		if (v != v || fabs((double)v) < lim)
			v = 0.0f;
	};
	tiny(car->acc, 1.0);
	tiny(car->vx, 1.0);
	tiny(car->vy, 1.0);
	tiny(car->spin, (double)0.01f);
	tiny(car->sx, 1.0);
	tiny(car->sy, 1.0);
	return car->hitWall == 0;
}

// FUN_00471b10: a wall: damage by the speed, the mass and how square the
// hit is; the motion follows the wall
void RacingScene::wallBounce(Car *car, const float dir[2], float factor) {
	float v[2] = { car->vx + car->sx, car->sy + car->vy };
	float a[2], b[2];
	unitVec(a, v);
	unitVec(b, dir);
	double c = (double)b[0] * a[0] + (double)b[1] * a[1];
	double f = 1.0 - fabs(c);
	double dmg = fabs((double)car->speed) * (double)car->mass * f * (double)factor;
	if ((double)0.0025f < dmg)
		addDamage(car, (float)dmg);
	project(&car->vx, &car->vy, dir[0] * 0.05f, dir[1] * 0.05f);
	project(&car->sx, &car->sy, dir[0] * 0.05f, dir[1] * 0.05f);
}

// FUN_00471ee0: the car's grip velocity is its heading projected on the
// new motion, the rest slides
static void setMotion(RacingScene::Car *c, float x, float y) {
	c->vx = (float)sin((double)c->angle);
	c->vy = (float)cos((double)c->angle);
	project(&c->vx, &c->vy, x, y);
	c->sx = x - c->vx;
	c->sy = y - c->vy;
}

// FUN_00471cb0: two cars: the motion across the hit direction is shared
// by the masses (times 350) with 5 % of the difference kept; along it a's
// part is limited by b's; both take damage by the difference.
void RacingScene::carBounce(Car *a, Car *b, const float dir[2]) {
	if (!b)
		return;
	const double dx = dir[0], dy = dir[1];
	const float ndx = -dir[0];
	double A1 = (double)a->sx + a->vx, A2 = (double)a->sy + a->vy;
	float B1 = (float)((double)b->sx + b->vx), B2 = (float)((double)b->sy + b->vy);
	float aAlong = (float)(A1 * dx + A2 * dy);
	float bAlong = (float)((double)B1 * dx + (double)B2 * dy);
	float aPerp = (float)((double)ndx * A2 + dy * A1);
	float bPerp = (float)((double)ndx * B2 + dy * B1);
	double ma = (double)a->mass * 350.0f, mb = (double)b->mass * 350.0f;
	double pa = ma * aPerp, pb = mb * bPerp;
	float P = (float)(pa + pb);
	float newA = (float)(((pa - pb) * (double)0.05f + (double)P) / (ma + ma));
	float newB = (float)(((double)P - ma * newA) / mb);
	float ax = (float)((double)aAlong * dx + (double)newA * dy);
	float ay = (float)((double)newA * ndx + (double)aAlong * dy);
	float bx = (float)((double)bAlong * dx + (double)newB * dy);
	float by = (float)((double)newB * ndx + (double)bAlong * dy);
	if (0.0f <= ax) {
		if (!(ax < bx))
			ax = bx;
	} else if (ax <= bx) {
		ax = bx;
	}
	if (0.0f <= ay) {
		if (!(ay < by))
			ay = by;
	} else if (ay <= by) {
		ay = by;
	}
	setMotion(a, ax, ay);
	setMotion(b, bx, by);
	float dmg = (float)(((double)aPerp - bPerp) * (double)0.000125f);
	addDamage(a, (float)((double)(b->mass / a->mass) * dmg));
	addDamage(b, (float)((double)(a->mass / b->mass) * dmg));
}

// FUN_0047a250: the car moves by its velocity; a place where it would
// cross a wall or a car is refused
bool RacingScene::move(Car *car) {
	float fx = (float)(((double)car->sx + car->vx) * (double)_dt);
	float fy = (float)(((double)car->sy + car->vy) * (double)_dt);
	car->x = fx + car->x;
	car->y = fy + car->y;
	if (canBeAt(car))
		return true;
	car->x = car->x - fx;
	car->y = car->y - fy;
	return false;
}

// ---- driving (FUN_00473410 .. FUN_004734b0) ---------------------------------------

static inline float tractionOf(const RacingScene::Car *c) {
	float g = c->terrain->friction * c->grip;
	if (1.0f <= g)
		g = 1.0f;
	return g;
}

// FUN_00473410: the accelerator (in the finish clip everyone coasts)
void RacingScene::accelerate(Car *car) {
	if (_state == kStateFinishClip) {
		coast(car);
		return;
	}
	car->reversing = 0;
	car->acc = car->speed < 0.0f ? car->braking : car->curAccel;
	stopSound(car, kSoundBrake);
	car->acc = tractionOf(car) * car->acc;
	integrate(car);
}

// FUN_00473860: the brake, backwards when standing
void RacingScene::brake(Car *car) {
	if (car->finished && !car->ai) {
		coast(car);
		return;
	}
	if (0.0f < car->speed) {
		if (!car->ai)
			playSound(car, kSoundBrake);
		car->acc = -car->braking;
	} else {
		stopSound(car, kSoundBrake);
		car->acc = car->revAccel;
		car->reversing = 1;
	}
	car->acc = tractionOf(car) * car->acc;
	integrate(car);
}

// FUN_00473920: rolling out, 500 x the grip a second
void RacingScene::coast(Car *car) {
	float f = car->terrain->friction * car->grip * 500.0f;
	if (car->speed <= 0.0f) {
		if (car->speed < 0.0f) {
			float a = -car->speed;
			if (f <= -car->speed)
				a = f;
			car->acc = a;
		} else {
			car->acc = 0.0f;
		}
	} else if (car->speed < f) {
		car->acc = -car->speed;
	} else {
		car->acc = -f;
	}
	stopSound(car, kSoundBrake);
	integrate(car);
}

// FUN_004734b0: the acceleration along the car; the grip takes 1000 x the
// grip a second off the motion across it (and off the sliding motion, 500
// along it); the speed along the car is held to what the top speed leaves
// besides the motion across.
void RacingScene::integrate(Car *car) {
	float lat, fwd, slat, sfwd;
	rotate(car->vx, car->vy, car->angle, &lat, &fwd);
	rotate(car->sx, car->sy, car->angle, &slat, &sfwd);
	fwd = _dt * car->acc + fwd;
	float g = car->terrain->friction * car->grip;
	if (g == 0.0f) {
		car->skidding = 0;
	} else if (car->wheelspin != 0.0f || g * 125.0f < fabsf(slat + lat)) {
		car->skidding = 1;
	} else {
		car->skidding = 0;
	}
	auto toZero = [this, g](float v, float rate) {
		float step = _dt * g * rate;
		if (v < 0.0f) {
			v = step + v;
			return v < 0.0f ? v : 0.0f;
		}
		if (0.0f < v) {
			v = v - step;
			return 0.0f < v ? v : 0.0f;
		}
		return v;
	};
	lat = toZero(lat, 1000.0f);
	slat = toZero(slat, 1000.0f);
	sfwd = toZero(sfwd, 500.0f);
	if (0.0f < fwd) {
		float lim = (float)sqrt((double)car->curTop * car->curTop - (double)lat * lat);
		if (lim != lim)
			fwd = 0.0f;
		else if (!(fwd < lim))
			fwd = lim;
	} else if (fwd < 0.0f) {
		float lim = (float)-sqrt((double)car->revTop * car->revTop - (double)lat * lat);
		if (lim != lim)
			fwd = 0.0f;
		else if (!(lim < fwd))
			fwd = lim;
	}
	rotate(lat, fwd, -car->angle, &car->vx, &car->vy);
	rotate(slat, sfwd, -car->angle, &car->sx, &car->sy);
}

// FUN_00479420: the wheel turns the car at its turning rate plus its spin,
// unless that makes it cross something; the spin dies away with the grip
void RacingScene::steer(Car *car, int dir) {
	float f = car->terrain->friction * car->grip * _dt * kFourPi;
	float d = ((float)dir * car->turning + car->spin) * _dt;
	car->couldTurn = car->canTurn;
	car->forward = car->reversing == 0 ? 1 : 0;
	car->angle = d + car->angle;
	if (!canBeAt(car)) {
		car->canTurn = 0;
		car->angle = car->angle - d;
	} else {
		car->canTurn = 1;
	}
	if (0.0f <= car->spin) {
		if (car->spin <= 0.0f)
			return;
		float s = car->spin - f;
		car->spin = s <= 0.0f ? 0.0f : s;
	} else {
		float s = f + car->spin;
		car->spin = 0.0f <= s ? 0.0f : s;
	}
}

// FUN_004792f0: the speed along the car, the wheelspin and the engine
void RacingScene::updateSpeed(Car *car) {
	rotate(car->vx, car->vy, car->angle, nullptr, &car->speed);
	updateWheelspin(car, false);
	updateEngine(car);
}

// FUN_00478740: at the start the engine's revs ahead of the speed spin
// the wheels; the spin dies away at 0.5 a second
void RacingScene::updateWheelspin(Car *car, bool start) {
	if (start) {
		int gears = car->turboOn == 0.0f ? car->gears : car->gears + 1;
		float top = car->reversing == 0 ? car->curTop : car->revTop;
		float w = (car->rpm / (float)gears - car->speed / top) * 1.5f;
		float m = w;
		if (w < car->wheelspin)
			m = car->wheelspin;
		float v = 0.0f;
		if (0.0f < m) {
			v = w;
			if (w < car->wheelspin)
				v = car->wheelspin;
		}
		car->wheelspin = v;
	}
	if (car->wheelspin != 0.0f) {
		float v = car->wheelspin - _dt * 0.5f;
		car->wheelspin = v <= 0.0f ? 0.0f : v;
	}
}

// FUN_00479330: the revs follow the accelerator, in the car's gears (one
// more with the turbo), limited by the speed
void RacingScene::updateEngine(Car *car) {
	if (car->reversing) {
		bool up = !(0.0f <= car->acc && car->speed != car->revTop);
		rev(car, up, 1, true, -car->revTop);
		return;
	}
	int gears = car->gears + (car->turboOn != 0.0f ? 1 : 0);
	bool up = !(car->acc <= 0.0f && car->speed != car->curTop);
	rev(car, up, gears, true, car->curTop);
}

// FUN_00479080: the revs rise or fall by half (1 - gear / gears) a second;
// limited, they follow the speed; the engine sound's pitch follows them
void RacingScene::rev(Car *car, bool up, int gears, bool limit, float top) {
	float g = (float)gears;
	float gearFrac = (float)(floor((double)car->rpm) / (double)g);
	float step = (1.0f - gearFrac) * 0.5f * _dt;
	float r;
	if (up) {
		r = step + car->rpm;
		float lim = g - 0.01f;
		if (!(r < lim))
			r = lim;
	} else {
		r = car->rpm - step;
		if (!(0.0f < r))
			r = 0.0f;
	}
	car->rpm = r;
	if (limit) {
		float m = (float)((double)fabsf(car->speed) * (double)g / (double)top);
		if (car->rpm < m)
			m = car->rpm;
		car->rpm = m;
	}
	float pitch = enginePitch(car->rpm, gears, gearFrac, up ? car->rpm < (float)(gears - 1) : false);
	if (car->engine)
		car->engine->setPitch(pitch);
}

// FUN_004791d0: the pitch in a gear: up from 0.5 + the gear's share (at
// most 0.8) to 1.5, 1.75, then down to 1.5 and 0.8 over the shares of the
// gear's revs; past the second share it holds 1.75 unless still rising.
float RacingScene::enginePitch(float rpm, int gears, float gearFrac, bool rising) {
	float t[5], p[9];
	float a = gearFrac * 0.6f;
	t[0] = a + 0.4f;
	t[2] = t[3] = (1.0f - a) * 0.05f;
	t[1] = 1.0f - t[0] - t[2] - t[3];
	float share = (float)(floor((double)rpm) / (double)(float)gears);
	t[4] = (share < 0.8f ? share : 0.8f) + 0.5f;
	p[4] = t[4];
	p[5] = 1.5f;
	p[6] = 1.75f;
	p[7] = 1.5f;
	p[8] = 0.8f;
	double x = fmod((double)rpm, 1.0);
	int i = 0;
	while (i < 5 && !(x < t[i])) {
		x -= t[i];
		i++;
	}
	if (i >= 2 && !rising)
		return 1.75f;
	if (i > 3)
		i = 3; // (x < 1 always ends the loop by the fifth share)
	return (float)(((double)p[i + 5] - p[i + 4]) * (x / t[i]) + p[i + 4]);
}

// FUN_00473a00: damage times the shield, 0..1; the frame of the car is the
// damage's quarter (up to 3), its performance 1 - damage / 2
void RacingScene::addDamage(Car *car, float amount) {
	float d = amount * car->shield + car->damage;
	if (d < 0.0f)
		d = 0.0f;
	else if (1.0f < d)
		d = 1.0f;
	car->damage = d;
	int f = ftol(d * 4.0f);
	if (f >= 3)
		f = 3;
	car->sprite = f;
	car->perf = 1.0f - d * 0.5f;
}

// FUN_00473a90: more than 10 % damage and not on the last lap
bool RacingScene::wantsPit(Car *car) const {
	return (double)car->damage > 0.1 && car->lap < _settings.laps;
}

// FUN_00476620: a car on the start line
void RacingScene::resetCar(Car *car) {
	for (int i = 0; i < 9; i++)
		car->lapTimes[i] = 0;
	car->revAccel = car->accel * -0.5f;
	car->revTop = car->topSpeed * -0.5f;
	car->curAccel = car->accel;
	car->curTop = car->topSpeed;
	car->raceTime = 0;
	car->progress = 0.0f;
	car->lap = 1;
	car->timedLap = 1;
	car->bestLap = 0x7fffffff;
	car->pathSeq = 0;
	car->damage = 0.0f;
	car->perf = 1.0f;
	car->turbo = 1.0f;
	car->catchUp = 1.0f;
	car->rpm = 0.0f;
	car->terrain = &kDefaultTerrain;
	car->wheelspin = 0.0f;
	car->spin = 0.0f;
	car->sx = car->sy = 0.0f;
	car->acc = 0.0f;
	car->speed = 0.0f;
	car->vx = car->vy = 0.0f;
	car->finished = 0;
	car->steer = 2;
	car->forward = 1;
	car->canTurn = 1;
	car->couldTurn = 1;
	car->backTimer = 0.0f;
	addDamage(car, -1.0f);
	showSlot(car, 0);
	wheelPoints(car, true);
}

// FUN_0047a0b0: the terrain under the car (terrain.bmp, a quarter of the
// track: indices of 16 types, more is 0); a repairing one (the pit lane)
// repairs a tenth of its rate a second and plays the pit stop clip
void RacingScene::checkTerrain(Car *car) {
	int w4 = _track.w / 4, h4 = _track.h / 4;
	int tx = rnd(car->x * 0.25f), ty = rnd(car->y * 0.25f);
	if (tx < 0)
		tx = 0;
	else if (w4 < tx)
		tx = w4;
	if (ty < 0)
		ty = 0;
	else if (h4 < ty)
		ty = h4;
	uint idx = (uint)(ty * w4 + tx);
	byte t = idx < _track.terrain.size() ? _track.terrain[idx] : 0;
	if (t >= 16)
		t = 0;
	car->terrain = &_track.terrains[t];
	View *v = viewOf(car);
	if (car->terrain->repair != 0.0f) {
		if (v)
			playClip(*v, "pitstop", true);
		addDamage(car, -(_dt * car->terrain->repair * 0.1f));
		return;
	}
	if (v && clipIs(*v, "pitstop"))
		stopClip(*v);
}

// ---- the drivers ----------------------------------------------------------------------

// FUN_00478bf0: a driver's frame
void RacingScene::driverUpdate(Driver &d) {
	Car *car = d.car;
	if (_state < kStateFinishClip && !car->finished)
		car->raceTime = _vm->getGameMillis() - _raceStart;
	if (car->ai && (car->trickHeld == kPickupTurbo || 1 < _settings.carCount))
		useTrick(car);
	moveAndTrigger(d);
	if (_state == kStateCountdown) {
		if (car->ai) {
			// revving on the start line
			rev(car, car->rpm < d.revTarget, 1, false, 0.0f);
			if (car->rpm == d.revTarget) {
				if (0.5f < d.revTarget)
					d.revTarget = (float)randMod(25) * 0.01f;
				else
					d.revTarget = (float)randMod(25) * 0.01f + 0.75f;
			}
		} else {
			playerInput(d);
		}
		return;
	}
	if (0.25f < car->damage) {
		car->smokeTimer = car->smokeTimer - _dt;
		if (car->smokeTimer < 0.0f) {
			int w = ftol(car->scaledW);
			int ox = randRange(-w, w) / 2 + ftol(car->x);
			int w2 = ftol(car->scaledW);
			int oy = randRange(-w2, w2) / 2 + ftol(car->y);
			startAnim(ox, oy, kAnimDamage, car->id);
			car->smokeTimer = (1.0f - car->damage) * _animTypes[kAnimDamage].interval;
		}
	}
	if (_state <= kStateFinishClip) {
		aiDrive(d);
		if (!car->ai)
			playerInput(d);
	}
	switch (car->steer) {
	case 0:
		steer(car, 1);
		break;
	case 1:
		steer(car, -1);
		break;
	default:
		steer(car, 0);
		break;
	}
	addCarSides(car);
	// FUN_00478e70: the driver's camera
	{
		int cy = rnd(car->y - (float)_vp.hh);
		int cx = rnd(car->x - (float)_vp.hw);
		cy = cy <= 0 ? 0 : cy;
		cx = cx <= 0 ? 0 : cx;
		int r = _vp.w + cx, b = _vp.h + cy;
		if (_track.w <= r)
			r = _track.w;
		if (_track.h <= b)
			b = _track.h;
		if (r - cx < _vp.w)
			cx = r - _vp.w;
		if (b - cy < _vp.h)
			cy = b - _vp.h;
		d.camL = cx;
		d.camT = cy;
		d.camR = r;
		d.camB = b;
	}
	updateSpeed(car);
	checkPickups(car);
	trickEffects(car);
	drawSkids(car);
}

// FUN_00479d70: the move, the terrain, the lines crossed (finish line:
// lap; the commentator's lap clips), sparks and crash sounds on hits, the
// skid sound
void RacingScene::moveAndTrigger(Driver &d) {
	Car *car = d.car;
	View *v = viewOf(car);
	float oldSpeed = car->speed;
	if (!move(car))
		return;
	checkTerrain(car);
	if (car->trigger) {
		int left = _settings.laps - car->lap;
		switch (car->hitType) {
		case 1:
			if (v && 0 < left + 1 && left + 1 < 8) {
				if (clipIs(*v, Common::String::format("lap%d_0", left + 1)))
					playClip(*v, Common::String::format("lap%d_1", left + 1), false);
			}
			lapDone(d);
			break;
		case 2:
			if (v && 0 < left && left < 8)
				playClip(*v, Common::String::format("lap%d_0", left), true);
			break;
		case 3:
			if (v && 0 < left && left < 8 && clipIs(*v, Common::String::format("lap%d_0", left)))
				stopClip(*v);
			break;
		case 4:
			if (_story && v)
				playClip(*v, "ending", false);
			break;
		default:
			break;
		}
	} else if (car->hitWall) {
		uint32 now = _vm->getGameMillis();
		if (1.0f < oldSpeed && 25 < (int32)(now - car->sparkTime)) {
			int type = (double)car->curTop * 0.9 <= (double)oldSpeed ? kAnimCrash : kAnimSpark;
			startAnim(rnd(car->hitX), rnd(car->hitY), type, car->id);
			car->sparkTime = now;
		}
		playSound(car, car->hitType == -3 ? kSoundCrashCar : kSoundCrash);
	}
	if (car->skidding)
		playSound(car, kSoundSkid);
	else
		stopSound(car, kSoundSkid);
	updateCarSounds(car);
}

// FUN_0047a350: the finish line: the lap's time (when the car really went
// round, the path's laps ahead of the timed ones), the best lap (a
// player's new track record is announced), and the end of the race for
// the car
void RacingScene::lapDone(Driver &d) {
	Car *car = d.car;
	View *v = viewOf(car);
	uint32 now = _vm->getGameMillis();
	uint32 t = now - car->lapStart;
	if (!(_state < kStateFinishClip) || !(car->timedLap < d.laps))
		return;
	int lap = car->timedLap;
	if (0 < lap && lap <= _settings.laps && lap <= 8)
		car->lapTimes[lap] = t;
	if (t < car->bestLap) {
		car->bestLap = t;
		if (!car->ai && t < _settings.bestLap) {
			if (v) {
				v->message = _vm->getString("race:BESTLAP");
				v->messageTime = 5000;
			}
			_settings.bestLap = car->bestLap;
		}
	}
	car->timedLap++;
	car->lapStart = now;
	debug(1, "Racing: %s lap %d in %s (best %s)", car->name.c_str(), lap, formatTime(t).c_str(), formatTime(car->bestLap).c_str());
	if (!car->finished && _settings.laps < car->timedLap) {
		_state = kStateFinished;
		car->finished = 1;
		debug(1, "Racing: %s finished in %s", car->name.c_str(), formatTime(car->raceTime).c_str());
		if (v) {
			playClip(*v, "finish", false);
			_state = kStateFinishClip;
			_lapTimer = false;
		}
	}
}

// FUN_00479c10: the player's keys
void RacingScene::playerInput(Driver &d) {
	Car *car = d.car;
	bool acc = keyDown(d.controls, RaceControls::kAccelerate);
	bool rev = keyDown(d.controls, RaceControls::kReverse);
	bool left = keyDown(d.controls, RaceControls::kLeft);
	bool right = keyDown(d.controls, RaceControls::kRight);
	if (_state == kStateCountdown) {
		this->rev(car, acc, 1, false, 0.0f);
		return;
	}
	if (keyDown(d.controls, RaceControls::kTrick))
		useTrick(car);
	if (keyDown(d.controls, RaceControls::kHorn))
		playSound(car, kSoundHorn);
	else
		stopSound(car, kSoundHorn);
	if (left)
		car->steer = 0;
	if (right)
		car->steer = 1;
	if (!left && !right)
		car->steer = 2;
	if (acc && !rev)
		accelerate(car);
	else if (!acc && rev)
		brake(car);
	else
		coast(car);
}

// FUN_0040e530: a key of the keyboard (GetAsyncKeyState); gamepads are not
// supported here
bool RacingScene::keyDown(const RaceControls *c, int input) const {
	if (!c || c->type != RaceControls::kTypeKeyboard)
		return false;
	int vk = c->keys[input];
	return vk > 0 && vk < 256 && _keys[vk];
}

// ---- pickups and tricks -------------------------------------------------------------

// FUN_00479650: the car's sides crossing a pickup's or a trap's rectangle
void RacingScene::checkPickups(Car *car) {
	Segment sides[4];
	sidesOf(car, sides, false);
	int box[4];
	carBox(car, box);
	for (auto &p : _pickups) {
		if (!p.active)
			continue;
		int r[4];
		pickupRect(p, r);
		if (!rectsMeet(box, r))
			continue;
		bool hit = false;
		for (int i = 0; i < 4 && !hit; i++)
			hit = crossesRect(segPts(sides[i]), r);
		if (!hit)
			continue;
		if (p.kind == 0) {
			hitTrap(car, p);
		} else if (p.kind == 1) {
			playSound(car, kSoundPickup);
			if (p.type == kPickupCarPart) {
				if (car->ai)
					return;
				takeCarPart();
				removePickup(p);
			} else {
				showSlot(car, p.type);
				removePickup(p);
			}
		}
	}
}

// FUN_00479770: oil spins a fast car (100 and more); spikes slow it to a
// fifth and damage it
void RacingScene::hitTrap(Car *car, Pickup &p) {
	if (p.anim == kAnimOil) {
		if (100.0f <= car->speed) {
			if (fabsf(car->sx) < fabsf(car->vx))
				car->sx = car->sx + car->vx;
			if (fabsf(car->sy) < fabsf(car->vy))
				car->sy = car->sy + car->vy;
			if (fabsf(car->spin) < fabsf(car->speed * 0.025f))
				car->spin = (float)(randMod(2) != 0 ? -1 : 1) * car->speed * 0.025f;
			car->vx = car->vy = 0.0f;
		}
	} else if (p.anim == kAnimSpikes) {
		playSound(car, kSoundSpikeHit);
		car->acc = 0.0f;
		car->vx = car->vx * 0.2f;
		car->vy = car->vy * 0.2f;
		addDamage(car, 0.075f);
		p.active = 0;
	}
}

// FUN_004798f0: the trick held is used (before the finish): oil, spikes
// and smoke drop their traps, the turbo lifts the top speed for 8 s,
// repair takes 10 % of the damage (kept when there is none)
void RacingScene::useTrick(Car *car) {
	if (kStateFinished < _state || car->trickHeld == 0 || car->trickActive != 0)
		return;
	switch (car->trickHeld) {
	case kPickupOil:
	case kPickupSpikes:
	case kPickupSmoke: {
		const AnimType &t = _animTypes[trickAnim(car->trickHeld)];
		car->trickCount = t.drops;
		car->trickTimer = t.interval;
		if (View *v = viewOf(car))
			playClip(*v, "dirtytrick", false);
		break;
	}
	case kPickupTurbo:
		car->turboOn = 1.0f;
		car->trickCount = 1;
		car->trickTimer = 8.0f;
		car->turbo = (float)1200 * 0.001f;   // DAT_006ce8d4 x 0.001
		if (View *v = viewOf(car))
			playClip(*v, randMod(2) == 0 ? "turbo2" : "turbo1", false);
		break;
	case kPickupRepair:
		if (car->damage == 0.0f)
			return;
		addDamage(car, -0.1f);
		break;
	default:
		break;
	}
	car->trickActive = car->trickHeld;
	showSlot(car, 0);
	if (_trickSounds[car->trickActive])
		playSound(car, kSoundTrick);
}

// FUN_00479a50: the animation a trick drops (oil, spikes, smoke)
int RacingScene::trickAnim(int trick) const {
	switch (trick) {
	case kPickupOil:
		return kAnimOil;
	case kPickupSpikes:
		return kAnimSpikes;
	case kPickupSmoke:
		return kAnimSmoke;
	default:
		return -1;
	}
}

// FUN_00479a80: an active trick drops its traps behind the car at its
// interval until its count is used up; the turbo ends
void RacingScene::trickEffects(Car *car) {
	if (car->trickActive == 0)
		return;
	car->trickTimer = car->trickTimer - _dt;
	if (car->trickTimer < 0.0f) {
		int at = trickAnim(car->trickActive);
		car->trickCount--;
		if (at >= 0) {
			const Image &frame = car->sprites[car->sprite];
			int px = rnd((double)car->x - sin((double)car->angle) * (double)frame.h * 0.5);
			int py = rnd((double)car->y - cos((double)car->angle) * (double)frame.h * 0.5);
			car->trickTimer = _animTypes[at].interval;
			switch (car->trickActive) {
			case kPickupOil:
				startAnim(px, py, kAnimOil, car->id);
				break;
			case kPickupSpikes:
				startAnim(px, py, kAnimSpikes, car->id);
				break;
			case kPickupSmoke: {
				int w = ftol(car->scaledW);
				int ox = randRange(-w, w) / 2;
				int w2 = ftol(car->scaledW);
				int oy = randRange(-w2, w2) / 2;
				startAnim(px + ox, py + oy, kAnimSmoke, car->id);
				break;
			}
			default:
				break;
			}
		}
	}
	if (car->trickCount < 1) {
		stopSound(car, kSoundTrick);
		if (car->trickActive == kPickupTurbo) {
			car->turboOn = 0.0f;
			car->turbo = 1.0f;
		}
		car->trickActive = 0;
	}
}

// FUN_00479560: an animation on the track, centred on (x, y) (128; when
// all are busy the oldest is taken), a random speed around 1
int RacingScene::startAnim(int x, int y, int type, int owner) {
	int slot = 0;
	while (slot < 128 && _anims[slot].active)
		slot++;
	if (slot == 128) {
		slot = 0;
		for (int i = 0; i < 128; i++)
			if ((int32)_anims[i].start < (int32)_anims[slot].start)
				slot = i;
	}
	const AnimType &t = _animTypes[CLIP(type, 0, 5)];
	RaceAnim &a = _anims[slot];
	a.x = x - (t.image.w / MAX(t.frames, 1)) / 2;
	a.y = y - t.image.h / 2;
	a.active = 1;
	a.type = type;
	a.start = _vm->getGameMillis();
	a.frame = 0.0f;
	a.owner = owner;
	a.speed = ((float)randMod(0x8000) * 3.05185094e-05f - 0.5f) * t.random + 1.0f;
	return slot;
}

// FUN_00478840: 10 frames a second times the speed; damage smoke and smoke
// drift with the wind; finished oil and spikes stay as traps
void RacingScene::updateAnims() {
	for (auto &a : _anims) {
		if (!a.active)
			continue;
		const AnimType &t = _animTypes[CLIP(a.type, 0, 5)];
		a.frame = a.speed * 10.0f * _dt + a.frame;
		if (a.frame < (float)t.frames) {
			if (a.type == kAnimDamage || a.type == kAnimSmoke) {
				a.x += ftol((double)_windX * _dt);
				a.y += ftol((double)_windY * _dt);
			}
		} else {
			if (a.type == kAnimOil || a.type == kAnimSpikes)
				dropTrap(a);
			a.active = 0;
		}
	}
}

// FUN_00478bb0
void RacingScene::dropTrap(RaceAnim &a) {
	Pickup *p = newPickup();
	if (!p)
		return;
	p->x = (float)a.x;
	p->y = (float)a.y;
	p->kind = 0;
	p->anim = a.type;
	p->sprite = nullptr;
	p->node = nullptr;
	p->owner = a.owner;
}

// FUN_00476a80: a free pickup slot of 300, cleared and marked active
RacingScene::Pickup *RacingScene::newPickup() {
	for (auto &p : _pickups) {
		if (p.active)
			continue;
		memset((void *)&p, 0, sizeof(p));
		p.active = 1;
		return &p;
	}
	return nullptr;
}

// FUN_004769f0: a pickup centred on a path node
void RacingScene::putPickup(Pickup *p, PathNode *node, int type) {
	if (!p)
		return;
	p->node = node;
	p->sprite = &_pickupImages[CLIP(type, 0, 6)];
	p->type = type;
	p->kind = 1;
	p->active = 1;
	p->x = (float)(node->x - p->sprite->w / 2);
	p->y = (float)(node->y - p->sprite->h / 2);
}

// FUN_004798c0
void RacingScene::removePickup(Pickup &p) {
	p.active = 0;
	if (p.node)
		p.node->used = 0;
	_pickupCount--;
}

// ---- the cars' sounds ------------------------------------------------------------

RacingScene::Voice *RacingScene::soundVoice(Car *car, int id) {
	switch (id) {
	case kSoundHorn:
		return car->horn;
	case kSoundEngine:
		return car->engine;
	default:
		return id >= 0 && id < 8 ? car->sounds[id] : nullptr;
	}
}

// FUN_00474000: the sounds that loop: skid, brake, horn, engine, and the
// tricks' with their flag
static bool soundLoops(int id, int trick) {
	switch (id) {
	case kSoundSkid:
	case kSoundBrake:
	case kSoundHorn:
	case kSoundEngine:
		return true;
	case kSoundTrick:
		return trick == kPickupOil || trick == kPickupSmoke;  // 0x50ac80 flags
	default:
		return false;
	}
}

// FUN_00473f50
bool RacingScene::soundPlaying(Car *car, int id) {
	Voice *v = soundVoice(car, id);
	return v && v->isLoaded() && v->isPlaying();
}

// FUN_00473ad0: a car's sound; a playing crash or looping one goes on, the
// others start again; crash and skid take one of their sounds at random,
// the trick slot the trick's sound
void RacingScene::playSound(Car *car, int id) {
	if (soundPlaying(car, id)) {
		bool restart = !(id >= 0 && id < 2) && !soundLoops(id, car->trickActive);
		if (!restart)
			return;
		stopSound(car, id);
	}
	Voice *v = nullptr;
	switch (id) {
	case kSoundCrash:
		delete car->sounds[0];
		car->sounds[0] = duplicateVoice(_crashSounds[randMod(3)], false);
		v = car->sounds[0];
		break;
	case kSoundSkid:
		delete car->sounds[3];
		car->sounds[3] = duplicateVoice(_skidSounds[randMod(2)], false);
		v = car->sounds[3];
		break;
	case kSoundTrick:
		delete car->sounds[7];
		car->sounds[7] = duplicateVoice(_trickSounds[CLIP(car->trickActive, 0, 5)], false);
		v = car->sounds[7];
		break;
	default:
		v = soundVoice(car, id);
		break;
	}
	if (!v || !v->isLoaded())
		return;
	placeSound(car->x, car->y, v, id);
	v->play(soundLoops(id, car->trickActive));
}

// FUN_00474050
void RacingScene::stopSound(Car *car, int id) {
	if (soundPlaying(car, id))
		soundVoice(car, id)->stop();
}

// FUN_00473c70: the volume by the distance to the player's car (the nearer
// player's in the split screen; full for the player's own sounds), the
// radius of the sound id (0x4a0008), times the race's volumes; the pan by
// the distance across (none within 250).
void RacingScene::placeSound(float x, float y, Voice *v, int id) {
	static const int kRadius[16] = { 250, 250, 250, 250, 250, 250, 250, 250, 200, 400, 750, 250, 150, 100, 250, 0 };
	if (!v)
		return;
	float dist = 1.0f, dx = 0.0f;
	Car *c0 = _drivers[0].car;
	if (_settings.mode == RaceSettings::kModeTwoPlayers && _drivers[1].car) {
		Car *c1 = _drivers[1].car;
		bool own = (x == c0->x && y == c0->y) || (x == c1->x && y == c1->y);
		if (!own) {
			dist = dx = 3.40282347e+38f;
			for (int i = 0; i < 2; i++) {
				Car *c = _drivers[i].car;
				float ddy = y - c->y;
				float ddx = x - c->x;
				float d = (float)sqrt((double)ddx * ddx + (double)ddy * ddy);
				if (d < dist)
					dist = d;
				if (!(fabsf(dx) <= fabsf(ddx)))
					dx = ddx;
			}
		}
	} else if (c0 && !(x == c0->x && y == c0->y)) {
		float ddy = y - c0->y;
		float ddx = x - c0->x;
		dist = (float)sqrt((double)ddx * ddx + (double)ddy * ddy);
		dx = ddx;
	}
	int radius = kRadius[CLIP(id, 0, 15)];
	float vol = ((float)radius > dist ? 1.0f : (float)radius / dist) * 100.0f;
	float pan;
	if (fabsf(dx) < 250.0f)
		pan = 0.0f;
	else
		pan = (float)(1.0 - 250.0 / fabs((double)dx));
	float opt = _vm->volumeFactor(FlaaklypaEngine::kVolumeOptions, FlaaklypaEngine::kVolumeSound);
	if (id >= 10)
		vol = _vm->volumeFactor(FlaaklypaEngine::kVolumeScene, FlaaklypaEngine::kVolumeSpeech) * _volumes[1] * opt * vol;
	else if (id == 9)
		vol = _vm->volumeFactor(FlaaklypaEngine::kVolumeScene, FlaaklypaEngine::kVolumeMusic) * _volumes[2] * opt * vol;
	else if (id >= 0)
		vol = _vm->volumeFactor(FlaaklypaEngine::kVolumeScene, FlaaklypaEngine::kVolumeSound) * _volumes[0] * opt * vol;
	v->setVolume(ftol(vol));
	v->setPan((dx < 0.0f ? -1.0f : 1.0f) * pan);
}

// FUN_0047a2e0: the engine, the horn and the car's playing sounds follow it
void RacingScene::updateCarSounds(Car *car) {
	placeSound(car->x, car->y, car->engine, kSoundEngine);
	if (car->horn && car->horn->isPlaying())
		placeSound(car->x, car->y, car->horn, kSoundHorn);
	for (int i = 0; i < 8; i++)
		if (car->sounds[i] && car->sounds[i]->isLoaded() && car->sounds[i]->isPlaying())
			placeSound(car->x, car->y, car->sounds[i], i);
}

// FUN_004788f0: the track's sounds
void RacingScene::updateTrackSounds() {
	for (auto &s : _track.sounds)
		placeSound(s.x, s.y, s.voice, s.type + 10);
}

} // End of namespace Flaaklypa
