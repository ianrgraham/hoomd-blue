
#ifndef __EXAMPLE_PAIR_POTENTIAL_H__
#define __EXAMPLE_PAIR_POTENTIAL_H__

#include "hoomd/md/PotentialPair.h"
#include "EvaluatorPairExample.h"

// #define ENABLE_HIP // to test

#ifdef ENABLE_HIP
#include "ExampleDriverPotentialPairGPU.cuh"
#include "hoomd/md/PotentialPairGPU.h"
#endif

#ifdef __HIPCC__
#error This header cannot be compiled by nvcc
#endif

//! Pair potential force compute for example forces

typedef PotentialPair<EvaluatorPairExample> PotentialPairExample;

#ifdef ENABLE_HIP
//! Pair potential force compute for lj forces on the GPU
typedef PotentialPairGPU<EvaluatorPairExample, gpu_compute_example_forces> PotentialPairExampleGPU;
#endif

#endif // _EXAMPLE_PAIR_POTENTIAL_H__
