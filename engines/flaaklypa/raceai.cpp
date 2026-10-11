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

#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/racing.h"
#include "flaaklypa/raceint.h"

namespace Flaaklypa {

/*
 * The computer's drivers (PATH module FUN_00472000 .. FUN_00473010 and
 * the sensing FUN_004740d0 .. FUN_004746e0): every driver follows the
 * path's nodes (the nearest of the nearby ones by a distance weighted by
 * the car's bias for the node's type, then the next of that type);
 * computer cars steer towards the nodes ahead, feel their way with rays
 * to both sides and ahead (walls, cars, pickups, traps, smoke), back off
 * when stuck and jump to their node when stuck out of sight.
 */

// FUN_00470880 and friends are in racecar.cpp; the ray tests need the
// same arithmetic.
static bool onSeg(const float *s, const float *p) {
	float t, d = s[2] - s[0];
	if (d == 0.0f)
		t = (p[1] - s[1]) / (s[3] - s[1]);
	else
		t = (p[0] - s[0]) / d;
	return t <= 1.0f && t >= 0.0f;
}

static bool crossAt(const float *a, const float *b, int32 *out) {
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
	if (!onSeg(a, p) || !onSeg(b, p))
		return false;
	if (out) {
		out[0] = rnd(p[0]);
		out[1] = rnd(p[1]);
	}
	return true;
}

static bool crossRect(const float *s, const int *r) {
	const float l = (float)r[0], t = (float)r[1], rr = (float)r[2], b = (float)r[3];
	const float e0[4] = { l, t, rr, t };
	const float e1[4] = { rr, t, rr, b };
	const float e2[4] = { rr, b, l, b };
	const float e3[4] = { l, b, l, t };
	return crossAt(s, e0, nullptr) || crossAt(s, e1, nullptr) || crossAt(s, e2, nullptr) || crossAt(s, e3, nullptr);
}

static void boxOf(const float *s, int *r) {
	r[0] = rnd(MIN(s[0], s[2]));
	r[2] = rnd(s[0] < s[2] ? s[2] : s[0]);
	r[1] = rnd(MIN(s[1], s[3]));
	r[3] = rnd(s[1] < s[3] ? s[3] : s[1]);
}

// FUN_004721f0: a driver of the computer (the path is followed for every
// driver, the progress needs it)
void RacingScene::aiDrive(Driver &d) {
	followPath(d);
	Car *car = d.car;
	if (!car->ai && !_autopilot)
		return;
	if (!(car->damage < 1.0f)) {
		coast(car);
		return;
	}
	if (car->backTimer <= 0.0f) {
		if (!car->canTurn && !car->couldTurn)
			car->backTimer = (float)randMod(40) * 0.01f + 0.1f;
		if (car->hitCar)
			car->backTimer = (float)randMod(10) * 0.01f + 0.1f;
	}
	if (0.0f < car->backTimer) {
		// stuck: back off (or forward when it was reversing)
		float t = car->backTimer - _dt;
		car->backTimer = 0.0f < t ? t : 0.0f;
		car->steer = 2;
		if (car->forward)
			brake(car);
		else
			accelerate(car);
		return;
	}
	// the horn sounds a while, then not for 5..10 s
	{
		float h = d.hornTimer;
		d.hornTimer = h - (_dt <= h ? _dt : h);
		if (d.hornTimer == 0.0f && car->horn && car->horn->isPlaying()) {
			car->horn->stop();
			d.hornTimer = (float)randMod(500) * 0.01f + 5.0f;
		}
	}
	if (wantsPit(car) && 0.0f < car->terrain->repair) {
		coast(car);
	} else {
		Hit hit;
		int len = rayLength(car, 200, 0.8f);
		if (castRay(d, hit, 0.0f, len, true, false, true) && !(car->bias[4 + hit.type] < 1.0f)) {
			if ((double)hit.frac < 0.75 || car->speed < 25.0f)
				brake(car);
			else
				coast(car);
		} else {
			aiSpeed(d);
		}
	}
	float a = steerToPath(d);
	int32 idB = (int32)0x80000000, idC = (int32)0x80000001;
	float b = sideSensors(d, 1, &idB);
	float c = sideSensors(d, -1, &idC);
	float s = idC == idB ? a : (b - c) + a;
	// (the FPU's compares fail on a not-a-number: left)
	if (s != s)
		car->steer = 0;
	else if ((double)s > 0.0025)
		car->steer = 1;
	else if ((double)s < -0.0025)
		car->steer = 0;
	else
		car->steer = 2;
}

// FUN_00472510: the nearest node within 12 of the sequence (weighted by the
// car's bias for its type), then the node after it is aimed at; the
// driver's sequence and its laps along the path, the car's lap, and the
// jump to the node of a computer car stuck out of sight for 2.5 s
void RacingScene::followPath(Driver &d) {
	if (_path.empty() || _maxSeq <= 0)
		return;
	Car *car = d.car;
	float best = 3.40282347e+38f;
	int32 cur = d.node->seq;
	int32 lo = (_maxSeq - 12 + cur) % _maxSeq;
	int32 hi = (cur + 12) % _maxSeq;
	for (auto &n : _path) {
		bool consider = lo < hi ? (lo <= n.seq && n.seq <= hi) : (lo <= n.seq || n.seq <= hi);
		if (!consider)
			continue;
		float dist = nodeDistance(n, d);
		if (dist < best) {
			best = dist;
			d.node = &n;
		}
	}
	int32 nearestSeq = d.node->seq;
	PathNode *target = nextNode(d.node, car);
	d.node = target;
	if (target->seq != d.seq) {
		if (d.seq < target->seq && target->seq < d.seq + 18)
			d.seq = target->seq;
		if (_maxSeq - 18 <= d.seq && target->seq < nearestSeq) {
			d.seq = target->seq;
			d.laps++;
		}
	}
	if (target->seq != car->pathSeq)
		d.stuck = 2.5f;
	if (car->ai && car->damage < 1.0f && !isVisible(car)) {
		d.stuck = d.stuck - _dt;
		if (d.stuck <= 0.0f) {
			float ox = car->x, oy = car->y;
			car->x = (float)target->x;
			car->y = (float)target->y;
			if (!canBeAt(car)) {
				car->x = ox;
				car->y = oy;
			} else {
				playSound(car, kSoundTeleport);
				debug(2, "Racing: %s jumps to node %d", car->name.c_str(), target->seq);
			}
			d.stuck = 2.5f;
		}
	}
	int32 old = car->pathSeq;
	car->pathSeq = target->seq;
	if (_maxSeq - 24 <= old && car->pathSeq < 25) {
		car->lap++;
		return;
	}
	if (_maxSeq - 24 <= car->pathSeq && old < 25)
		car->lap--;
}

// FUN_00472740: the next node of the same type (the pit lane's, type 2,
// when the car wants repairs) within the next sequence number, else one
// of the first four, else the next normal one
RacingScene::PathNode *RacingScene::nextNode(PathNode *node, Car *car) {
	int32 type = wantsPit(car) ? 2 : node->type;
	int n = _path.size();
	int i = node->index + 1;
	int j = i;
	if (i < n) {
		for (; j < n; j++) {
			if (node->seq + 1 < _path[j].seq)
				break;
			if (node->seq < _path[j].seq && _path[j].type == type)
				return &_path[j];
		}
	}
	if (j == n) {
		for (int k = 0; k < 4 && k < n; k++)
			if (_path[k].type == type)
				return &_path[k];
	}
	if (n <= i)
		return &_path[0];
	for (int k = i; k < n; k++)
		if (node->seq < _path[k].seq && _path[k].type == 0)
			return &_path[k];
	return &_path[0];
}

// FUN_00472850 / FUN_004728a0: the integer distance to the car (truncated)
// times the car's bias for the node's type
float RacingScene::nodeDistance(const PathNode &n, Driver &d) {
	int32 cy = ftol(d.car->y), cx = ftol(d.car->x);
	int32 dx = n.x - cx, dy = n.y - cy;
	double dist = sqrt((double)(int32)(dy * dy + dx * dx));
	return (float)(dist * (double)d.car->bias[CLIP<int32>(n.type, 0, 3)]);
}

// FUN_004728d0: a node's speed (a share of the top speed) holds the car
// back; no node of the game's tracks has one, so the computer accelerates
void RacingScene::aiSpeed(Driver &d) {
	Car *car = d.car;
	float s = d.node->speed;
	if (0.0f < s) {
		if (s < 0.1f)
			s = 0.1f;
		else if (1.0f < s)
			s = 1.0f;
		float f = car->speed / car->curTop - s;
		if (0.0f <= f) {
			if (0.25 <= (double)f)
				brake(car);
			else
				coast(car);
			return;
		}
	}
	accelerate(car);
}

// FUN_00472970: a ray's length: len x (1 - frac) plus len x frac by the
// speed's share of 350, times the cars' scale
int RacingScene::rayLength(Car *car, int len, float frac) {
	float l = (float)len;
	double v = (double)fabsf(car->speed) * (double)(l * frac) * (1.0 / 350.0) + (double)((1.0f - frac) * l);
	return ftol(v * (double)_scale);
}

// FUN_004729b0: a ray from the car at an angle to its heading
bool RacingScene::castRay(Driver &d, Hit &hit, float angle, int len, bool anims, bool pickups, bool walls) {
	Car *car = d.car;
	Ray r;
	r.x1 = car->x;
	r.y1 = car->y;
	double a = (double)car->angle - (double)kHalfPi + (double)angle;
	r.x2 = (float)(cos(a) * len + (double)car->x);
	r.y2 = (float)((double)car->y - sin(a) * len);
	r.carId = car->id;
	r.len = len;
	r.anims = anims;
	r.pickups = pickups;
	r.walls = walls;
	return sense(r, hit);
}

// FUN_004741b0: the nearest of the walls and cars (a car only when it is
// slower and going the same way, and then a quarter as far), the pickups
// and traps, and the smoke the ray crosses
bool RacingScene::sense(const Ray &ray, Hit &hit) {
	hit.type = 0;
	hit.frac = 1.0f;
	hit.x = hit.y = hit.id = 0;
	Hit tmp;
	tmp.type = 0;
	tmp.frac = 0.0f;
	tmp.x = tmp.y = tmp.id = 0;
	if (ray.walls) {
		for (const auto &s : _segs) {
			if (!senseSegment(ray, s, tmp) || !(tmp.frac < hit.frac))
				continue;
			if (s.id < 1) {
				hit = tmp;
			} else if (closingIn(ray.carId, s.id)) {
				hit = tmp;
				hit.frac = hit.frac * 0.25f;
			}
		}
	}
	if (ray.pickups) {
		for (const auto &p : _pickups)
			if (sensePickup(ray, p, tmp) && tmp.frac < hit.frac)
				hit = tmp;
	}
	if (ray.anims) {
		for (const auto &a : _anims)
			if (senseAnim(ray, a, tmp) && tmp.frac < hit.frac)
				hit = tmp;
	}
	return hit.type != 0;
}

// FUN_00474480: a wall (3) or a car's side (2, at the car)
bool RacingScene::senseSegment(const Ray &ray, const Segment &s, Hit &hit) {
	if (ray.carId == s.id || s.type >= 0)
		return false;
	const float rs[4] = { ray.x1, ray.y1, ray.x2, ray.y2 };
	const float ss[4] = { s.x1, s.y1, s.x2, s.y2 };
	int r1[4], r2[4];
	boxOf(rs, r1);
	boxOf(ss, r2);
	if (!(r1[0] <= r2[2] && r1[1] <= r2[3] && r2[0] <= r1[2] && r2[1] <= r1[3]))
		return false;
	int32 pt[2];
	if (!crossAt(rs, ss, pt))
		return false;
	double dy = (double)pt[1] - ray.y1, dx = (double)pt[0] - ray.x1;
	hit.frac = (float)(sqrt(dx * dx + dy * dy) / (double)ray.len);
	hit.id = s.id;
	if (s.type == -3) {
		hit.type = 2;
		hit.x = s.car ? ftol(s.car->x) : 0;
		hit.y = s.car ? ftol(s.car->y) : 0;
	} else if (s.type > -3) {
		hit.type = 3;
		hit.x = pt[0];
		hit.y = pt[1];
	}
	return true;
}

// FUN_004745c0: a pickup (1) or a trap (4); never the car part
bool RacingScene::sensePickup(const Ray &ray, const Pickup &p, Hit &hit) {
	if (!p.active || p.type == kPickupCarPart)
		return false;
	int r[4];
	pickupRect(p, r);
	int cx = (r[2] - r[0]) / 2 + ftol(p.x);
	int cy = (r[3] - r[1]) / 2 + ftol(p.y);
	const float rs[4] = { ray.x1, ray.y1, ray.x2, ray.y2 };
	if (!crossRect(rs, r))
		return false;
	double dy = (double)cy - ray.y1, dx = (double)cx - ray.x1;
	hit.frac = (float)(sqrt(dx * dx + dy * dy) / (double)ray.len);
	hit.id = 0;
	hit.x = cx;
	hit.y = cy;
	hit.type = p.kind == 1 ? 1 : 4;
	return true;
}

// FUN_00474320: an animation the computer avoids (smoke), not the car's own
bool RacingScene::senseAnim(const Ray &ray, const RaceAnim &a, Hit &hit) {
	if (!a.active || ray.carId == a.owner)
		return false;
	const AnimType &t = _animTypes[CLIP(a.type, 0, 5)];
	if (!t.sensed)
		return false;
	// FUN_00474440
	int r[4] = { a.x, a.y, t.image.w / MAX(t.frames, 1) + a.x, t.image.h + a.y };
	int cx = (r[2] - r[0]) / 2 + a.x;
	int cy = (r[3] - r[1]) / 2 + a.y;
	const float rs[4] = { ray.x1, ray.y1, ray.x2, ray.y2 };
	if (!crossRect(rs, r))
		return false;
	double dy = (double)cy - ray.y1, dx = (double)cx - ray.x1;
	hit.frac = (float)(sqrt(dx * dx + dy * dy) / (double)ray.len);
	hit.x = cx;
	hit.y = cy;
	hit.id = 0;
	hit.type = 4;
	return true;
}

// FUN_004740d0: the other car goes the same way (cos >= 0.5) and slower
bool RacingScene::closingIn(int32 carId, int32 otherId) {
	Car *a = nullptr, *b = nullptr;
	for (auto *c : _cars) {
		if (c->id == carId)
			a = c;
		else if (c->id == otherId)
			b = c;
	}
	if (!a || !b)
		return false;
	float va[2] = { a->vx, a->vy }, vb[2] = { b->vx, b->vy };
	float la = (float)sqrt((double)va[1] * va[1] + (double)va[0] * va[0]);
	va[0] /= la;
	va[1] /= la;
	float lb = (float)sqrt((double)vb[1] * vb[1] + (double)vb[0] * vb[0]);
	vb[0] /= lb;
	vb[1] /= lb;
	float dot = vb[0] * va[0] + vb[1] * va[1];
	return 0.5f <= dot && b->speed < a->speed;
}

// FUN_00472a50: towards the nodes ahead: the more the direction differs
// from the heading, the harder (times the car's track bias, a twentieth)
float RacingScene::steerToPath(Driver &d) {
	Car *car = d.car;
	double a = (double)car->angle - (double)kHalfPi;
	float c = (float)cos(a), ns = (float)-sin(a);
	float dir[2];
	pathDirection(d, dir);
	float lat = (float)((double)dir[0] * ns - (double)dir[1] * c);
	float fwd = (float)((double)dir[1] * ns + (double)dir[0] * c);
	float v;
	if (lat < 0.0f) {
		v = (1.0f - fwd) * car->trackBias;
	} else {
		if (lat <= 0.0f)
			return 0.0f * 0.05f;
		v = -((1.0f - fwd) * car->trackBias);
	}
	if (0.0f < v && !sideFree(d, -1))
		v = v * 0.1f;
	if (v < 0.0f && !sideFree(d, 1))
		return v * 0.1f * 0.05f;
	return v * 0.05f;
}

// FUN_00472b80: the sum of the directions to the next 1 + 8 x speed share
// nodes, made unit
void RacingScene::pathDirection(Driver &d, float out[2]) {
	Car *car = d.car;
	int n = 1 - ftol((double)(car->speed / car->curTop) * -8.0);
	float cx = car->x, cy = car->y;
	PathNode *node = d.node;
	out[0] = out[1] = 0.0f;
	for (int i = 0; i < n; i++) {
		out[0] = (float)((double)node->x - cx) + out[0];
		out[1] = (float)((double)node->y - cy) + out[1];
		node = nextNode(node, car);
	}
	float l = (float)sqrt((double)out[1] * out[1] + (double)out[0] * out[0]);
	out[0] = out[0] / l;
	out[1] = out[1] / l;
}

// FUN_00472e00 (side 1) / FUN_00472c40 (side -1): a short ray at 60..90
// degrees and a long one at 7..45 degrees (narrower with speed) to the
// side; the long one counts three quarters
float RacingScene::sideSensors(Driver &d, int side, int32 *hitId) {
	Car *car = d.car;
	int len1 = rayLength(car, 30, 0.25f);
	float a1 = sensorAngle(car, 1.04719758f, 1.57079637f);
	float v1 = sensor(d, side < 0 ? -a1 : a1, len1, nullptr);
	int len2 = rayLength(car, 150, 0.6f);
	float a2 = sensorAngle(car, 0.125663713f, 0.785398185f);
	float v2 = sensor(d, side < 0 ? -a2 : a2, len2, hitId);
	float r = (float)((double)v2 * 0.75f + v1);
	if (!sideFree(d, side < 0 ? 1 : -1))
		return r * 0.1f;
	return r;
}

// FUN_00472cf0: from hi at rest to lo at a speed of 350
float RacingScene::sensorAngle(Car *car, float lo, float hi) {
	double v = (double)hi - ((double)hi - lo) * fabs((double)car->speed) * (1.0 / 350.0);
	if (v < (double)lo)
		return lo;
	if ((double)hi < v)
		return hi;
	return (float)v;
}

// FUN_00472d40: what a ray finds, closer is stronger (25 / len), times the
// car's bias for it; a slower car ahead within the horn's bias gets the
// horn (then not for 0.25..1 s)
float RacingScene::sensor(Driver &d, float angle, int len, int32 *hitId) {
	Hit hit;
	if (!castRay(d, hit, angle, len, true, true, true))
		return 0.0f;
	Car *car = d.car;
	if (hit.type == 2 && hit.frac < car->hornBias && d.hornTimer == 0.0f) {
		playSound(car, kSoundHorn);
		d.hornTimer = (float)randMod(75) * 0.01f + 0.25f;
	}
	if (hitId)
		*hitId = hit.id;
	return (float)((1.0 - (double)hit.frac) * ((double)(25.0f / (float)len) * car->bias[4 + CLIP<int32>(hit.type, 0, 4)]));
}

// FUN_00472ec0: meant to find a car beside, it only looks at the driver
// itself (the loop compares the driver with each record), so it always
// answers that the side is free.
bool RacingScene::sideFree(Driver &d, int side) {
	Car *car = d.car;
	double a = (double)side * (double)kHalfPi + (double)car->angle;
	float s = (float)sin(a), c = (float)cos(a);
	float mine = (float)((double)c * car->y + (double)s * car->x);
	for (int i = 0; i < _settings.carCount; i++) {
		Driver &o = _drivers[i];
		if (&o != &d)
			continue;
		Car *oc = o.car;
		float ds = (float)(car->pathSeq - oc->pathSeq);
		if (!(ds <= 2.0f))
			continue;
		if (!(ds <= 0.0f) && !(car->speed <= oc->speed))
			continue;
		if (ds < -1.0f)
			continue;
		if (ds < 0.0f && oc->speed < car->speed)
			continue;
		float p = (float)((double)c * oc->y + (double)s * oc->x) - mine;
		if (0.0f < p && p < 50.0f)
			return false;
	}
	return true;
}

// FUN_004731e0: the car is in the viewport drawn last (never in the demo)
bool RacingScene::isVisible(Car *car) {
	if (_settings.mode == RaceSettings::kModeDemo)
		return false;
	return onScreen(rnd(car->x), rnd(car->y));
}

} // End of namespace Flaaklypa
