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
 * @file: mctsagent.cpp
 * Created on 13.05.2019
 * @author: queensgambit
 */

#include "mctsagent.h"
#include "../evalinfo.h"
#include "movegen.h"
#include "inputrepresentation.h"
#include "outputrepresentation.h"
#include "constants.h"
#include "../util/blazeutil.h"
#include "mxnet-cpp/MxNetCpp.h"
#include "uci.h"
#include "../manager/statesmanager.h"
#include "../manager/treemanager.h"
#include "../node.h"

using namespace mxnet::cpp;

MCTSAgent::MCTSAgent(NeuralNetAPI *netSingle, NeuralNetAPI** netBatches,
                     SearchSettings* searchSettings, PlaySettings playSettings,
                     StatesManager *states
                     ):
    Agent(playSettings.temperature, playSettings.temperatureMoves, true),
    netSingle(netSingle),
    netBatches(netBatches),
    searchSettings(searchSettings),
    playSettings(playSettings),
    rootNode(nullptr),
    oldestRootNode(nullptr),
    ownNextRoot(nullptr),
    opponentsNextRoot(nullptr),
    states(states),
    lastValueEval(-1.0f)
{
    hashTable = new unordered_map<Key, Node*>;
    hashTable->reserve(1e6);

    for (auto i = 0; i < searchSettings->threads; ++i) {
        searchThreads.push_back(new SearchThread(netBatches[i], searchSettings, hashTable, i));
    }

    valueOutput = new NDArray(Shape(1, 1), Context::cpu());

    if (netSingle->getSelectPolicyFromPlane()) {
        probOutputs = new NDArray(Shape(1, NB_LABELS_POLICY_MAP), Context::cpu());
    } else {
        probOutputs = new NDArray(Shape(1, NB_LABELS), Context::cpu());
    }
    timeManager = new TimeManager();
    generator = default_random_engine(r());
}

size_t MCTSAgent::init_root_node(Board *pos)
{
    size_t nodesPreSearch;
    rootNode = get_root_node_from_tree(pos);

    if (rootNode != nullptr) {
        // swap the states because now the old states are used
        // This way the memory won't be freed for the next new move
        states->swap_states();
        nodesPreSearch = rootNode->numberVisits;
        sync_cout << "info string reuse the tree with " << nodesPreSearch << " nodes" << sync_endl;
    }
    else {
        create_new_root_node(pos);
        nodesPreSearch = 0;
    }
    return nodesPreSearch;
}

Node *MCTSAgent::get_root_node_from_tree(Board *pos)
{
    if (same_hash_key(rootNode, pos)) {
        sync_cout << "info string reuse the full tree" << sync_endl;
        return rootNode;
    }
    if (same_hash_key(ownNextRoot, pos)) {
        ownNextRoot->delete_sibling_subtrees(hashTable);
        opponentsNextRoot->delete_sibling_subtrees(hashTable);
        ownNextRoot->make_to_root();
        return ownNextRoot;
    }
    if (same_hash_key(opponentsNextRoot, pos)) {
        opponentsNextRoot->delete_sibling_subtrees(hashTable);
        opponentsNextRoot->make_to_root();
        return opponentsNextRoot;
    }
    return nullptr;
}

void MCTSAgent::stop_search_based_on_limits()
{
    int curMovetime = timeManager->get_time_for_move(searchLimits, rootNode->pos->side_to_move(), rootNode->pos->plies_from_null()/2);
    sync_cout << "string info movetime " << curMovetime << sync_endl;
    this_thread::sleep_for(chrono::milliseconds(curMovetime/2));
    if (early_stopping()) {
        stop_search();
    } else {
        this_thread::sleep_for(chrono::milliseconds(curMovetime/2));
        if (continue_search()) {
            this_thread::sleep_for(chrono::milliseconds(curMovetime/2));
        }
        stop_search();
    }
}

void MCTSAgent::stop_search()
{
    for (auto searchThread : searchThreads) {
        searchThread->stop();
    }
}

bool MCTSAgent::early_stopping()
{
    //    if (false && max(rootNode->childNumberVisits) > 0.9f * rootNode->numberVisits) {
    if (max(rootNode->policyProbSmall) > 0.9f && argmax(rootNode->policyProbSmall) == argmax(rootNode->qValues)) {
        sync_cout << "string info Early stopping" << sync_endl;
        return true;
    }
    return false;
}

bool MCTSAgent::continue_search() {
    if (searchLimits->movetime == 0 && searchLimits->movestogo != 1 && rootNode->qValues[argmax(rootNode->childNumberVisits)]+0.1f < lastValueEval) {
        sync_cout << "info Increase search time" << sync_endl;
        return true;
    }
    return false;
}

void MCTSAgent::create_new_root_node(Board *pos)
{
    Board* newPos = new Board(*pos);
    newPos->setStateInfo(new StateInfo(*(pos->getStateInfo())));
    if (oldestRootNode != nullptr) {
        sync_cout << "info string delete the old tree " << sync_endl;
        if (opponentsNextRoot != nullptr) {
            opponentsNextRoot->delete_sibling_subtrees(hashTable);
        }
    }
    sync_cout << "info string create new tree" << sync_endl;
    rootNode = new Node(newPos, nullptr, 0, searchSettings);
    oldestRootNode = rootNode;
    board_to_planes(pos, 0, true, begin(input_planes));
    netSingle->predict(input_planes, *valueOutput, *probOutputs);
    get_probs_of_move_list(0, probOutputs, rootNode->legalMoves, newPos->side_to_move(),
                           !netSingle->getSelectPolicyFromPlane(), rootNode->policyProbSmall, netSingle->getSelectPolicyFromPlane());
    rootNode->value = valueOutput->At(0, 0);
    rootNode->enhance_moves();
    rootNode->make_to_root();
    gameNodes.push_back(rootNode);
}


void MCTSAgent::apply_move_to_tree(Move move, bool ownMove)
{
    sync_cout << "info string apply move to tree" << sync_endl;
    if (ownMove) {
        opponentsNextRoot = pick_next_node(move, rootNode);
        if (opponentsNextRoot != nullptr && opponentsNextRoot->hasNNResults) {
            gameNodes.push_back(opponentsNextRoot);
        }
    }
    else {
        ownNextRoot = pick_next_node(move, opponentsNextRoot);
        if (ownNextRoot != nullptr && ownNextRoot->hasNNResults) {
            gameNodes.push_back(ownNextRoot);
        }
    }
}

void MCTSAgent::clear_game_history()
{
    for (Node* node: gameNodes) {
        delete node;
    }
    gameNodes.clear();
    hashTable->clear();
    oldestRootNode = nullptr;
    lastValueEval = -1.0f;
}

void MCTSAgent::evalute_board_state(Board *pos, EvalInfo& evalInfo)
{
    size_t nodesPreSearch = init_root_node(pos);

    if (rootNode->nbDirectChildNodes == 1) {
        sync_cout << "info string Only single move available -> early stopping" << sync_endl;
    }
    else if (rootNode->nbDirectChildNodes == 0) {
        sync_cout << "info string The given position has no legal moves" << sync_endl;
    }
    else {
        sync_cout << "info string apply dirichlet" << sync_endl;
        rootNode->apply_dirichlet_noise_to_prior_policy();
        run_mcts_search();
    }

    evalInfo.policyProbSmall.resize(rootNode->nbDirectChildNodes);
    rootNode->get_mcts_policy(evalInfo.policyProbSmall);

    size_t bestIdx = argmax(evalInfo.policyProbSmall);

    if (bestIdx != argmax(rootNode->childNumberVisits)) {
        sync_cout << "string info Select different move due to higher Q-value" << sync_endl;
    }

    evalInfo.centipawns = value_to_centipawn(this->rootNode->getQValues()[bestIdx]);
    lastValueEval = rootNode->qValues[bestIdx];
    evalInfo.legalMoves = rootNode->getLegalMoves();
//    evalInfo.policyProbSmall = mctsPolicy;
    this->rootNode->get_principal_variation(evalInfo.pv);
    evalInfo.depth = evalInfo.pv.size();
    evalInfo.is_chess960 = pos->is_chess960();
    evalInfo.nodes = rootNode->numberVisits;
    evalInfo.nodesPreSearch = nodesPreSearch;
}

void MCTSAgent::run_mcts_search()
{
    thread** threads = new thread*[searchSettings->threads];
    sync_cout << "sample" << "," << "visits_Ne6" << "," << "qValue_Ne6" << sync_endl;

    for (size_t i = 0; i < searchSettings->threads; ++i) {
        searchThreads[i]->set_root_node(rootNode);
        searchThreads[i]->set_search_limits(searchLimits);
        threads[i] = new thread(go, searchThreads[i]);
    }
    stop_search_based_on_limits();

    for (size_t i = 0; i < searchSettings->threads; ++i) {
        threads[i]->join();
    }
}

void MCTSAgent::print_root_node()
{
    if (rootNode == nullptr) {
        sync_cout << "info string You must do a search before you can print the root node statistics" << sync_endl;
        return;
    }
    sync_cout << rootNode << sync_endl;
}


