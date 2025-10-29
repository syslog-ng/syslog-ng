cmake_minimum_required(VERSION 3.14)

# use, i.e. don't skip the full RPATH for the build tree
set(CMAKE_SKIP_BUILD_RPATH FALSE)

# when building, don't use the install RPATH already
# (but later on when installing)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
set(CMAKE_MACOSX_RPATH 1)

# add the automatically determined parts of the RPATH
# which point to directories outside the build tree to the install RPATH
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

project(syslog-ng C)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/Modules ${PROJECT_SOURCE_DIR}/cmake/)
set(CMAKE_POSITION_INDEPENDENT_CODE 1)

include(common_helpers)
include(CheckIncludeFiles)
include(CheckFunctionExists)
include(ExternalProject)
include(external_or_find_package)
include(add_module)
include(module_switch)
include(FindGperf)

include(enable_cpp)
module_switch(ENABLE_DARWIN_OSL "Enable OSLog" APPLE)
include(enable_objc)

include(init_submodules)

set(MEMORYCHECK_SUPPRESSIONS_FILE "${PROJECT_SOURCE_DIR}/tests/valgrind/unit-test-leak.supp")
set(MEMORYCHECK_COMMAND_OPTIONS "--num-callers=30 --sim-hints=no-nptl-pthread-stackcache --gen-suppressions=all --leak-check=full --trace-children=yes --freelist-vol=200000000 --freelist-big-blocks=10000000 --malloc-fill=55 --free-fill=AA")

execute_process(COMMAND ${PROJECT_SOURCE_DIR}/scripts/version.sh SET OUTPUT_VARIABLE SYSLOG_NG_VERSION)
set(SYSLOG_NG_COMBINED_VERSION ${SYSLOG_NG_VERSION})
set(SYSLOG_NG_SOURCE_REVISION ${SYSLOG_NG_VERSION})

set(SYSLOG_NG_PATH_PREFIX ${CMAKE_INSTALL_PREFIX})
set(SYSLOG_NG_PATH_SYSCONFDIR "\${prefix}/etc")
set(SYSLOG_NG_PATH_DATADIR "\${datarootdir}")
set(SYSLOG_NG_PATH_PKGDATADIR "\${datarootdir}/syslog-ng")
set(SYSLOG_NG_PATH_PIDFILEDIR "\${localstatedir}")
set(SYSLOG_NG_PATH_LOCALSTATEDIR "\${prefix}/var")
set(SYSLOG_NG_MODULE_PATH "\${exec_prefix}/lib/syslog-ng")
set(SYSLOG_NG_PATH_EXECPREFIX "\${prefix}")
set(SYSLOG_NG_PATH_CONFIG_INCLUDEDIR "\${datadir}/syslog-ng/include")
set(SYSLOG_NG_PATH_SCLDIR "\${datadir}/syslog-ng/include/scl")
set(SYSLOG_NG_PATH_LIBEXECDIR "\${exec_prefix}/libexec")
set(SYSLOG_NG_PATH_DATAROOTDIR "\${prefix}/share")
set(SYSLOG_NG_PATH_MODULEDIR "\${exec_prefix}/lib/syslog-ng")
set(SYSLOG_NG_PACKAGE_NAME "${CMAKE_PROJECT_NAME}")
set(SYSLOG_NG_PATH_XSDDIR "\${datadir}/syslog-ng/xsd")
set(SYSLOG_NG_JAVA_MODULE_PATH "\${exec_prefix}/lib/syslog-ng/java-modules")
set(SYSLOG_NG_PYTHON_MODULE_DIR "\${exec_prefix}/lib/syslog-ng/python")
set(SYSLOG_NG_PYTHON_SYSCONF_MODULE_DIR "\${prefix}/etc/python")
set(SYSLOG_NG_PYTHON_VENV_DIR "\${localstatedir}/python-venv")
set(SYSLOG_NG_PATH_TOPSRC_DIR "${PROJECT_SOURCE_DIR}")
set(LIBDIR "\${exec_prefix}/lib")
set(INCLUDEDIR "\${prefix}/include")
set(TOOLSDIR "\${datadir}/syslog-ng/tools")

set(LOGGEN_PLUGIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/lib/syslog-ng/loggen")

include(set_debug_options)

include(CheckTypeSize)
include(CheckStructMember)
include(CheckSymbolExists)
include(GenerateYFromYm)
include(CheckStructHasMember)
include(CheckCSourceCompiles)
include(CheckSockaddrStorage)
include(CheckCreds)
include(CheckTCP)


set(CMAKE_C_STANDARD 99)

# Find only hard dependencies here directly to prevent hard linking of optional dependencies
include(find_bison)
include(find_glib)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(FLEX REQUIRED)
find_package(Resolv REQUIRED)
find_package(WRAP)
external_or_find_package(JSONC REQUIRED)
include(find_python)
include(find_ivykis)
pkg_check_modules(LIBPCRE REQUIRED libpcre2-8)

include(detect_features)
include(platform_options)

include(EnableIPv6)
include(enable_systemd)
include(enable_spoof_source)
include(enable_caps)
include(enable_wrap)
include(enable_stackdump)
include(enable_env_wrap)
include(enable_server_mode)

option(WITH_COMPILE_DATE "Include compile date in binary" ON)
if(WITH_COMPILE_DATE)
  set(SYSLOG_NG_WITH_COMPILE_DATE 1)
endif()

# Let the user specify a timezone directory
set(TIMEZONE_DIR "" CACHE PATH "Use path as the directory to get the timezone files")
if(TIMEZONE_DIR)
  set(PATH_TIMEZONEDIR "${TIMEZONE_DIR}")
endif()

set(Eventlog_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib/eventlog/src")

add_custom_target(style-check COMMAND ${PROJECT_SOURCE_DIR}/scripts/style-checker.sh check ${PROJECT_SOURCE_DIR} ${PROJECT_BINARY_DIR})
add_custom_target(style-format COMMAND ${PROJECT_SOURCE_DIR}/scripts/style-checker.sh format ${PROJECT_SOURCE_DIR} ${PROJECT_BINARY_DIR})
add_custom_target(check-commits COMMAND ${PROJECT_SOURCE_DIR}/tests/commits/check.sh WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
add_custom_target(check-copyright COMMAND ${PROJECT_SOURCE_DIR}/tests/copyright/check.sh . ${PROJECT_BINARY_DIR} policy WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
set_target_properties(check-copyright PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "copyright-run.log;copyright-err.log")

set(IMPORTANT_WARNINGS
  -Wshadow)

set(ACCEPTABLE_WARNINGS
  -Wno-stack-protector
  -Wno-unused-parameter
  -Wno-variadic-macros)

option(ENABLE_EXTRA_WARNINGS "Enable extra warnings" ON)

if(ENABLE_EXTRA_WARNINGS)
  set(EXTRA_WARNINGS
    $<$<COMPILE_LANGUAGE:C>:-Wimplicit-function-declaration>
    $<$<COMPILE_LANGUAGE:C>:-Wnested-externs>
    $<$<COMPILE_LANGUAGE:C>:-Wstrict-prototypes>
    -Wswitch-default
    $<$<COMPILE_LANGUAGE:C>:-Wimplicit-int>
    -Wall
    -Wuninitialized
    -Wdeprecated
    -Wdeprecated-declarations
    -Woverflow
    -Wdouble-promotion
    -Wfloat-equal
    -Wpointer-arith
    $<$<COMPILE_LANGUAGE:C>:-Wpointer-sign>
    -Wmissing-format-attribute
    $<$<COMPILE_LANGUAGE:C>:-Wold-style-definition>
    -Wundef
    -Wignored-qualifiers
    -Wfloat-conversion
    $<$<COMPILE_LANGUAGE:C>:-Wbad-function-cast>)

  if("${CMAKE_C_COMPILER_ID}" MATCHES "Clang")
    set(EXTRA_WARNINGS
      ${EXTRA_WARNINGS}
    )
  else()
    set(EXTRA_WARNINGS
      $<$<COMPILE_LANGUAGE:C>:-Wold-style-declaration>
      -Wunused-but-set-parameter
      $<$<COMPILE_LANGUAGE:C>:-Woverride-init>
      ${EXTRA_WARNINGS}
    )
  endif()
endif()

add_compile_options(${IMPORTANT_WARNINGS} ${ACCEPTABLE_WARNINGS} ${EXTRA_WARNINGS})

option(ENABLE_FORCE_GNU99 "Enforce C99 with gnu extensions" OFF)

if(ENABLE_FORCE_GNU99)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")
endif()
include(set_testing)

include(set_diagnostic_options)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(Mk)
add_subdirectory(scl)
add_subdirectory(lib)
add_subdirectory(modules)
add_subdirectory(scripts)
add_subdirectory(syslog-ng)
add_subdirectory(syslog-ng-ctl)
add_subdirectory(persist-tool)
add_subdirectory(doc)
add_subdirectory(tests)
add_subdirectory(libtest)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/syslog-ng-config.h.in ${CMAKE_CURRENT_BINARY_DIR}/syslog-ng-config.h)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/syslog-ng-config.h DESTINATION include/syslog-ng)

install(DIRECTORY DESTINATION var)

# Package Information for pkg-config
set(PKG_CONFIG_EXEC_PREFIX ${SYSLOG_NG_PATH_EXECPREFIX})
set(PKG_CONFIG_DATAROOTDIR ${SYSLOG_NG_PATH_DATAROOTDIR})
set(PKG_CONFIG_DATADIR ${SYSLOG_NG_PATH_DATAROOTDIR})
set(PKG_CONFIG_LIBDIR ${LIBDIR})
set(PKG_CONFIG_INCLUDEDIR ${INCLUDEDIR})
set(PKG_CONFIG_TOOLSDIR ${TOOLSDIR})
set(PKG_CONFIG_MODULEDIR ${SYSLOG_NG_PATH_MODULEDIR})
set(PKG_CONFIG_CONFIG_INCLUDEDIR ${SYSLOG_NG_PATH_CONFIG_INCLUDEDIR})
set(PKG_CONFIG_SCLDIR ${SYSLOG_NG_PATH_SCLDIR})
set(PKG_CONFIG_IVYKIS ${IVYKIS_INCLUDE_DIRS})
set(PKG_CONFIG_INTERNAL_IVYKIS_CFLAGS ${IVYKIS_INCLUDE_DIRS})
set(PKG_CONFIG_PACKAGE_VERSION ${SYSLOG_NG_VERSION})
set(libdir "\${libdir}")
set(includedir "\${includedir}")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/syslog-ng.pc.cmake ${CMAKE_CURRENT_BINARY_DIR}/syslog-ng.pc)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/syslog-ng.pc DESTINATION lib/pkgconfig)

include(print_config_summary)
