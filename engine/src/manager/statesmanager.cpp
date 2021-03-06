/*
  CrazyAra, a deep learning chess variant engine
  Copyright (C) 2018  Johannes Czech, Moritz Willig, Alena Beyer
  Copyright (C) 2019  Johannes Czech

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

/*
 * @file: statesmanager.cpp
 * Created on 04.07.2019
 * @author: queensgambit
 */

#include "statesmanager.h"

StatesManager::StatesManager()
{

}

void StatesManager::swap_states()
{
    activeStates.swap(passiveStates);
}

void StatesManager::clear_states()
{
    if (passiveStates.size() > 0) {
        for (auto state: passiveStates) {
            delete state;
        }
        passiveStates.clear();
    }
}

