import pytest

import hoomd
import hoomd.md as md


def make_simulation(simulation_factory, two_particle_snapshot_factory):

    def sim_factory(particle_types=['A'], dimensions=3, d=1, L=20):
        return simulation_factory(
            two_particle_snapshot_factory(particle_types, dimensions, d, L))

    return sim_factory


@pytest.fixture
def integrator_elements():
    nlist = md.nlist.Cell(buffer=0.4)
    lj = md.pair.LJ(nlist=nlist, default_r_cut=2.5)
    gauss = md.pair.Gauss(nlist, default_r_cut=3.0)
    lj.params[("A", "A")] = {"epsilon": 1.0, "sigma": 1.0}
    gauss.params[("A", "A")] = {"epsilon": 1.0, "sigma": 1.0}
    return {
        "methods": [md.methods.NVE(hoomd.filter.All())],
        "forces": [lj, gauss],
        "constraints": [md.constrain.Distance()]
    }


def test_attaching(make_simulation, integrator_elements):
    sim = make_simulation()
    integrator = hoomd.md.Integrator(0.005, **integrator_elements)
    sim.operations.integrator = integrator
    sim.run(0)
    assert integrator._attached
    assert integrator._forces._synced
    assert integrator._methods._synced
    assert integrator._contraints._synced


def test_detaching(make_simulation, methods, forces):
    sim = make_simulation()
    integrator = hoomd.md.Integrator(0.005, **integrator_elements)
    sim.operations.integrator = integrator
    sim.run(0)
    sim.operations._unschedule()
    assert not integrator._attached
    assert not integrator._forces._synced
    assert not integrator._methods._synced
    assert not integrator._contraints._synced
