#-------------------------------------------------------------------------------
# GraphBLAS/GraphBLAS/CMakeLists.txt:  alternative cmake script for GraphBLAS
#-------------------------------------------------------------------------------

# SuiteSparse:GraphBLAS, Timothy A. Davis, (c) 2017-2022, All Rights Reserved.
# The @GrB interface is under the GNU GPLv3 (or later) license.
# SPDX-License-Identifier: GPL-3.0-or-later

# CMakeLists.txt: instructions for cmake to build GraphBLAS for use in MATLAB
# R2021a and following, includes v3.3.3 of GraphBLAS, used by the built-in
# C=A*B.  Using this version of GraphBLAS causes a naming conflict, which this
# cmake handles.

#-------------------------------------------------------------------------------
# get the version
#-------------------------------------------------------------------------------

cmake_minimum_required ( VERSION 3.13 )

message ( STATUS "CMake version: " ${CMAKE_VERSION} )

if ( CMAKE_VERSION VERSION_GREATER "3.0" )
    cmake_policy ( SET CMP0042 NEW )
    cmake_policy ( SET CMP0048 NEW )
    cmake_policy ( SET CMP0054 NEW )
endif ( )

# MacOS settings
set ( CMAKE_MACOSX_RPATH TRUE )

# version of SuiteSparse:GraphBLAS (must match ../CMakeLists.txt)
set ( GraphBLAS_DATE "Feb 14, 2022" )
set ( GraphBLAS_VERSION_MAJOR 6 )
set ( GraphBLAS_VERSION_MINOR 2 )
set ( GraphBLAS_VERSION_SUB   0 )

message ( STATUS "Building SuiteSparse:GraphBLAS version: v" ${GraphBLAS_VERSION_MAJOR}.${GraphBLAS_VERSION_MINOR}.${GraphBLAS_VERSION_SUB}  " date: " ${GraphBLAS_DATE} )

# GraphBLAS C API Specification version, at graphblas.org
if ( GraphBLAS_VERSION_MAJOR GREATER_EQUAL 6 )
    # SuiteSparse:GraphBLAS v6.0.0 and later fully implement the v2 spec
    set ( GraphBLAS_API_DATE "Nov 15, 2021" )
    set ( GraphBLAS_API_VERSION_MAJOR 2 )
    set ( GraphBLAS_API_VERSION_MINOR 0 )
    set ( GraphBLAS_API_VERSION_SUB   0 )
else ( )
    # SuiteSparse:GraphBLAS v5.2.0 has much of the v2 spec but not all.
    # It is backward compatible with all v5.x which have little of the new
    # features of the v2 C API Specification.
    set ( GraphBLAS_API_DATE "Sept 25, 2019" )
    set ( GraphBLAS_API_VERSION_MAJOR 1 )
    set ( GraphBLAS_API_VERSION_MINOR 3 )
    set ( GraphBLAS_API_VERSION_SUB   0 )
endif ( )

message ( STATUS "GraphBLAS C API: v" "${GraphBLAS_API_VERSION_MAJOR}.${GraphBLAS_API_VERSION_MINOR}, date: ${GraphBLAS_API_DATE}" )

project ( graphblas_renamed
    VERSION "${GraphBLAS_VERSION_MAJOR}.${GraphBLAS_VERSION_MINOR}.${GraphBLAS_VERSION_SUB}" LANGUAGES C )

#-------------------------------------------------------------------------------
# find OpenMP and cpu_features
#-------------------------------------------------------------------------------

find_package ( OpenMP )

if ( NOT GBNCPUFEAT )
    # default: enable Google's cpu_features package
    message ( STATUS "cpu_features (by google.com): enabled " )
    include_directories ( "../cpu_features/include" "../cpu_features" "../cpu_features/src" "../cpu_features/include/internal" )
else ( )
    # disable Google's cpu_features package
    message ( STATUS "cpu_features (by google.com): disabled" )
    set ( CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -DGBNCPUFEAT " )
endif ( )

if ( DEFINED GBX86 )
    # default: this is detected automatically, but can be set here also
    if ( GBX86 )
        set ( CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -DGBX86=1 " )
    else ( )
        set ( CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -DGBX86=0 " )
    endif ( )
endif ( )

if ( DEFINED GBAVX2 )
    # default: this is detected automatically, but can be set here also
    if ( GBAVX2 )
        set ( CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -DGBAVX2=1 " )
    else ( )
        set ( CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -DGBAVX2=0 " )
    endif ( )
endif ( )

if ( DEFINED GBAVX512F )
    # default: this is detected automatically, but can be set here also
    if ( GBAVX512F )
        set ( CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -DGBAVX512F=1 " )
    else ( )
        set ( CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -DGBAVX512F=0 " )
    endif ( )
endif ( )

#-------------------------------------------------------------------------------
# determine build type
#-------------------------------------------------------------------------------

include ( GNUInstallDirs )

# Edit these lines for code development only, not for end-users:
# set ( GBCOMPACT true )
# set ( CMAKE_BUILD_TYPE Debug )

if ( NOT CMAKE_BUILD_TYPE )
    set ( CMAKE_BUILD_TYPE Release )
endif ( )

set ( CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -DGBRENAME=1 " )

message ( STATUS "CMAKE build type:          " ${CMAKE_BUILD_TYPE} )

if ( ${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    message ( STATUS "CMAKE C Flags debug:       " ${CMAKE_C_FLAGS_DEBUG} )
else ( )
    message ( STATUS "CMAKE C Flags release:     " ${CMAKE_C_FLAGS_RELEASE} )
endif ( )

message ( STATUS "CMAKE compiler ID:         " ${CMAKE_C_COMPILER_ID} )
message ( STATUS "CMAKE have OpenMP:         " ${OPENMP_FOUND} )

if ( GBCOMPACT )
    set ( CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -DGBCOMPACT=1 " )
endif ( )

#-------------------------------------------------------------------------------
# include directories
#-------------------------------------------------------------------------------

set ( CMAKE_INCLUDE_CURRENT_DIR ON )

if ( GBCOMPACT )
    include_directories ( ../Source/Template ../Source ../Include rename ../Source/Generated1 ../lz4 ../rmm_wrap )
else ( )
    include_directories ( ../Source/Template ../Source ../Include rename ../Source/Generated1 ../lz4 ../rmm_wrap ../Source/Generated2 )
endif ( )

#-------------------------------------------------------------------------------
# compiler options:
#-------------------------------------------------------------------------------

# check which compiler is being used.  If you need to make
# compiler-specific modifications, here is the place to do it.
if ( "${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
    # cmake 3.0 doesn't have this problem.
    set ( CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -std=c11 -lm -Wno-pragmas " )
    # operations may be carried out in higher precision
    set ( CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -fexcess-precision=fast " )
    # faster single complex multiplication and division
    set ( CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -fcx-limited-range " )
    # math functions do not need to report errno
    set ( CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -fno-math-errno ")
    # integer operations wrap
    set ( CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -fwrapv ")
    # check all warnings (uncomment for development only)
#   set ( CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic -Werror " )
    if ( CMAKE_C_COMPILER_VERSION VERSION_LESS 4.9 )
        message ( FATAL_ERROR "gcc version must be at least 4.9" )
    endif ( )
elseif ( "${CMAKE_C_COMPILER_ID}" STREQUAL "Intel" )
    # options for icc: also needs -std=c11
    set ( CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -diag-disable 10397,15552 " )
    set ( CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -qopt-report=5 -qopt-report-phase=vec" )
    # the -mp1 option is important for predictable floating-point results with
    # the icc compiler.  Without, ((float) 1.)/((float) 0.) produces NaN,
    # instead of the correct result, Inf.
    set ( CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -std=c11 -mp1" )
    # The -g option is useful for the Intel VTune tool, but it should be
    # removed in production.  Comment this line out if not in use:
    # set ( CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -g" )
#   set ( CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -qopt-malloc-options=3" )
    # check all warnings and remarks (uncomment for development only):
#   set ( CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -w3 -Wremarks -Werror " )
    if ( CMAKE_C_COMPILER_VERSION VERSION_LESS 19.0 )
        message ( FATAL_ERROR "icc version must be at least 19.0" )
    endif ( )
elseif ( "${CMAKE_C_COMPILER_ID}" STREQUAL "IntelLLVM" )
    # options for icx
elseif ( "${CMAKE_C_COMPILER_ID}" STREQUAL "Clang" )
    # options for clang
    set ( CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -Wno-pointer-sign " )
    if ( CMAKE_C_COMPILER_VERSION VERSION_LESS 3.3 )
        message ( FATAL_ERROR "clang version must be at least 3.3" )
    endif ( )
elseif ( "${CMAKE_C_COMPILER_ID}" STREQUAL "MSVC" )
    # options for MicroSoft Visual Studio
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /O2 -wd\"4244\" -wd\"4146\" -wd\"4018\" -wd\"4996\" -wd\"4047\" -wd\"4554\"")
endif ( )

if ( ${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    set ( CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_DEBUG}" )
else ( )
    set ( CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_RELEASE}")
endif ( )

#-------------------------------------------------------------------------------
# dynamic graphblas_renamed library properties
#-------------------------------------------------------------------------------

if ( GBCOMPACT )
    file ( GLOB GRAPHBLAS_SOURCES "../Source/*.c" "../Source/Generated1/*.c" )
else ( )
    file ( GLOB GRAPHBLAS_SOURCES "../Source/*.c" "../Source/Generated1/*.c" "../Source/Generated2/*.c" )
endif ( )

add_library ( graphblas_renamed SHARED ${GRAPHBLAS_SOURCES} )
SET_TARGET_PROPERTIES ( graphblas_renamed PROPERTIES
    VERSION ${GraphBLAS_VERSION_MAJOR}.${GraphBLAS_VERSION_MINOR}.${GraphBLAS_VERSION_SUB}
    SOVERSION ${GraphBLAS_VERSION_MAJOR}
    C_STANDARD_REQUIRED 11
    PUBLIC_HEADER "../Include/GraphBLAS.h" )
set_property ( TARGET graphblas_renamed PROPERTY C_STANDARD 11 )

#-------------------------------------------------------------------------------
# select the threading library
#-------------------------------------------------------------------------------

if ( OPENMP_FOUND )
    set ( USE_OPENMP true )
endif ( )

#-------------------------------------------------------------------------------
# select the math library (not required for Microsoft Visual Studio)
#-------------------------------------------------------------------------------

if ( "${CMAKE_C_COMPILER_ID}" STREQUAL "MSVC" )
    set ( M_LIB "" )
else ( )
    set ( M_LIB "m" )
endif ( )

target_link_libraries ( graphblas_renamed PUBLIC ${M_LIB} )

#-------------------------------------------------------------------------------
# add the OpenMP, CUDA, BLAS, ... libraries
#-------------------------------------------------------------------------------

if ( USE_OPENMP )
    message ( STATUS "CMAKE OpenMP libraries:    " ${OpenMP_C_LIBRARIES} )
    message ( STATUS "CMAKE OpenMP include:      " ${OpenMP_C_INCLUDE_DIRS} )
    target_link_libraries ( graphblas_renamed PUBLIC ${OpenMP_C_LIBRARIES} )
    set ( CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS} " )
    include_directories ( ${OpenMP_C_INCLUDE_DIRS} )
endif ( )

#-------------------------------------------------------------------------------
# cpu_features settings
#-------------------------------------------------------------------------------

if ( NOT GBNCPUFEAT )
    if ( UNIX )
        # look for requirements for cpu_features/src/hwcaps.c
        include ( CheckIncludeFile )
        include ( CheckSymbolExists )
        check_include_file ( dlfcn.h HAVE_DLFCN_H )
        if ( HAVE_DLFCN_H )
            target_compile_definitions ( graphblas_renamed PRIVATE HAVE_DLFCN_H )
            if ( BUILD_GRB_STATIC_LIBRARY )
                target_compile_definitions ( graphblas_static PRIVATE HAVE_DLFCN_H )
            endif ( )
        endif ( )
        check_symbol_exists ( getauxval "sys/auxv.h" HAVE_STRONG_GETAUXVAL )
        if ( HAVE_STRONG_GETAUXVAL )
            target_compile_definitions ( graphblas_renamed PRIVATE HAVE_STRONG_GETAUXVAL )
            if ( BUILD_GRB_STATIC_LIBRARY )
                target_compile_definitions ( graphblas_static PRIVATE HAVE_STRONG_GETAUXVAL )
            endif ( )
        endif ( )
    endif ( )
endif ( )

#-------------------------------------------------------------------------------
# print final C flags
#-------------------------------------------------------------------------------

message ( STATUS "CMAKE C flags: " ${CMAKE_C_FLAGS} )

#-------------------------------------------------------------------------------
# graphblas_renamed installation
#-------------------------------------------------------------------------------

install ( TARGETS graphblas_renamed
     LIBRARY       DESTINATION ${CMAKE_INSTALL_LIBDIR}
     PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
     ARCHIVE       DESTINATION ${CMAKE_INSTALL_LIBDIR} )

