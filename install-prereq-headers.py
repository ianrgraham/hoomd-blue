import os
import sys
import tempfile
import pathlib
import subprocess
import copy
import logging
import argparse
import urllib.request
import tarfile

log = logging.getLogger(__name__)

def find_cmake_package(name, version, location_variable=None, ignore_system=False):
    """ Find a package with cmake

    Return True if the package is found
    """

    if location_variable is None:
        location_variable = name + "_DIR";

    find_package_options = '';
    if ignore_system:
        find_package_options += 'NO_SYSTEM_ENVIRONMENT_PATH NO_CMAKE_PACKAGE_REGISTRY NO_CMAKE_SYSTEM_PATH NO_CMAKE_SYSTEM_PACKAGE_REGISTRY';

    with tempfile.TemporaryDirectory() as tmpdirname:
        tmp_path = pathlib.Path(tmpdirname);

        # write the cmakelists file
        with open(tmp_path / 'CMakeLists.txt', 'w') as f:
            f.write(f"""
project(test)
set(PYBIND11_PYTHON_VERSION 3)
cmake_minimum_required(VERSION 3.9)
find_package({name} {version} CONFIG REQUIRED {find_package_options})
""");

        # add the python prefix to the cmake prefix path
        env = copy.copy(os.environ)
        env['CMAKE_PREFIX_PATH'] = sys.prefix

        cmake_out = subprocess.run(['cmake', '-S', tmpdirname, '-B', tmp_path / 'build'],
                                    capture_output=True,
                                    timeout=120,
                                    env=env,
                                    encoding='UTF-8'
                                    )

        log.debug(cmake_out.stdout.strip())
        if len(cmake_out.stderr) > 0:
            log.debug(cmake_out.stderr.strip())

        # if cmake completed correctly, the package was found
        if cmake_out.returncode == 0:
            location = ''
            with open(tmp_path / 'build' / 'CMakeCache.txt', 'r') as f:
                for line in f.readlines():
                    if line.startswith(location_variable):
                        location=line.strip();

            log.info(f"Found {name}: {location}")
            return True
        else:
            log.debug(cmake_out.stdout.strip())
            return False

def install_cmake_package(url, cmake_options):
    with tempfile.TemporaryDirectory() as tmpdirname:
        tmp_path = pathlib.Path(tmpdirname);

        log.info(f"Fetching {url}")
        filename, headers = urllib.request.urlretrieve(url, tmp_path / 'file.tar.gz');
        with tarfile.open(tmp_path / 'file.tar.gz') as tar:
            tar.extractall(path=tmp_path)
            root = tar.getnames()[0]
            if '/' in root:
                root = os.path.dirname(root)

        # add the python prefix to the cmake prefix path
        env = copy.copy(os.environ)
        env['CMAKE_PREFIX_PATH'] = sys.prefix


        log.info(f"Configuring {root}")
        cmake_out = subprocess.run(['cmake', '-S', tmp_path / root, '-B', tmp_path / 'build',
                                    f'-DCMAKE_INSTALL_PREFIX={sys.prefix}'] + cmake_options,
                                    capture_output=True,
                                    timeout=120,
                                    env=env,
                                    encoding='UTF-8'
                                    )

        log.debug(cmake_out.stdout.strip())
        if len(cmake_out.stderr) > 0:
            log.debug(cmake_out.stderr.strip())

        if cmake_out.returncode != 0:
            log.error(f"Error configuring {root} (run with -v to see detailed error messages)")
            raise RuntimeError('Failed to configure package');

        log.info(f"Installing {root}")
        cmake_out = subprocess.run(['cmake', '--build', tmp_path / 'build', '--', 'install'],
                                    capture_output=True,
                                    timeout=120,
                                    env=env,
                                    encoding='UTF-8'
                                    )

        log.debug(cmake_out.stdout.strip())
        if len(cmake_out.stderr) > 0:
            log.debug(cmake_out.stderr.strip())

        if cmake_out.returncode != 0:
            log.error(f"Error installing {root} (run with -v to see detailed error messages)")
            raise RuntimeError('Failed to install package');


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Install header-only libraries needed to build HOOMD-blue.')
    parser.add_argument('-q', action='store_true', default=False, help='Suppress info messages.')
    parser.add_argument('-v', action='store_true', default=False, help='Show debug messages (overrides -q).')
    parser.add_argument('-y', action='store_true', default=False, help='Skip user input and force installation.')
    parser.add_argument('--ignore-system', action='store_true', default=False, help='Ignore packages installed at the system level.')
    args = parser.parse_args()

    logging.basicConfig(level=logging.INFO)
    if args.q:
        log.setLevel(level=logging.WARNING)
    if args.v:
        log.setLevel(level=logging.DEBUG)

    log.info(f"Searching for packages in: {sys.prefix}")

    pybind = find_cmake_package('pybind11', '2.0', ignore_system=args.ignore_system);
    cereal = find_cmake_package('cereal', '', ignore_system=args.ignore_system);
    eigen = find_cmake_package('Eigen3', '3.2', ignore_system=args.ignore_system);

    all_found = all([pybind, cereal, eigen])

    if all_found:
        log.info("Done. Found all packages.")
    else:
        missing_packages = '';
        if not pybind:
            missing_packages += 'pybind11, ';
        if not cereal:
            missing_packages += 'cereal, ';
        if not eigen:
            missing_packages += 'Eigen, ';
        missing_packages = missing_packages[:-2];

        if args.y:
            proceed = 'y';
        else:
            print(f"*** About to install {missing_packages} into {sys.prefix}");
            proceed = input('Proceed (y/n)? ')

        if proceed == 'y':
            log.info(f"Installing packages in: {sys.prefix}")

            if not pybind:
                install_cmake_package('https://github.com/pybind/pybind11/archive/v2.3.0.tar.gz',
                                       cmake_options=['-DPYBIND11_INSTALL=on', '-DPYBIND11_TEST=off'])

            if not cereal:
                install_cmake_package('https://github.com/USCiLab/cereal/archive/v1.2.2.tar.gz',
                                       cmake_options=['-DJUST_INSTALL_CEREAL=on'])

            if not eigen:
                install_cmake_package('http://bitbucket.org/eigen/eigen/get/3.3.7.tar.gz',
                                       cmake_options=['-DBUILD_TESTING=off', '-DEIGEN_TEST_NOQT=on'])
            log.info('Done.')
        else:
            print('Cancelled')
