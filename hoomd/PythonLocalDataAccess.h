// Copyright (c) 2009-2022 The Regents of the University of Michigan.
// Part of HOOMD-blue, released under the BSD 3-Clause License.

#ifndef __PYTHON_LOCAL_DATA_ACCESS_H__
#define __PYTHON_LOCAL_DATA_ACCESS_H__

#include "GlobalArray.h"
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <string>
#include <type_traits>
#include <utility>

namespace hoomd
    {
/// Base class for buffers for LocalDataAccess template class type checking.
/** In addition, this class allows for a uniform way of specifying a CPU(Host)
 *  or GPU(Device) buffer.  HOOMDBuffer classes need to implement a templated
 *  make method that takes:
 *
 *  data: pointer to type T
 *  shape: shape of the array
 *  stride: the strides to move one index in each dimension of shape
 *  read_only: whether the buffer is meant to be read_only
 */
struct HOOMDBuffer
    {
    void* m_data;
    std::string m_typestr;
    std::vector<ssize_t> m_shape;
    std::vector<ssize_t> m_strides;
    bool m_read_only;

    HOOMDBuffer(void* data,
                std::string typestr,
                std::vector<ssize_t> shape,
                std::vector<ssize_t> strides,
                bool read_only)
        : m_data(data), m_typestr(typestr), m_shape(shape), m_strides(strides),
          m_read_only(read_only)
        {
        if (m_shape.size() != m_strides.size())
            {
            throw std::runtime_error("buffer shape != strides.");
            }
        }

    bool getReadOnly() const
        {
        return m_read_only;
        }
    };

/// Represents the data required to specify a CPU buffer object in Python.
/** Stores the data to create pybind11::buffer_info objects. This is necessary
 *  since buffer_info objects cannot be reused. Once the buffer is read many of
 *  the members are moved. */
struct HOOMDHostBuffer : public HOOMDBuffer
    {
    static const auto device = access_location::host;
    ssize_t m_itemsize;
    ssize_t m_dimensions;

    HOOMDHostBuffer(void* data,
                    std::string typestr,
                    std::vector<ssize_t> shape,
                    std::vector<ssize_t> strides,
                    bool read_only,
                    ssize_t itemsize,
                    ssize_t dimensions)
        : HOOMDBuffer(data, typestr, shape, strides, read_only), m_itemsize(itemsize),
          m_dimensions(dimensions)
        {
        }

    template<class T>
    static HOOMDHostBuffer
    make(T* data, std::vector<ssize_t> shape, std::vector<ssize_t> strides, bool read_only)
        {
        return HOOMDHostBuffer(data,
                               pybind11::format_descriptor<T>::format(),
                               shape,
                               strides,
                               read_only,
                               sizeof(T),
                               shape.size());
        }

    pybind11::buffer_info new_buffer()
        {
        return pybind11::buffer_info(m_data,
                                     m_itemsize,
                                     m_typestr,
                                     m_dimensions,
                                     std::vector<ssize_t>(m_shape),
                                     std::vector<ssize_t>(m_strides));
        }
    };

#if ENABLE_HIP
/// Represents the data required to implement the __cuda_array_interface__.
/** Creates the Python dictionary to represent a GPU array through the
 *  __cuda_array_interface__. Currently supports version 2 of the protocol.
 */
struct HOOMDDeviceBuffer : public HOOMDBuffer
    {
    static const auto device = access_location::device;

    HOOMDDeviceBuffer(void* data,
                      std::string typestr,
                      std::vector<ssize_t> shape,
                      std::vector<ssize_t> strides,
                      bool read_only)
        : HOOMDBuffer(data, typestr, shape, strides, read_only)
        {
        }

    template<class T>
    static HOOMDDeviceBuffer
    make(T* data, std::vector<ssize_t> shape, std::vector<ssize_t> strides, bool read_only)
        {
        return HOOMDDeviceBuffer(data,
                                 pybind11::format_descriptor<T>::format(),
                                 shape,
                                 strides,
                                 read_only);
        }

    /// Convert object to a __cuda_array_interface__ v2 compliant Python dict.
    /** We can't only add the existing values in the HOOMDDeviceBuffer because
     *  CuPy and potentially other packages that use the interface can't handle
     *  a shape where the first dimension is zero and the rest are non-zero. In
     *  addition, strides must always be the same length as shape, meaning we
     *  need to ensure that we change strides to (0,) when the shape is (0,).
     *  Likewise, we need to ensure that the int that serves as the device
     *  pointer is zero for size 0 arrays.
     */
    pybind11::dict getCudaArrayInterface()
        {
        auto data = std::pair<intptr_t, bool>(0, m_read_only);
        pybind11::list shape {};
        pybind11::list strides {};
        if (m_shape.size() == 0 || m_shape[0] == 0)
            {
            shape.append(0);
            strides.append(0);
            }
        else
            {
            data.first = (intptr_t)m_data;
            for (size_t i = 0; i < m_shape.size(); i++)
                {
                shape.append(m_shape[i]);
                strides.append(m_strides[i]);
                }
            }
        auto interface = pybind11::dict();
        interface["typestr"] = m_typestr;
        interface["version"] = 2;
        interface["data"] = data;
        interface["shape"] = pybind11::tuple(shape);
        interface["strides"] = pybind11::tuple(strides);
        return interface;
        }
    };
#endif

enum class GhostDataFlag
    {
    standard,
    ghost,
    both
    };

/// Base class for accessing Global or GPU arrays/vectors in Python.
/** Template Parameters:
 *  Output - the output buffer class for the class should be HOOMDDeviceBuffer
 *  or HOOMDHostBuffer
 *  Data - the class of the object we wish to expose data from
 *
 *  This class only allows access when the m_in_manager flag is true. The flag
 *  should only be changed when entering or exiting a Python context manager.
 *  The design of Python access is to restrict access to within a context
 *  manager to prevent invalid reads/writes in Python (and SEGFAULTS).
 *
 *  The main methods of LocalDataAccess are getBuffer and getGlobalBuffer which
 *  provide a way to automatically convert an Global/GPUArray into an object of
 *  type Output. All classes that expose arrays in Python should use these
 *  classes.
 *
 *  This class stores ArrayHandles using a unique pointer to prevent a resource
 *  from being dropped before the object is destroyed. This can be simplified if
 *  a move constructor for ArrayHandle is created.
 */
template<class Output, class Data> class LocalDataAccess
    {
    static_assert(
        std::is_base_of<HOOMDBuffer, Output>::value,
        "Output template parameter for LocalDataAccess must be a subclass of HOOMDBuffer.");

    public:
    inline LocalDataAccess(Data& data) : m_data(data), m_in_manager(false) { }

    virtual ~LocalDataAccess() = default;

    /// signifies entering into a Python context manager
    void enter()
        {
        m_in_manager = true;
        }

    /// signifies exiting a Python context manager
    void exit()
        {
        clear();
        m_in_manager = false;
        }

    protected:
    /// Convert Global/GPUArray or vector into an Ouput object for Python
    /** This function is for arrays that are of a size less than or equal to
     *  their global size. An example is particle positions. On each MPI
     *  rank or GPU, the number of positions a ranks knows about (including
     *  ghost particles) is less than or equal to the number of total
     *  particles in the system. For arrays that are the sized according to
     *  the global number, use getGlobalBuffer (quantities such as rtags).
     *
     *  Template parameters:
     *  T: the value stored in the by the internal array (i.e. the template
     *  parameter of the ArrayHandle)
     *  S: the exposed type of data to Python
     *  U: the templated array class returned by the parameter
     *  get_array_func. It is templated off of T (which means that if
     *  U=GlobalArray then the full type is GlobalArray<T>)
     *
     *  Arguments:
     *  handle: a reference to the unique_ptr that holds the ArrayHandle.
     *  get_array_func: the method of m_data to use to access the array.
     *  flag: indications whether to get data on ghost particles and/or
     *  standard particles.
     *  bufferWriteable: Whether this buffer should be read-only or not. If false,
     *  the exposed buffer is read-only. If true, the buffer is writeable only
     *  if the ghost data flag is standard.
     *  second_dimension_size: the size of the second dimension (defaults to
     *  0)
     *  offset: the offset in bytes from the start of the array to the
     *  start of the exposed array in Python (defaults to no offset).
     *  strides: the strides in bytes of the array (defaults to sizeof(T) or
     *  {sizeof(S), sizeof(T)} depending on dimension).
     */
    template<class T, class S, template<class> class U = GlobalArray>
    Output getBuffer(std::unique_ptr<ArrayHandle<T>>& handle,
                     const U<T>& (Data::*get_array_func)() const,
                     GhostDataFlag flag,
                     bool bufferWriteable,
                     unsigned int second_dimension_size = 0,
                     ssize_t offset = 0,
                     std::vector<ssize_t> strides = {})
        {
        checkManager();

        bool read_only = !bufferWriteable;
        if (flag != GhostDataFlag::standard && read_only == false)
            {
            read_only = false;
            }

        updateHandle(handle, get_array_func, read_only);

        auto N = m_data.getN();
        auto ghostN = m_data.getNGhosts();
        auto size = N;
        T* _data = handle.get()->data;

        if (flag == GhostDataFlag::both)
            {
            size += ghostN;
            }
        else if (flag == GhostDataFlag::ghost)
            {
            _data += N;
            size = ghostN;
            }
        S* data = (S*)(((char*)_data) + offset);

        std::vector<ssize_t> shape {size, second_dimension_size};
        if (strides.size() == 0 && second_dimension_size == 0)
            {
            shape.pop_back();
            strides = std::vector<ssize_t>({sizeof(T)});
            }
        else if (strides.size() == 0 && second_dimension_size != 0)
            {
            strides = std::vector<ssize_t>({sizeof(T), sizeof(S)});
            }
        return Output::make(data, shape, strides, read_only);
        }

    /// Convert Global/GPUArray or vector into an Ouput object for Python
    /** This function is for arrays that are of a size equal to their global
     *  size. An example is the reverse tag index. On each MPI rank or GPU,
     *  the size of the particle reverse tag index is equal to the entire
     *  number of particles in the system.  For arrays that are the sized
     *  according to the local box, use getBuffer (quantities such as
     *  particle positions).
     *
     *  Template parameters:
     *  T: the value stored in the by the internal array (i.e. the template
     *  parameter of the ArrayHandle)
     *  U: the templated array class returned by the parameter
     *  get_array_func. It is templated off of T (which means that if
     *  U=GlobalArray then the full type is GlobalArray<T>)
     *
     *  Arguments:
     *  handle: a reference to the unique_ptr that holds the ArrayHandle.
     *  get_array_func: the method of m_data to use to access the array.
     *  of the exposed array in Python.
     *  read_only: whether the array should be read only (defaults to True).
     */
    template<class T, template<class> class U = GlobalArray>
    Output getGlobalBuffer(std::unique_ptr<ArrayHandle<T>>& handle,
                           const U<T>& (Data::*get_array_func)() const,
                           bool read_only = true)
        {
        checkManager();
        updateHandle(handle, get_array_func, read_only);

        auto size = m_data.getNGlobal();
        unsigned int* data = handle.get()->data;

        return Output::make(data,
                            std::vector<ssize_t>({size}),
                            std::vector<ssize_t>({sizeof(T)}),
                            true);
        }

    // clear should remove any references to ArrayHandle objects so the
    // handle can be released for other objects.
    virtual void clear() = 0;

    private:
    /// Ensure that arrays are not accessed outside context manager.
    inline void checkManager()
        {
        if (!m_in_manager)
            {
            throw std::runtime_error("Cannot access arrays outside context manager.");
            }
        }

    /// Helper function to acquire array handle if not already acquired.
    template<class T, template<class> class U>
    void updateHandle(std::unique_ptr<ArrayHandle<T>>& handle,
                      const U<T>& (Data::*get_array_func)() const,
                      bool read_only)
        {
        if (!handle)
            {
            auto mode = read_only ? access_mode::read : access_mode::readwrite;
            handle = std::move(std::unique_ptr<ArrayHandle<T>>(
                new ArrayHandle<T>((m_data.*get_array_func)(), Output::device, mode)));
            }
        }

    /// object to access array data from
    Data& m_data;
    /// flag for being inside Python context manager
    bool m_in_manager;
    };

namespace detail
    {
void export_HOOMDHostBuffer(pybind11::module& m);

void export_GhostDataFlag(pybind11::module& m);

#if ENABLE_HIP
void export_HOOMDDeviceBuffer(pybind11::module& m);
#endif

    } // end namespace detail

    } // end namespace hoomd

#endif
