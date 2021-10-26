// Copyright (c) 2009-2021 The Regents of the University of Michigan
// This file is part of the HOOMD-blue project, released under the BSD 3-Clause License.

// Include the defined classes that are to be exported to python
#include "ExamplePotential.h"
#include "hoomd/md/PotentialPair.h"

#include <pybind11/pybind11.h>

#ifdef ENABLE_HIP
#include "hoomd/md/PotentialPairGPU.h"
#include "ExampleDriverPotentialPairGPU.cuh"
#endif

// specify the python module. Note that the name must explicitly match the PROJECT() name provided
// in CMakeLists (with an underscore in front)
PYBIND11_MODULE(_pair_plugin, m)
    {
    export_PotentialPair<PotentialPairExample>(m, "PotentialPairExample");
#ifdef ENABLE_HIP
    export_PotentialPairGPU<PotentialPairExampleGPU, PotentialPairExample>(m, "PotentialPairExampleGPU");
#endif
    }
