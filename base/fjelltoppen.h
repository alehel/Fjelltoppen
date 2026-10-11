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

#ifndef BASE_FJELLTOPPEN_H
#define BASE_FJELLTOPPEN_H

#include "common/hash-str.h"

/**
 * Fjelltoppen, the stand-alone Flåklypa Grand Prix runner (configure
 * --enable-fjelltoppen): it starts the game instead of the launcher,
 * keeps its config and saves apart from ScummVM's, and asks for the game
 * data on the first launch, copying it from the two CDs if needed.
 */
namespace Fjelltoppen {

/** Puts the config file in the application's folder, unless --config is given. */
void updateStartSettings(Common::StringMap &settings);

/** The application's defaults; call after the config file is loaded. */
void registerDefaults();

/**
 * Makes the game's target the active domain, asking for the game data if
 * there is none yet or it went missing. After a failed run it only asks if
 * the data went missing. Returns false if the application should quit.
 */
bool selectGame(bool afterError = false);

} // End of namespace Fjelltoppen

#endif
