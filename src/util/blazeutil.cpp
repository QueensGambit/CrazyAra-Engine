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
 * @file: blazeutil.cpp
 * Created on 17.06.2019
 * @author: queensgambit
 */

#include "blazeutil.h"


DynamicVector<float> get_dirichlet_noise(size_t length, const float alpha)
{
    DynamicVector<float> dirichletNoise(length);

    for (size_t i = 0; i < length; ++i) {
        std::gamma_distribution<float> distribution(alpha, 1.0f);
        dirichletNoise[i] = distribution(generator);
    }
    dirichletNoise /= sum(dirichletNoise);
    return  dirichletNoise;
}