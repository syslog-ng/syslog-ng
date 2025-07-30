cmake_minimum_required(VERSION 3.14)

# use, i.e. don't skip the full RPATH for the build tree
set(CMAKE_SKIP_BUILD_RPATH  FALSE)

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
set(BISON_FLAGS "-Wno-other -Werror=conflicts-sr -Werror=conflicts-rr -Wcounterexamples")
set(BISON_BUILT_SOURCE_CFLAGS "-Wno-unused-but-set-variable")

include(CheckIncludeFiles)
include(CheckFunctionExists)
include(ExternalProject)
include(external_or_find_package)
include(add_module)
include(module_switch)
include(FindGperf)

option(ENABLE_CPP "Enable C++" ON)
if (ENABLE_CPP)
  enable_language(CXX)
  set(CMAKE_CXX_STANDARD 14 CACHE STRING "C++ standard")
  set(CMAKE_CXX_STANDARD_REQUIRED ON CACHE BOOL "C++ standard is a requirement")
  set(CMAKE_CXX_EXTENSIONS OFF CACHE BOOL "C++ compiler specific extensions")
  set(SYSLOG_NG_ENABLE_CPP ${ENABLE_CPP})
endif()

if(APPLE)
  module_switch(ENABLE_DARWIN_OSL "Enable OSLog" ON)
  option(ENABLE_OBJC "Enable ObjC" ${ENABLE_DARWIN_OSL})
endif()

if(ENABLE_OBJC)
  # Turned out that even the latest gcc has no proper support of the required ObjC version
  # Once gcc was able to compile our ObjC modules properly this requirement could be removed again
  set(CMAKE_OBJC_COMPILER clang)

  set_property(GLOBAL PROPERTY OBJC_STANDARD 11)
  set_property(GLOBAL PROPERTY OBJC_STANDARD_REQUIRED ON)
endif()

# FIXME: This does not handle multiple sub-modules, must be made recursive and smarter later
#        Also it should be moved to a separate file
#        We have only one such module now (ivykis), so it is not a big problem currently
if("${IVYKIS_SOURCE}" STREQUAL "internal" OR "${IVYKIS_SOURCE}}" STREQUAL "auto" OR "${IVYKIS_SOURCE}}" STREQUAL "AUTO")
  find_package(Git QUIET)

  if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
    option(GIT_SUBMODULE "Checkout submodules during build" ON)

    if(GIT_SUBMODULE)
      message(STATUS "Submodule update")
      execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        RESULT_VARIABLE GIT_SUBMOD_RESULT)

      if(NOT GIT_SUBMOD_RESULT EQUAL "0")
        message(FATAL_ERROR "git submodule update --init --recursive failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
      endif()
    endif()
  endif()
endif()

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

set (LOGGEN_PLUGIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/lib/syslog-ng/loggen")

IF(CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES RelWithDebInfo)
  set(SYSLOG_NG_ENABLE_DEBUG ON)
  add_definitions(-DYYDEBUG=1)
ELSE()
  set(SYSLOG_NG_ENABLE_DEBUG OFF)
ENDIF()

include(CheckTypeSize)
include(CheckStructMember)
include(CheckSymbolExists)
include(GenerateYFromYm)
include(CheckStructHasMember)
include(CheckCSourceCompiles)

add_definitions(-D_GNU_SOURCE=1)
add_definitions(-D_LARGEFILE64_SOURCE=1)
add_definitions(-D__APPLE_USE_RFC_3542)


include(CheckSockaddrStorage)
if (HAVE_STRUCT_SOCKADDR_STORAGE)
  set(SYSLOG_NG_HAVE_STRUCT_SOCKADDR_STORAGE 1)
else()
  set(SYSLOG_NG_HAVE_STRUCT_SOCKADDR_STORAGE 0)
endif()

include(CheckCreds)
set(SYSLOG_NG_HAVE_STRUCT_UCRED (HAVE_STRUCT_UCRED OR HAVE_STRUCT_CMSGCRED))
check_struct_member ("struct msghdr" "msg_control" "sys/types.h;sys/socket.h" SYSLOG_NG_HAVE_CTRLBUF_IN_MSGHDR)

include(CheckIPv6)
module_switch(ENABLE_IPV6 "Enable IPv6" HAVE_IPV6)
set(SYSLOG_NG_ENABLE_IPV6 ${ENABLE_IPV6})

check_symbol_exists(TCP_KEEPIDLE  "sys/socket.h;netinet/in.h;netinet/tcp.h" HAVE_TCP_KEEPIDLE)
check_symbol_exists(TCP_KEEPINTVL "sys/socket.h;netinet/in.h;netinet/tcp.h" HAVE_TCP_KEEPINTVL)
check_symbol_exists(TCP_KEEPCNT   "sys/socket.h;netinet/in.h;netinet/tcp.h" HAVE_TCP_KEEPCNT)
if (HAVE_TCP_KEEPIDLE AND HAVE_TCP_KEEPINTVL AND HAVE_TCP_KEEPCNT)
  set(SYSLOG_NG_HAVE_TCP_KEEPALIVE_TIMERS 1)
endif()

if(NOT DEFINED ENABLE_FORCED_SERVER_MODE)
  set(ENABLE_FORCED_SERVER_MODE ON)
endif()
set(SYSLOG_NG_ENABLE_FORCED_SERVER_MODE ${ENABLE_FORCED_SERVER_MODE})

option(ENV_LD_LIBRARY_PATH "Set LD_LIBRARY_PATH during runtime to the value given" "")
if(NOT DEFINED ENABLE_ENV_WRAPPER)
  set (ENABLE_ENV_WRAPPER OFF)
  if (ENV_LD_LIBRARY_PATH)
    set (ENABLE_ENV_WRAPPER ON)
  endif ()
endif ()
set(SYSLOG_NG_ENABLE_ENV_WRAPPER ${ENABLE_ENV_WRAPPER})

# NOTE: This is now seems to be an Apple/Xcode "only" issue, but probably much better a clang one, so later we might want to add this globally
if (APPLE)
  option(FORCE_CLASSIC_LINKING "Enable classic linking" OFF)
  if (FORCE_CLASSIC_LINKING)
    # XCode15 new linker has some issues (e.g. https://developer.apple.com/forums/thread/737707)
    # switch back to classic linking till not fixed (https://developer.apple.com/documentation/xcode-release-notes/xcode-15-release-notes#Linking)
    SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-ld_classic")
  endif()
endif()

option(WITH_COMPILE_DATE "Include compile date in binary" ON)
if (WITH_COMPILE_DATE)
  set(SYSLOG_NG_WITH_COMPILE_DATE 1)
endif()

set(CMAKE_REQUIRED_DEFINITIONS "-D_LARGEFILE64_SOURCE=1")
set(CMAKE_EXTRA_INCLUDE_FILES "fcntl.h")
check_type_size(O_LARGEFILE O_LARGEFILE)
if (HAVE_O_LARGEFILE)
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

if ((UTMPX_HAS_UT_TYPE AND UTMPX_HAS_UT_USER) OR (UTMPX_HAS_UT_TYPE AND UTMP_HAS_UT_USER))
  set(SYSLOG_NG_HAVE_MODERN_UTMP 1)
endif()

check_symbol_exists(SO_MEMINFO "sys/socket.h" SYSLOG_NG_HAVE_SO_MEMINFO)
check_include_file("linux/sock_diag.h" SYSLOG_NG_HAVE_LINUX_SOCK_DIAG_H)
set (SYSLOG_NG_ENABLE_AFSOCKET_MEMINFO_METRICS ${SYSLOG_NG_HAVE_SO_MEMINFO})
if (NOT DEFINED SYSLOG_NG_ENABLE_AFSOCKET_MEMINFO_METRICS)
  set (SYSLOG_NG_ENABLE_AFSOCKET_MEMINFO_METRICS OFF)
endif()

set(WITH_GETTEXT "" CACHE STRING "Set the prefix where gettext is installed (e.g. /usr)")

set(CMAKE_C_STANDARD 99)

find_package(BISON 3.7.6 REQUIRED)

string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\..*$" _dummy "${BISON_VERSION}")

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
if (PC_GLIB_VERSION VERSION_GREATER_EQUAL "2.68")

  set_target_properties(GLib::GLib PROPERTIES
        INTERFACE_COMPILE_DEFINITIONS "GLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_68")

else()
  set_target_properties(GLib::GLib PROPERTIES
        INTERFACE_COMPILE_DEFINITIONS "GLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_32")
endif()

find_package(RabbitMQ)
if (RabbitMQ_FOUND)
  set(CMAKE_REQUIRED_INCLUDES ${RabbitMQ_INCLUDE_DIR})
  set(CMAKE_REQUIRED_LIBRARIES ${RabbitMQ_LIBRARY})
  check_include_file("rabbitmq-c/tcp_socket.h" SYSLOG_NG_HAVE_RABBITMQ_C_TCP_SOCKET_H)
  check_symbol_exists(amqp_ssl_socket_set_verify_peer "amqp.h;amqp_ssl_socket.h" SYSLOG_NG_HAVE_AMQP_SSL_SOCKET_SET_VERIFY_PEER)
endif()


set (SYSLOG_NG_USE_CONST_IVYKIS_MOCK 1)
set (IVYKIS_SOURCE "internal" CACHE STRING "Select ivykis source (internal/system/path/AUTO)")
external_or_find_package(IVYKIS REQUIRED)
set(IVYKIS_LIBRARIES ${IVYKIS_LIBRARY} CACHE STRING "IVYKIS libraries path")
if((NOT IVYKIS_INTERNAL) AND (IVYKIS_PKGCONF_VERSION VERSION_LESS "0.39"))
  set (SYSLOG_NG_USE_CONST_IVYKIS_MOCK 0)
elseif(IVYKIS_INTERNAL)
  # internal ivykis is linked statically, io_uring might be available and linked against too, we have to use it as well
  find_package(uring)
  if(URING_FOUND)
    set(IVYKIS_LIBRARIES ${IVYKIS_LIBRARIES} ${URING_LIBRARIES})
  endif()
endif()

external_or_find_package(JSONC REQUIRED)

find_package(OpenSSL REQUIRED)
find_package(FLEX REQUIRED)
find_package(Resolv REQUIRED)
find_package(WRAP)
find_package(Inotify)
find_package(LIBCAP)
find_package(LIBNET)

find_package(systemd)
pkg_search_module(SYSTEMD_WITH_NAMESPACE libsystemd>=245)
message(STATUS "Found lib${SYSTEMD_WITH_NAMESPACE_LIBRARIES} version ${SYSTEMD_WITH_NAMESPACE_VERSION}")

module_switch(ENABLE_JOURNALD "Enable systemd-journal" Libsystemd_FOUND)
set(WITH_SYSTEMD_JOURNAL "system" CACHE STRING "Link against the system supplied or the wrapper library")
set_property(CACHE WITH_SYSTEMD_JOURNAL PROPERTY STRINGS system wrapper)

if (ENABLE_JOURNALD)
  if (WITH_SYSTEMD_JOURNAL STREQUAL "system" AND NOT Libsystemd_FOUND)
    message(FATAL_ERROR "systemd library not found")
  endif()

  # TODO: Add the missing configure option handling like it has for autotools
  if(DEFINED ENABLE_SYSTEMD)
    message(FATAL_ERROR "ENABLE_SYSTEMD is defined, but the cmake build system does not support it currently.")
  endif()
  set(ENABLE_SYSTEMD ON)
  if (WITH_SYSTEMD_JOURNAL STREQUAL "system")
    if (SYSTEMD_WITH_NAMESPACE_LIBRARIES STREQUAL "systemd")
       set(SYSLOG_NG_HAVE_JOURNAL_NAMESPACES 1)
       message(STATUS "Have journal namespaces")
    else()
       set(SYSLOG_NG_HAVE_JOURNAL_NAMESPACES 0)
    endif()
    set(SYSLOG_NG_SYSTEMD_JOURNAL_MODE SYSLOG_NG_JOURNALD_SYSTEM)
  elseif (WITH_SYSTEMD_JOURNAL STREQUAL "wrapper")
    set(SYSLOG_NG_SYSTEMD_JOURNAL_MODE SYSLOG_NG_JOURNALD_OPTIONAL)
  endif()
else()
  set(ENABLE_SYSTEMD OFF)
  set(SYSLOG_NG_SYSTEMD_JOURNAL_MODE SYSLOG_NG_JOURNALD_OFF)
endif()
set (SYSLOG_NG_ENABLE_SYSTEMD ${ENABLE_SYSTEMD})

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
set(SYSLOG_NG_HAVE_INOTIFY "${Inotify_FOUND}")
check_function_exists(kqueue SYSLOG_NG_HAVE_KQUEUE)

set (PYTHON_VERSION "AUTO" CACHE STRING "Version of the installed development library" )

if("${PYTHON_VERSION}" STREQUAL "AUTO" OR "${PYTHON_VERSION}" STREQUAL "auto")
  find_package(PythonInterp EXACT 3)
  find_package(PythonLibs EXACT 3)
else ()
  find_package(PythonInterp EXACT "${PYTHON_VERSION}" REQUIRED)
  find_package(PythonLibs EXACT "${PYTHON_VERSION}" REQUIRED)
endif ()

MESSAGE(STATUS "Detected pythonlib version: ${PYTHONLIBS_VERSION_STRING}")
include(python_build_venv)

include(openssl_functions)
openssl_set_defines()

pkg_check_modules(LIBPCRE REQUIRED libpcre2-8)

option (ENABLE_TCP_WRAPPER "Enable TCP wrapper support" ${WRAP_FOUND})
set (SYSLOG_NG_ENABLE_TCP_WRAPPER ${ENABLE_TCP_WRAPPER})

set(ENABLE_SPOOF_SOURCE "AUTO" CACHE STRING "Enable spoof source support: ON, OFF, AUTO")
set_property(CACHE ENABLE_SPOOF_SOURCE PROPERTY STRINGS AUTO ON OFF)
message(STATUS "Checking Spoof source support")

# find_package(LIBNET) already set LIBNET_FOUND
if(ENABLE_SPOOF_SOURCE STREQUAL "AUTO" OR ENABLE_SPOOF_SOURCE STREQUAL "auto")
  if(LIBNET_FOUND)
    set(SYSLOG_NG_ENABLE_SPOOF_SOURCE ON)
    message(STATUS "  Spoof source support: enabled (AUTO, found libnet)")
  else()
    set(SYSLOG_NG_ENABLE_SPOOF_SOURCE OFF)
    message(STATUS "  Spoof source support: disabled (AUTO, libnet not found)")
  endif()
elseif(ENABLE_SPOOF_SOURCE STREQUAL "ON")
  if(NOT LIBNET_FOUND)
    message(FATAL_ERROR "Could not find libnet, and spoof source support was explicitly enabled.")
  endif()
  set(SYSLOG_NG_ENABLE_SPOOF_SOURCE ON)
  message(STATUS "  Spoof source support: enabled (forced ON)")
elseif(ENABLE_SPOOF_SOURCE STREQUAL "OFF")
  set(SYSLOG_NG_ENABLE_SPOOF_SOURCE OFF)
  message(STATUS "  Spoof source support: disabled (forced OFF)")
else()
  message(FATAL_ERROR "Invalid value (${ENABLE_SPOOF_SOURCE}) for ENABLE_SPOOF_SOURCE (must be ON, OFF, or AUTO)")
endif()

if(ENABLE_LINUX_CAPS AND NOT PC_LIBCAP_FOUND)
  message(FATAL_ERROR "ENABLE_LINUX_CAPS is defined, but the POSIX capabilities library could not be found.")
endif()
option (ENABLE_LINUX_CAPS "Enable memtrace" ${PC_LIBCAP_FOUND})
set(SYSLOG_NG_ENABLE_LINUX_CAPS ${ENABLE_LINUX_CAPS})

option (ENABLE_MEMTRACE "Enable memtrace" 0)
set (SYSLOG_NG_ENABLE_MEMTRACE ${ENABLE_MEMTRACE})

# TODO: This one does not seem to be used at all, add the missing configure option handling if needed in the future
if (DEFINED ENABLE_PERF)
  message (FATAL_ERROR "ENABLE_PERF is defined, but the cmake build system does not support it currently.")
endif ()
set (ENABLE_PERF OFF)
set (SYSLOG_NG_ENABLE_PERF ${ENABLE_PERF})

option(ENABLE_STACKDUMP "Enable stackdump using libunwind" OFF)
if (ENABLE_STACKDUMP)
  if (NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message (FATAL_ERROR "ENABLE_STACKDUMP is defined, but it is only supported on Linux currently.")
  endif ()
  find_package (LIBUNWIND)
  if (NOT LIBUNWIND_FOUND)
    message (FATAL_ERROR "ENABLE_STACKDUMP is defined, but the libunwind library could not be found.")
  endif ()
  set (CMAKE_REQUIRED_INCLUDES ${LIBUNWIND_INCLUDE_DIR})
  if (NOT APPLE)
    set (CMAKE_REQUIRED_LIBRARIES ${LIBUNWIND_LIBRARY})
  endif ()
endif ()
set (SYSLOG_NG_ENABLE_STACKDUMP ${ENABLE_STACKDUMP})

if (WITH_GETTEXT)
    set(CMAKE_PREFIX_PATH ${WITH_GETTEXT})
    find_package(Gettext REQUIRED QUIET)
    set(CMAKE_PREFIX_PATH "")
else()
    find_package(Gettext REQUIRED QUIET)
endif()

set(Eventlog_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib/eventlog/src")

add_custom_target(style-check  COMMAND ${PROJECT_SOURCE_DIR}/scripts/style-checker.sh check  ${PROJECT_SOURCE_DIR} ${PROJECT_BINARY_DIR})
add_custom_target(style-format COMMAND ${PROJECT_SOURCE_DIR}/scripts/style-checker.sh format ${PROJECT_SOURCE_DIR} ${PROJECT_BINARY_DIR})
add_custom_target(check-commits
  COMMAND ${PROJECT_SOURCE_DIR}/tests/commits/check.sh
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
add_custom_target(check-copyright
  COMMAND ${PROJECT_SOURCE_DIR}/tests/copyright/check.sh . ${PROJECT_BINARY_DIR} policy
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
set_target_properties(check-copyright PROPERTIES
  ADDITIONAL_MAKE_CLEAN_FILES
  "copyright-run.log;copyright-err.log")

option(BUILD_TESTING "Enable unit tests" ON)
if (BUILD_TESTING)
  find_package (criterion)
endif()

include(add_tests)

if (BUILD_TESTING)
  if (NOT CRITERION_FOUND)
    message(FATAL_ERROR "BUILD_TESTING is enabled (by default, or explicitely) without criterion detected! You can turn off testing via the -DBUILD_TESTING=OFF cmake option.")
  else()
    set(CTEST_ENVIRONMENT
      "G_SLICE=always-malloc,debug-blocks"
      "G_DEBUG=fatal-warnings,fatal-criticals,gc-friendly")

    # The inclusion of CTest triggers enable_testing()
    # CMake will generate tests only if the enable_testing() command has been invoked.
    # The CTest module invokes the command automatically when the BUILD_TESTING option is ON.
    include(CTest)

    # This flag might be a security issue, do not use in production code, unfortunately we still need it for criterion tests and the current mocking soution
    if (APPLE)
      SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-flat_namespace")
    endif()

    add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} -j $$(nproc) --output-on-failure)
    # This one is useful to see the failed tests in details
    add_custom_target(check_failed COMMAND ${CMAKE_CTEST_COMMAND} -j $$(nproc) --rerun-failed --output-on-failure)
  endif()
endif()

set(IMPORTANT_WARNINGS
  -Wshadow)

set(ACCEPTABLE_WARNINGS
  -Wno-stack-protector
  -Wno-unused-parameter
  -Wno-variadic-macros)

option(ENABLE_EXTRA_WARNINGS "Enable extra warnings" ON)
if (ENABLE_EXTRA_WARNINGS)
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

  if ("${CMAKE_C_COMPILER_ID}" MATCHES "Clang")
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

add_compile_options (${IMPORTANT_WARNINGS} ${ACCEPTABLE_WARNINGS} ${EXTRA_WARNINGS})

option (ENABLE_FORCE_GNU99 "Enforce C99 with gnu extensions" OFF)
if (ENABLE_FORCE_GNU99)
  set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")
endif ()

option (ENABLE_GPROF "Enable gcc profiling" OFF)
if (ENABLE_GPROF)
  set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pg")
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")
endif ()
set (SYSLOG_NG_ENABLE_GCOV ${ENABLE_GPROF})

option (ENABLE_GCOV "Enable code coverage analysis (like gcov) support" OFF)
if (ENABLE_GCOV)
  set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage")
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
endif ()
set (SYSLOG_NG_ENABLE_GPROF ${ENABLE_GCOV})

# Sanatizer configuration
set(SANITIZER "OFF" CACHE STRING "Enable clang sanitizer")
set_property(CACHE SANITIZER PROPERTY STRINGS OFF address thread undefined address-all)
if (SANITIZER)
  SET(SANITIZE_MODE "${SANITIZER}")
  if (SANITIZER MATCHES address-all)
    SET(SANITIZE_MODE "address")
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize-address-use-after-scope")
  endif()
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O1 -fsanitize=${SANITIZE_MODE} -fno-omit-frame-pointer -fno-optimize-sibling-calls")
  if(APPLE)
    SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=${SANITIZE_MODE}")
  endif()
endif()


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
print_config_summary()
