/*! \file OPPDriverPotentialPairGPU.cu
    \brief Defines the driver functions for computing opp pair forces on the GPU
*/

#include "EvaluatorPairOPP.h"
#include "AllDriverPotentialPairGPU.cuh"
hipError_t gpu_compute_opp_forces(
    const pair_args_t& pair_args,
    const EvaluatorPairOPP::param_type *d_params)
    {
    return gpu_compute_pair_forces<EvaluatorPairOPP>(
        pair_args, d_params);
    }


