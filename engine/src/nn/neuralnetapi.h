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
 * @file: neuralnetapi.h
 * Created on 12.06.2019
 * @author: queensgambit
 *
 * This file contains wrappers for handling the neural network.
 * Parts of the code are based on the MXNet C++ inference tutorial:
 * https://github.com/apache/incubator-mxnet/tree/master/cpp-package/example/inference
 */

#ifndef NEURALNETAPI_H
#define NEURALNETAPI_H

#include <iostream>
#include <sys/stat.h>
#include <mutex>
#include "mxnet-cpp/MxNetCpp.h"

using namespace mxnet::cpp;
using namespace std;

class NeuralNetAPI
{
private:
    std::mutex mtx;
    std::map<std::string, NDArray> argsMap;
    std::map<std::string, NDArray> auxMap;
    std::vector<std::string> outputLabels;
    Symbol net;
    Executor *executor;
    Shape inputShape;
    Context globalCtx = Context::cpu();
    unsigned int batchSize;
    bool isPolicyMap;
    bool enableTensorrt;

    /**
     * @brief FileExists Function to check if a file exists in a given path
     * @param name Filepath
     * @return True if exists else false
     */
    inline bool file_exists(const std::string& name);

    /**
     * @brief load_model Loads the model architecture definition from a json file
     * @param model_json_file JSON-Path to the json file
     */
    void load_model(const std::string& jsonFilePath);

    /**
     * @brief load_parameters Loads the parameters a.k.a weights of the model given a parameter path
     * @param model_parameters_file Parameter file path
     */
    void load_parameters(const std::string& paramterFilePath);

    /**
     * @brief bind_executor Binds the executor object to the neural network
     */
    void bind_executor();

    /**
     * @brief infer_select_policy_from_planes Checks if the loaded model encodes the policy as planes
     * and sets the selectPolicyFromPlane boolean accordingly
     */
    void check_if_policy_map();

    /**
     * @brief SplitParamMap Splits loaded param map into arg parm and aux param with target context
     * @param paramMap Parameter map
     * @param argParamInTargetContext Output intermediate parameter map
     * @param auxParamInTargetContext Output intermediate auxiliary map
     * @param targetContext Computation context e.g. Context::cpu(), Context::gpu()
     */
    void SplitParamMap(const std::map<std::string, NDArray> &paramMap,
        std::map<std::string, NDArray> *argParamInTargetContext,
        std::map<std::string, NDArray> *auxParamInTargetContext,
        Context targetContext);

    /**
     * @brief ConvertParamMapToTargetContext Copies the param map into the target context
     * @param paramMap Parameter map
     * @param paramMapInTargetContext Output parameter map
     * @param targetContext Computation context e.g. Context::cpu(), Context::gpu()
     */
    void ConvertParamMapToTargetContext(const std::map<std::string, NDArray> &paramMap,
        std::map<std::string, NDArray> *paramMapInTargetContext,
        Context targetContext);

public:
    /**
     * @brief NeuralNetAPI
     * @param ctx Computation contex either "cpu" or "gpu"
     * @param batchSize Constant batch size which is used for inference
     * @param modelDirectory Directory where the network architecture is stored (.json file) and
     * where parameters a.k.a weights of the neural are stored (.params file) are stored
     */
    NeuralNetAPI(const string& ctx, unsigned int batchSize, const string& modelDirectory, bool enableTensorrt);

    /**
     * @brief predict Runs a prediction on the given inputPlanes and returns the policy vector in form of a NDArray and the value as a float number
     * @param inputPlanes Pointer to the input planes of a single board position
     * @param value Value prediction for the board by the neural network
     * @return Policy NDArray
     */
    NDArray predict(float *inputPlanes, float &value);

    /**
     * @brief predict Runs a prediction on the given inputPlanes and returns the policy vector in form of a NDArray and the value as a float number
     * @param inputPlanes Pointer to the input planes of a single board position
     * @param value Value prediction for the board by the neural network
     * @param probOutputs Policy NDArray of the raw network output (including illegal moves). It's assumend that the memory has already been allocated.
     */
    void predict(float *inputPlanes, NDArray &valueOutput, NDArray &probOutputs);

    bool is_policy_map() const;
};

#endif // NEURALNETAPI_H
