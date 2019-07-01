/*
 * CrazyAra, a deep learning chess variant engine
 * Copyright (C) 2018 Johannes Czech, Moritz Willig, Alena Beyer
 * Copyright (C) 2019 Johannes Czech
 *
 * CrazyAra is free software: You can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * @file: mathutil.cpp
 * Created on 01.07.2019
 * @author: queensgambit
 *
 * Additional plain C++ math utility functions.
 */

#include "mathutil.h"

unsigned int log2I(unsigned int value)
{
    int logValue = 0;
    while (value >>= 1) {
        ++logValue;
    }
    return logValue;
}
