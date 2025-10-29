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

include(EnableIPv6)

option(WITH_COMPILE_DATE "Include compile date in binary" ON)

if(WITH_COMPILE_DATE)
  set(SYSLOG_NG_WITH_COMPILE_DATE 1)
endif()

set(CMAKE_REQUIRED_DEFINITIONS "-D_LARGEFILE64_SOURCE=1")
set(CMAKE_EXTRA_INCLUDE_FILES "fcntl.h")
check_type_size(O_LARGEFILE O_LARGEFILE)

if(HAVE_O_LARGEFILE)
  set(SYSLOG_NG_HAVE_O_LARGEFILE 1)
endif()

check_c_source_compiles("
#include <pthread.h>
__thread int a; int main() { a=0; }" SYSLOG_NG_HAVE_THREAD_KEYWORD)

check_symbol_exists(strtoll stdlib.h SYSLOG_NG_HAVE_STRTOLL)
check_symbol_exists(strnlen string.h SYSLOG_NG_HAVE_STRNLEN)
check_symbol_exists(getline "stdio.h" SYSLOG_NG_HAVE_GETLINE)
check_symbol_exists(strtok_r string.h SYSLOG_NG_HAVE_STRTOK_R)
check_symbol_exists(strtoimax inttypes.h SYSLOG_NG_HAVE_STRTOIMAX)
check_symbol_exists(inet_aton "sys/socket.h;netinet/in.h;arpa/inet.h" SYSLOG_NG_HAVE_INET_ATON)
check_symbol_exists(inet_ntoa "sys/socket.h;netinet/in.h;arpa/inet.h" SYSLOG_NG_HAVE_INET_NTOA)
check_symbol_exists(getutent utmp.h SYSLOG_NG_HAVE_GETUTENT)
check_symbol_exists(getutxent utmpx.h SYSLOG_NG_HAVE_GETUTXENT)
check_symbol_exists(getaddrinfo "netdb.h;sys/socket.h;sys/types.h" SYSLOG_NG_HAVE_GETADDRINFO)
check_symbol_exists(getnameinfo "netdb.h;sys/socket.h" SYSLOG_NG_HAVE_GETNAMEINFO)
check_symbol_exists(clock_gettime "time.h" SYSLOG_NG_HAVE_CLOCK_GETTIME)
check_symbol_exists(gmtime_r "time.h" SYSLOG_NG_HAVE_GMTIME_R)
check_symbol_exists(localtime_r "time.h" SYSLOG_NG_HAVE_LOCALTIME_R)
check_symbol_exists("getrandom" "sys/random.h" SYSLOG_NG_HAVE_GETRANDOM)
check_symbol_exists(fmemopen "stdio.h" SYSLOG_NG_HAVE_FMEMOPEN)
set(CMAKE_REQUIRED_DEFINITIONS "-D_GNU_SOURCE=1")
check_symbol_exists(memrchr "string.h" SYSLOG_NG_HAVE_MEMRCHR)
check_symbol_exists(strcasestr "string.h" SYSLOG_NG_HAVE_STRCASESTR)
check_symbol_exists(pread "unistd.h" SYSLOG_NG_HAVE_PREAD)
check_symbol_exists(pwrite "unistd.h" SYSLOG_NG_HAVE_PWRITE)
check_symbol_exists(posix_fallocate "fcntl.h" SYSLOG_NG_HAVE_POSIX_FALLOCATE)
check_symbol_exists(timezone time.h SYSLOG_NG_HAVE_TIMEZONE)

check_include_files(utmp.h SYSLOG_NG_HAVE_UTMP_H)
check_include_files(utmpx.h SYSLOG_NG_HAVE_UTMPX_H)
check_include_files(dlfcn.h SYSLOG_NG_HAVE_DLFCN_H)
check_include_files(getopt.h SYSLOG_NG_HAVE_GETOPT_H)
check_include_files(door.h SYSLOG_NG_HAVE_DOOR_H)

check_struct_has_member("struct utmpx" "ut_type" "utmpx.h" UTMPX_HAS_UT_TYPE LANGUAGE C)
check_struct_has_member("struct utmp" "ut_type" "utmp.h" UTMP_HAS_UT_TYPE LANGUAGE C)
check_struct_has_member("struct utmpx" "ut_user" "utmpx.h" UTMPX_HAS_UT_USER LANGUAGE C)
check_struct_has_member("struct utmp" "ut_user" "utmp.h" UTMP_HAS_UT_USER LANGUAGE C)

# AIX has issues detecting getprotobynumber_r
check_c_source_compiles("
#include <netdb.h>
int main(void) {
    struct protoent result;
    struct protoent *res = 0;
    char buf[1024];
    getprotobynumber_r(6, &result, buf, sizeof(buf), &res);
    return 0;
}
" SYSLOG_NG_HAVE_GETPROTOBYNUMBER_R)

# AIX has issues detecting struct tm.tm_gmtoff
check_c_source_compiles("
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
int main(void) {
    struct tm t;
    (void) t.tm_gmtoff; /* try to actually use it */
    return 0;
}
" SYSLOG_NG_HAVE_STRUCT_TM_TM_GMTOFF)

if((UTMPX_HAS_UT_TYPE AND UTMPX_HAS_UT_USER) OR(UTMPX_HAS_UT_TYPE AND UTMP_HAS_UT_USER))
  set(SYSLOG_NG_HAVE_MODERN_UTMP 1)
endif()

check_symbol_exists(SO_MEMINFO "sys/socket.h" SYSLOG_NG_HAVE_SO_MEMINFO)
check_include_file("linux/sock_diag.h" SYSLOG_NG_HAVE_LINUX_SOCK_DIAG_H)
set(SYSLOG_NG_ENABLE_AFSOCKET_MEMINFO_METRICS ${SYSLOG_NG_HAVE_SO_MEMINFO})

if(NOT DEFINED SYSLOG_NG_ENABLE_AFSOCKET_MEMINFO_METRICS)
  set(SYSLOG_NG_ENABLE_AFSOCKET_MEMINFO_METRICS OFF)
endif()

set(CMAKE_C_STANDARD 99)

find_package(GLIB 2.32.0 REQUIRED COMPONENTS gmodule gthread)

set(CMAKE_REQUIRED_INCLUDES ${GLIB_INCLUDE_DIRS})
set(CMAKE_REQUIRED_LIBRARIES ${GLIB_LIBRARIES})
check_symbol_exists(g_list_copy_deep "glib.h" SYSLOG_NG_HAVE_G_LIST_COPY_DEEP)
check_symbol_exists(g_memdup2 "glib.h" SYSLOG_NG_HAVE_MEMDUP2)
check_symbol_exists(g_ptr_array_find_with_equal_func "glib.h" SYSLOG_NG_HAVE_G_PTR_ARRAY_FIND_WITH_EQUAL_FUNC)
check_symbol_exists(g_canonicalize_filename "glib.h" SYSLOG_NG_HAVE_G_CANONICALIZE_FILENAME)

# only set MIN_REQUIRED to 2.68 if we know that we are at least on that version.
# On CentOS7/SLES12 we don't have recent enough glib but with compat
# wrappers we can compile. Bad news is that if we don't set to 2.68 at least,
# we can't compile on new platforms with clang.
#
# Best of both worlds: we set to 2.68 if we have that version, so we
# get clang to compile our code, otherwise we set it to 2.32.
if(PC_GLIB_VERSION VERSION_GREATER_EQUAL "2.68")
  set_target_properties(GLib::GLib PROPERTIES
    INTERFACE_COMPILE_DEFINITIONS "GLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_68")
else()
  set_target_properties(GLib::GLib PROPERTIES
    INTERFACE_COMPILE_DEFINITIONS "GLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_32")
endif()

set(SYSLOG_NG_USE_CONST_IVYKIS_MOCK 1)
set(IVYKIS_SOURCE "internal" CACHE STRING "Select ivykis source (internal/system/path/AUTO)")
external_or_find_package(IVYKIS REQUIRED)
set(IVYKIS_LIBRARIES ${IVYKIS_LIBRARY} CACHE STRING "IVYKIS libraries path")

if((NOT IVYKIS_INTERNAL) AND(IVYKIS_PKGCONF_VERSION VERSION_LESS "0.39"))
  set(SYSLOG_NG_USE_CONST_IVYKIS_MOCK 0)
elseif(IVYKIS_INTERNAL)
  # internal ivykis is linked statically, io_uring might be available and linked against too, we have to use it as well
  find_package(uring)

  if(URING_FOUND)
    set(IVYKIS_LIBRARIES ${IVYKIS_LIBRARIES} ${URING_LIBRARIES})
  endif()
endif()

external_or_find_package(JSONC REQUIRED)

# Find only hard dependencies here directly to prevent hard linking of optional dependencies
include(find_bison)
find_package(OpenSSL REQUIRED)
find_package(FLEX REQUIRED)
find_package(Resolv REQUIRED)
find_package(WRAP)

include(enable_systemd)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Use only one of them if both are present, like on FreeBSD, where the inotify wrapper started to be distributed.
# Prioritize kqueue, as inotify is just a wrapper around it if both are present.
check_function_exists(kqueue SYSLOG_NG_HAVE_KQUEUE)

if(NOT SYSLOG_NG_HAVE_KQUEUE)
  find_package(Inotify)

  if(Inotify_FOUND)
    set(SYSLOG_NG_HAVE_INOTIFY 1)
  endif()
endif()

set(PYTHON_VERSION "AUTO" CACHE STRING "Version of the installed development library")

if("${PYTHON_VERSION}" MATCHES "^(auto|AUTO)$")
  find_package(PythonInterp EXACT 3)
  find_package(PythonLibs EXACT 3)
else()
  find_package(PythonInterp EXACT "${PYTHON_VERSION}" REQUIRED)
  find_package(PythonLibs EXACT "${PYTHON_VERSION}" REQUIRED)
endif()

MESSAGE(STATUS "Detected pythonlib version: ${PYTHONLIBS_VERSION_STRING}")
include(python_build_venv)

include(openssl_functions)
openssl_set_defines()
include(platform_options)

pkg_check_modules(LIBPCRE REQUIRED libpcre2-8)
include(enable_caps)
include(enable_wrap)
include(enable_env_wrap)
include(enable_server_mode)

endif()

# Let the user specify a timezone directory
set(TIMEZONE_DIR "" CACHE PATH "Use path as the directory to get the timezone files")

if(TIMEZONE_DIR)
  set(PATH_TIMEZONEDIR "${TIMEZONE_DIR}")
endif()

include(enable_spoof_source)


include(enable_stackdump)

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
