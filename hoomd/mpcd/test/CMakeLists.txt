# external_field test will not link properly due to separable compilation, disabling.
set(TEST_LIST
    at_collision_method
    cell_list
    cell_thermo_compute
    force
    parallel_plate_geometry_filler
    planar_pore_geometry_filler
    rejection_filler
    sorter
    srd_collision_method
    streaming_method
    virtual_particle
    )

if(ENABLE_MPI)
    MACRO(ADD_TO_MPI_TESTS _KEY _VALUE)
    SET("NProc_${_KEY}" "${_VALUE}")
    SET(MPI_TEST_LIST ${MPI_TEST_LIST} ${_KEY})
    ENDMACRO(ADD_TO_MPI_TESTS)

    ADD_TO_MPI_TESTS(cell_communicator 8)
    ADD_TO_MPI_TESTS(cell_list 8)
    ADD_TO_MPI_TESTS(cell_thermo_compute 8)
    ADD_TO_MPI_TESTS(communicator 8)
    ADD_TO_MPI_TESTS(parallel_plate_geometry_filler 8)
    ADD_TO_MPI_TESTS(planar_pore_geometry_filler 8)
    ADD_TO_MPI_TESTS(rejection_filler 8)
endif()

macro(compile_test TEST_EXE TEST_SRC)
    # check for extra cuda files
    if(ENABLE_HIP AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${TEST_EXE}.cu)
        set(_cuda_sources ${TEST_EXE}.cu)
    else()
        set(_cuda_sources "")
    endif()

    # add and link the unit test executable
    add_executable(${TEST_EXE} EXCLUDE_FROM_ALL ${TEST_SRC} ${_cuda_sources})
    add_dependencies(test_all ${TEST_EXE})
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" AND NOT APPLE)
        # these options are needed to avoid linker errors with GCC
        set(additional_link_options "-Wl,--allow-shlib-undefined -Wl,--no-as-needed")
    endif()
    target_link_libraries(${TEST_EXE} _mpcd ${additional_link_options} pybind11::embed)
endmacro(compile_test)

# add non-MPI tests to test list first
foreach (CUR_TEST ${TEST_LIST})
    set(CUR_TEST_SRC ${CUR_TEST}_test.cc)
    set(CUR_TEST_EXE ${CUR_TEST}_test)
    set(CUR_TEST_NAME mpcd-${CUR_TEST})

    compile_test(${CUR_TEST_EXE} ${CUR_TEST_SRC})
    if (ENABLE_MPI)
        add_test(NAME ${CUR_TEST_NAME} COMMAND ${MPIEXEC} ${MPIEXEC_NUMPROC_FLAG} 1 ${MPIEXEC_POSTFLAGS} $<TARGET_FILE:${CUR_TEST_EXE}>)
    else()
        add_test(NAME ${CUR_TEST_NAME} COMMAND $<TARGET_FILE:${CUR_TEST_EXE}>)
    endif()
endforeach(CUR_TEST)

# add MPI tests
foreach (CUR_TEST ${MPI_TEST_LIST})
    set(CUR_TEST_SRC ${CUR_TEST}_mpi_test.cc)
    set(CUR_TEST_EXE ${CUR_TEST}_mpi_test)
    set(CUR_TEST_NAME mpcd-${CUR_TEST}-mpi)

    compile_test(${CUR_TEST_EXE} ${CUR_TEST_SRC})
    add_test(NAME ${CUR_TEST_NAME} COMMAND
             ${MPIEXEC} ${MPIEXEC_NUMPROC_FLAG}
             ${NProc_${CUR_TEST}} ${MPIEXEC_POSTFLAGS}
             $<TARGET_FILE:${CUR_TEST_EXE}>)
endforeach(CUR_TEST)
