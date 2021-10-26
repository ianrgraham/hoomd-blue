
#include "EvaluatorPairExample.h"
#include "hoomd/md/PotentialPairGPU.cuh"

hipError_t __attribute__((visibility("default")))
gpu_compute_example_forces(const pair_args_t& pair_args,
                          const EvaluatorPairExample::param_type* d_params);
