#############################################################################
# Copyright (c) 2023 Attila Szakacs
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#
# As an additional exemption you are allowed to compile & link against the
# OpenSSL libraries as published by the OpenSSL project. See the file
# COPYING for details.
#
#############################################################################

include(CMakeParseArguments)


get_property(_protoc_path TARGET protobuf::protoc PROPERTY IMPORTED_LOCATION)
execute_process(COMMAND ${_protoc_path} --version OUTPUT_VARIABLE _protoc_version)

if("${_protoc_version}" MATCHES "libprotoc ([0-9.]+)")
  set(_protoc_version "${CMAKE_MATCH_1}")
endif()

if("${_protoc_version}" VERSION_LESS "3.15.0")
  set_property(GLOBAL PROPERTY PROTOC_EXTRA_FLAGS "--experimental_allow_proto3_optional")
endif()


function(_protobuf_generate_cpp_internal)
  cmake_parse_arguments(_protobuf_generate_cpp_internal "" "PROTO_PATH;CPP_OUT;OUT_SRCS" "PROTOS;EXTRA_EXT;EXTRA_PROTOC_FLAGS;EXTRA_DEPENDS" "${ARGN}")

  get_property(_protoc_extra_flags GLOBAL PROPERTY PROTOC_EXTRA_FLAGS)
  file(MAKE_DIRECTORY ${_protobuf_generate_cpp_internal_CPP_OUT})
  set(_generated_srcs_all)
  foreach(_proto ${_protobuf_generate_cpp_internal_PROTOS})
    get_filename_component(_abs_file ${_proto} ABSOLUTE)
    get_filename_component(_abs_dir ${_abs_file} DIRECTORY)
    get_filename_component(_basename ${_proto} NAME_WLE)
    file(RELATIVE_PATH _rel_dir ${CMAKE_CURRENT_SOURCE_DIR} ${_abs_dir})

    set(_generated_srcs)
    foreach(_ext .pb.h .pb.cc ${_protobuf_generate_cpp_internal_EXTRA_EXT})
      list(APPEND _generated_srcs "${_protobuf_generate_cpp_internal_CPP_OUT}/${_rel_dir}/${_basename}${_ext}")
    endforeach()

    add_custom_command(
      OUTPUT ${_generated_srcs}
      COMMAND protobuf::protoc
      ARGS
        ${_protobuf_generate_cpp_internal_EXTRA_PROTOC_FLAGS}
        --proto_path=${_protobuf_generate_cpp_internal_PROTO_PATH}
        --cpp_out=${_protobuf_generate_cpp_internal_CPP_OUT}
        ${_protoc_extra_flags}
        ${_proto}
      DEPENDS protobuf::protoc ${_protobuf_generate_cpp_internal_EXTRA_DEPENDS}
      COMMENT "Running C++ protocol buffer compiler on ${_proto}"
      VERBATIM)
    set_source_files_properties(${_generated_srcs} PROPERTIES GENERATED TRUE)
    list(APPEND _generated_srcs_all ${_generated_srcs})
  endforeach()
  set(${_protobuf_generate_cpp_internal_OUT_SRCS} ${_generated_srcs_all} PARENT_SCOPE)
endfunction()


function(protobuf_generate_cpp)
  cmake_parse_arguments(protobuf_generate_cpp "" "PROTO_PATH;CPP_OUT;OUT_SRCS" "PROTOS" "${ARGN}")

  _protobuf_generate_cpp_internal(
    PROTO_PATH ${protobuf_generate_cpp_PROTO_PATH}
    CPP_OUT ${protobuf_generate_cpp_CPP_OUT}
    OUT_SRCS _out_srcs
    PROTOS ${protobuf_generate_cpp_PROTOS})

  set(${protobuf_generate_cpp_OUT_SRCS} ${_out_srcs} PARENT_SCOPE)
endfunction()


function(protobuf_generate_cpp_grpc)
  cmake_parse_arguments(protobuf_generate_cpp_grpc "" "PROTO_PATH;CPP_OUT;OUT_SRCS" "PROTOS" "${ARGN}")

  get_property(_grpc_cpp_plugin_path TARGET gRPC::grpc_cpp_plugin PROPERTY IMPORTED_LOCATION)

  _protobuf_generate_cpp_internal(
    PROTO_PATH ${protobuf_generate_cpp_grpc_PROTO_PATH}
    CPP_OUT ${protobuf_generate_cpp_grpc_CPP_OUT}
    OUT_SRCS _out_srcs
    PROTOS ${protobuf_generate_cpp_grpc_PROTOS}
    EXTRA_EXT .grpc.pb.cc .grpc.pb.h
    EXTRA_PROTOC_FLAGS
      --plugin=protoc-gen-grpc-cpp=${_grpc_cpp_plugin_path}
      --grpc-cpp_out=${protobuf_generate_cpp_grpc_CPP_OUT}
    EXTRA_DEPENDS gRPC::grpc_cpp_plugin)

  set(${protobuf_generate_cpp_grpc_OUT_SRCS} ${_out_srcs} PARENT_SCOPE)
endfunction()
