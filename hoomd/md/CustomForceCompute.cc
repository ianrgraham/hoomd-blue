// Copyright (c) 2009-2022 The Regents of the University of Michigan.
// Part of HOOMD-blue, released under the BSD 3-Clause License.

#include "CustomForceCompute.h"
#include "hoomd/PythonLocalDataAccess.h"

namespace py = pybind11;

using namespace std;

/*! \file CustomForceCompute.cc
    \brief Contains code for the CustomForceCompute class
*/

namespace hoomd
    {
namespace md
    {
/*! \param sysdef SystemDefinition containing the ParticleData to compute forces on
 */
CustomForceCompute::CustomForceCompute(std::shared_ptr<SystemDefinition> sysdef,
                                       pybind11::object py_setForces,
                                       bool aniso)
    : ForceCompute(sysdef), m_aniso(aniso)
    {
    m_exec_conf->msg->notice(5) << "Constructing ConstForceCompute" << endl;
    m_setForces = py_setForces;
    m_buffers_writeable = true;
    }

CustomForceCompute::~CustomForceCompute()
    {
    m_exec_conf->msg->notice(5) << "Destroying ConstForceCompute" << endl;
    }

/*! This function calls the python set_forces method.
    \param timestep Current timestep
*/
void CustomForceCompute::computeForces(uint64_t timestep)
    {
    // execute python callback to update the forces, if present
    m_setForces(timestep);
    }

namespace detail
    {
void export_CustomForceCompute(py::module& m)
    {
    py::class_<CustomForceCompute, ForceCompute, std::shared_ptr<CustomForceCompute>>(
        m,
        "CustomForceCompute")
        .def(py::init<std::shared_ptr<SystemDefinition>, pybind11::object, bool>());
    }

    } // end namespace detail
    } // end namespace md
    } // end namespace hoomd
