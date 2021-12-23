import hoomd
import pytest
import numpy as np
from hoomd.error import DataAccessError
from hoomd.logging import LoggerCategories
from hoomd.conftest import logging_check


def test_before_attaching():
    free_volume = hoomd.hpmc.compute.FreeVolume(test_particle_type='B',
                                                num_samples=100)

    assert free_volume.test_particle_type == 'B'
    assert free_volume.num_samples == 100
    with pytest.raises(DataAccessError):
        free_volume.free_volume


def test_after_attaching(simulation_factory, lattice_snapshot_factory):
    snap = lattice_snapshot_factory(particle_types=['A', 'B'])
    sim = simulation_factory(snap)
    mc = hoomd.hpmc.integrate.Sphere()
    mc.shape["A"] = {'diameter': 1.0}
    mc.shape["B"] = {'diameter': 0.2}
    sim.operations.add(mc)

    free_volume = hoomd.hpmc.compute.FreeVolume(test_particle_type='B',
                                                num_samples=100)

    sim.operations.add(free_volume)
    assert len(sim.operations.computes) == 1
    sim.run(0)

    assert free_volume.test_particle_type == 'B'
    assert free_volume.num_samples == 100

    sim.run(10)
    assert isinstance(free_volume.free_volume, float)


_radii = [
    (0.25, 0.05),
    (0.4, 0.05),
    (0.7, 0.17),
]


@pytest.mark.parametrize("radius1, radius2", _radii)
def test_validation_systems(simulation_factory, two_particle_snapshot_factory,
                            lattice_snapshot_factory, radius1, radius2):
    n = 7
    free_volume = (n**3) * (1 - (4 / 3) * np.pi * (radius1 + radius2)**3)
    free_volume = max([0.0, free_volume])
    sim = simulation_factory(
        lattice_snapshot_factory(particle_types=['A', 'B'],
                                 n=n,
                                 a=1,
                                 dimensions=3,
                                 r=0))

    mc = hoomd.hpmc.integrate.Sphere()
    mc.shape["A"] = {'diameter': radius1 * 2}
    mc.shape["B"] = {'diameter': radius2 * 2}
    sim.operations.add(mc)

    free_volume_compute = hoomd.hpmc.compute.FreeVolume(test_particle_type='B',
                                                        num_samples=10000)
    sim.operations.add(free_volume_compute)
    sim.run(0)
    # rtol is fairly high as the free volume available to a sized particle
    # is less than the total available volume
    np.testing.assert_allclose(free_volume,
                               free_volume_compute.free_volume,
                               rtol=2e-2)


def test_logging():
    logging_check(
        hoomd.hpmc.compute.FreeVolume, ('hpmc', 'compute'),
        {'free_volume': {
            'category': LoggerCategories.scalar,
            'default': True
        }})
