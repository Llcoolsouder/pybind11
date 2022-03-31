// Copyright (c) 2021 The Pybind Development Team.
// All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

#pragma once

#include "../pybind11.h"
#include "../detail/common.h"
#include "../detail/descr.h"
#include "../cast.h"
#include "../pytypes.h"

#include <string>

#ifdef __has_include
#    if defined(PYBIND11_CPP17) && __has_include(<filesystem>) && \
      PY_VERSION_HEX >= 0x03060000
#        include <filesystem>
#        define PYBIND11_HAS_FILESYSTEM 1
namespace fs = std::filesystem;
#    elif __has_include(<experimental/filesystem>)
#        include <experimental/filesystem>
#        define PYBIND11_HAS_EXPERIMENTAL_FILESYSTEM
namespace fs = std::experimental::filesystem;
#    endif
#endif

#if !defined(PYBIND11_HAS_FILESYSTEM) && !defined(PYBIND11_HAS_FILESYSTEM_IS_OPTIONAL)            \
    && !defined(PYBIND11_HAS_EXPERIMENTAL_FILESYSTEM)
#    error                                                                                        \
        "#include <filesystem> is not available. (Use -DPYBIND11_HAS_FILESYSTEM_IS_OPTIONAL to ignore.)"
#endif

PYBIND11_NAMESPACE_BEGIN(PYBIND11_NAMESPACE)
PYBIND11_NAMESPACE_BEGIN(detail)

#if defined(PYBIND11_HAS_FILESYSTEM) || defined(PYBIND11_HAS_EXPERIMENTAL_FILESYSTEM)
template <typename T>
struct path_caster {

private:
    static PyObject *unicode_from_fs_native(const std::string &w) {
#    if !defined(PYPY_VERSION)
        return PyUnicode_DecodeFSDefaultAndSize(w.c_str(), ssize_t(w.size()));
#    else
        // PyPy mistakenly declares the first parameter as non-const.
        return PyUnicode_DecodeFSDefaultAndSize(const_cast<char *>(w.c_str()), ssize_t(w.size()));
#    endif
    }

    static PyObject *unicode_from_fs_native(const std::wstring &w) {
        return PyUnicode_FromWideChar(w.c_str(), ssize_t(w.size()));
    }

public:
    static handle cast(const T &path, return_value_policy, handle) {
        if (auto py_str = unicode_from_fs_native(path.native())) {
            return module_::import("pathlib")
                .attr("Path")(reinterpret_steal<object>(py_str))
                .release();
        }
        return nullptr;
    }

    bool load(handle handle, bool) {
        // PyUnicode_FSConverter and PyUnicode_FSDecoder normally take care of
        // calling PyOS_FSPath themselves, but that's broken on PyPy (PyPy
        // issue #3168) so we do it ourselves instead.
        PyObject *buf = PyOS_FSPath(handle.ptr());
        if (!buf) {
            PyErr_Clear();
            return false;
        }
        PyObject *native = nullptr;
        if constexpr (std::is_same_v<typename T::value_type, char>) {
            if (PyUnicode_FSConverter(buf, &native) != 0) {
                if (auto *c_str = PyBytes_AsString(native)) {
                    // AsString returns a pointer to the internal buffer, which
                    // must not be free'd.
                    value = c_str;
                }
            }
        } else if constexpr (std::is_same_v<typename T::value_type, wchar_t>) {
            if (PyUnicode_FSDecoder(buf, &native) != 0) {
                if (auto *c_str = PyUnicode_AsWideCharString(native, nullptr)) {
                    // AsWideCharString returns a new string that must be free'd.
                    value = c_str; // Copies the string.
                    PyMem_Free(c_str);
                }
            }
        }
        Py_XDECREF(native);
        Py_DECREF(buf);
        if (PyErr_Occurred()) {
            PyErr_Clear();
            return false;
        }
        return true;
    }

    PYBIND11_TYPE_CASTER(T, const_name("os.PathLike"));
};

template <>
struct type_caster<fs::path> : public path_caster<fs::path> {};
#endif // PYBIND11_HAS_FILESYSTEM

PYBIND11_NAMESPACE_END(detail)
PYBIND11_NAMESPACE_END(PYBIND11_NAMESPACE)
