#/***************************************************************************
# *
# *  @license
# *  Copyright (C) Codeplay Software Limited
# *  Licensed under the Apache License, Version 2.0 (the "License");
# *  you may not use this file except in compliance with the License.
# *  You may obtain a copy of the License at
# *
# *      http://www.apache.org/licenses/LICENSE-2.0
# *
# *  For your convenience, a copy of the License has been included in this
# *  repository.
# *
# *  Unless required by applicable law or agreed to in writing, software
# *  distributed under the License is distributed on an "AS IS" BASIS,
# *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# *  See the License for the specific language governing permissions and
# *  limitations under the License.
# *
# *
# *
# **************************************************************************/
cmake_minimum_required(VERSION 3.4.3)
project(blas VERSION 0.2.0 LANGUAGES CXX)

if(POLICY CMP0074)
  cmake_policy(SET CMP0074 NEW)
endif()

if(EXISTS ${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
  include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
  conan_basic_setup(TARGETS NO_OUTPUT_DIRS)
  # We copy the pkgconfig files here
  list(APPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR})
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations  -Wno-deprecated-copy-with-user-provided-copy") 

# This turns the long file names that are generated into short versions using a hash
# This is required to build blas kernels on Windows and on WSL since Windows doesn't
# have good support for long file names
if (WIN32)
  # On WIN32 we can force this option to be true, but when building on WSL this
  # must be set in the command line
  set(ONEMATH_SYCL_BLAS_USE_SHORT_NAMES ON CACHE BOOL "Enable short names for generated files in Windows" FORCE)
endif()

# By default, tall and skinny Gemm is enabled (for better performance)
option(GEMM_TALL_SKINNY_SUPPORT "Whether to enable tall and skinny Gemm" ON)
# By default vectorization in gemm kernels is enabled as it improve the performance on all Devices.
option(GEMM_VECTORIZATION_SUPPORT "Whether to enable vectorization in Gemm kernels" ON)

add_definitions(-DCL_TARGET_OPENCL_VERSION=220)

set(ONEMATH_SYCL_BLAS_INCLUDE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
set(ONEMATH_SYCL_BLAS_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(ONEMATH_SYCL_BLAS_INSTALL_SRC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src> $<INSTALL_INTERFACE:src>)

option(BLAS_ENABLE_EXTENSIONS "Whether to enable BLAS extensions" ON)
option(BLAS_ENABLE_COMPLEX "Whether to enable complex data type for GEMM" ON)
option(BLAS_ENABLE_USM "Whether to enable USM API" ON)
option(BLAS_ENABLE_HALF "Whether to enable sycl::half data type for supported operators" ON)

if (SYCL_COMPILER MATCHES "adaptivecpp" OR ${CMAKE_CXX_COMPILER} MATCHES "acpp|syclcc")
  if(BLAS_ENABLE_COMPLEX)
    message(STATUS "SYCL Complex data is not supported on AdaptiveCpp/hipSYCL. Complex data type is disabled")
    set(BLAS_ENABLE_COMPLEX OFF)
  endif()
  if (BLAS_ENABLE_USM)
    message(STATUS "USM API is not supported on AdaptiveCpp/hipSYCL. USM API is disabled")
    set(BLAS_ENABLE_USM OFF)
  endif()
endif()

add_library(onemath_sycl_blas INTERFACE)
set_target_properties(onemath_sycl_blas PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${ONEMATH_SYCL_BLAS_INCLUDE};${ONEMATH_SYCL_BLAS_INSTALL_SRC}")
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-fsycl" is_dpcpp)
if(is_dpcpp)
  check_cxx_compiler_flag("-mllvm=-enable-global-offset=false" support_disable_global_offset)
  if (${support_disable_global_offset})
    if ((${CMAKE_CXX_COMPILER_ID} STREQUAL "IntelLLVM"
           AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 2024.1)
         OR NOT ${CMAKE_CXX_COMPILER_ID} STREQUAL "IntelLLVM")
       # Apply only for oneAPI releases >= 2024.1 OR for intel/llvm.
       target_link_options(onemath_sycl_blas INTERFACE "-mllvm=-enable-global-offset=false")
       target_compile_options(onemath_sycl_blas INTERFACE "-mllvm=-enable-global-offset=false")
       message(STATUS "Adding -mllvm=-enable-global-offset=false to onemath_sycl_blas")
     endif()
  endif()
endif()
if(${BLAS_ENABLE_COMPLEX})
  target_compile_definitions(onemath_sycl_blas INTERFACE "BLAS_ENABLE_COMPLEX")
endif()
if(${BLAS_ENABLE_USM})
  target_compile_definitions(onemath_sycl_blas INTERFACE "SB_ENABLE_USM")
endif()
target_compile_definitions(onemath_sycl_blas INTERFACE ${TUNING_TARGET})
target_compile_options(onemath_sycl_blas INTERFACE -Wno-deprecated-declarations)
target_compile_options(onemath_sycl_blas INTERFACE -Wno-deprecated-copy-with-user-provided-copy)
if((${CMAKE_CXX_COMPILER_ID} STREQUAL "IntelLLVM") AND NOT
  (${TUNING_TARGET} STREQUAL "INTEL_GPU") )
    target_compile_options(onemath_sycl_blas INTERFACE -fno-fast-math)
    target_compile_options(onemath_sycl_blas INTERFACE -mllvm=-loopopt=0)
    target_link_options(onemath_sycl_blas INTERFACE -mllvm=-loopopt=0)
    message(STATUS "Adding -fno-fast-math -mllvm=-loopopt=0 to onemath_sycl_blas")
endif()

include(CMakePackageConfigHelpers)
set(version_file "${CMAKE_CURRENT_BINARY_DIR}/cmake/onemath_sycl_blas-version.cmake")
set(config_file "${CMAKE_CURRENT_BINARY_DIR}/cmake/onemath_sycl_blas-config.cmake")
set(targets_name "onemath_sycl_blas-targets")
write_basic_package_version_file(${version_file}
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY AnyNewerVersion
)
configure_package_config_file(
  cmake/config.cmake.in
  ${config_file}
  INSTALL_DESTINATION ${CMAKE_INSTALL_PREFIX}
)
install(FILES ${version_file} ${config_file}
  DESTINATION ${CMAKE_INSTALL_PREFIX})

include(GNUInstallDirs)
install(TARGETS onemath_sycl_blas
  EXPORT ${targets_name}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY ${ONEMATH_SYCL_BLAS_INCLUDE}
  DESTINATION ${CMAKE_INSTALL_PREFIX}
  COMPONENT onemath_sycl_blas
  FILES_MATCHING PATTERN "*.h"
)
  install(DIRECTORY ${ONEMATH_SYCL_BLAS_INSTALL_SRC}
    DESTINATION ${CMAKE_INSTALL_PREFIX}
    COMPONENT onemath_sycl_blas
    FILES_MATCHING PATTERN "*.hpp"
  )

install(EXPORT ${targets_name}
  DESTINATION ${CMAKE_INSTALL_PREFIX}
  NAMESPACE ONEMATH_SYCL_BLAS::
)

option(BLAS_ENABLE_CONST_INPUT "Whether to enable kernel instantiation with const input buffer" ON)
