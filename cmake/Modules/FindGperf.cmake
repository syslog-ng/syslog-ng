# SPDX-FileCopyrightText: 2016-2017 Pino Toscano <pino@kde.org>
#
# SPDX-License-Identifier: BSD-3-Clause
#
# Adapted from https://invent.kde.org/frameworks/extra-cmake-modules/-/blob/master/find-modules/FindGperf.cmake?ref_type=heads
#

find_program(Gperf_EXECUTABLE NAMES gperf)

if (Gperf_EXECUTABLE)
    execute_process(COMMAND ${Gperf_EXECUTABLE} -v
        OUTPUT_VARIABLE _version_string
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(_version_string MATCHES "^GNU gperf ([-0-9\\.]+)")
        set(Gperf_VERSION "${CMAKE_MATCH_1}")
    endif()
    unset(_version_string)
else()
    set(Gperf_VERSION)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Gperf
    FOUND_VAR
        Gperf_FOUND
    REQUIRED_VARS
        Gperf_EXECUTABLE
    VERSION_VAR
        Gperf_VERSION
)

if (Gperf_FOUND)
    if (NOT TARGET GPerf::Gperf)
        add_executable(GPerf::Gperf IMPORTED)
        set_target_properties(GPerf::Gperf PROPERTIES
            IMPORTED_LOCATION "${Gperf_EXECUTABLE}"
        )
    endif()
endif()

function(gperf_generate input_file output_file _target_or_sources_var)
    # Parse arguments
    set(oneValueArgs GENERATION_FLAGS)
    cmake_parse_arguments(ARGS "" "${oneValueArgs}" "" ${ARGN})

    if(ARGS_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unknown keywords given to gperf_generate(): \"${ARGS_UNPARSED_ARGUMENTS}\"")
    endif()
    if (TARGET ${_target_or_sources_var})
        get_target_property(aliased_target ${_target_or_sources_var} ALIASED_TARGET)
        if(aliased_target)
            message(FATAL_ERROR "Target argument passed to gperf_generate must not be an alias: ${_target_or_sources_var}")
        endif()
    endif()

    get_filename_component(_infile ${input_file} ABSOLUTE)
    set(_extraopts "${ARGS_GENERATION_FLAGS}")
    separate_arguments(_extraopts)
    add_custom_command(OUTPUT ${output_file}
        COMMAND ${Gperf_EXECUTABLE} ${_extraopts} --output-file=${output_file} ${_infile}
        DEPENDS ${_infile}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        VERBATIM
    )
    set_property(SOURCE ${output_file} PROPERTY SKIP_AUTOMOC ON)

    if (TARGET ${_target_or_sources_var})
        target_sources(${_target_or_sources_var} PRIVATE ${output_file})
    else()
        set(${_target_or_sources_var} ${${_target_or_sources_var}} ${output_file} PARENT_SCOPE)
    endif()
endfunction()
