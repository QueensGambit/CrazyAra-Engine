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
 * @file: rawnetagent.cpp
 * Created on 12.06.2019
 * @author: queensgambit
 */

#include <blaze/Math.h>
#include "rawnetagent.h"
#include "inputrepresentation.h"
#include "outputrepresentation.h"
#include "constants.h"
#include "misc.h"
#include "../util/blazeutil.h"
#include "uci.h"

using blaze::HybridVector;

RawNetAgent::RawNetAgent(NeuralNetAPI *net, PlaySettings playSettings, float temperature, unsigned int temperature_moves, bool verbose):
    Agent (temperature, temperature_moves, verbose)
{
    this->net = net;
    this->playSettings = playSettings;
    fill(inputPlanes, inputPlanes+NB_VALUES_TOTAL, 0.0f);  // will be filled in evalute_board_state()
}

void RawNetAgent::evalute_board_state(Board *pos, EvalInfo& evalInfo)
{
    for (const ExtMove& move : MoveList<LEGAL>(*pos)) {
        evalInfo.legalMoves.push_back(move);
    }

    // sanity check
    assert(evalInfo.legalMoves.size() >= 1);

    // immediatly stop the search if there's only one legal move
    if (evalInfo.legalMoves.size() == 1) {
        evalInfo.policyProbSmall.resize(1UL);
        evalInfo.policyProbSmall = 1;
          // a value of 0 is likely a wron evaluation but won't be written to stdout
        evalInfo.centipawns = value_to_centipawn(0);
        evalInfo.depth = 0;
        evalInfo.nodes = 0;
        evalInfo.pv = {evalInfo.legalMoves[0]};
    }

    board_to_planes(pos, 0, true, begin(inputPlanes));
    float value;

    NDArray probOutputs = net->predict(begin(inputPlanes), value);
    auto predicted = probOutputs.ArgmaxChannel();
    predicted.WaitToRead();

    int bestIdx = predicted.At(0, 0);

    string bestmove_mxnet;
    if (pos->side_to_move() == WHITE) {
        bestmove_mxnet = LABELS[bestIdx];
    }
    else {
        bestmove_mxnet = LABELS_MIRRORED[bestIdx];
    }

    evalInfo.policyProbSmall.resize(evalInfo.legalMoves.size());
    get_probs_of_move_list(0, &probOutputs, evalInfo.legalMoves, pos->side_to_move(),
                           !net->is_policy_map(), evalInfo.policyProbSmall, net->is_policy_map());
    size_t sel_idx = argmax(evalInfo.policyProbSmall);

    Move bestmove = evalInfo.legalMoves[sel_idx];
    assert(bestmove_mxnet == UCI::move(bestmove, pos->is_chess960()));

    evalInfo.centipawns = value_to_centipawn(value);
    evalInfo.depth = 1;
    evalInfo.nodes = 1;
    evalInfo.isChess960 = pos->is_chess960();
	evalInfo.pv = { bestmove };
}
