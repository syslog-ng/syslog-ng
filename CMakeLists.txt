cmake_minimum_required (VERSION 2.8.12)

# use, i.e. don't skip the full RPATH for the build tree
SET(CMAKE_SKIP_BUILD_RPATH  FALSE)

# when building, don't use the install RPATH already
# (but later on when installing)
SET(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
set(CMAKE_MACOSX_RPATH 1)

# add the automatically determined parts of the RPATH
# which point to directories outside the build tree to the install RPATH
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

project (syslog-ng)
set (CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/Modules)
set (CMAKE_POSITION_INDEPENDENT_CODE 1)
set (BISON_FLAGS "-Wno-other")

add_compile_options("-Wno-initializer-overrides")

include (CheckIncludeFiles)

set (SYSLOG_NG_VERSION_MAJOR 3)
set (SYSLOG_NG_VERSION_MINOR 9)
set (SYSLOG_NG_VERSION_PATCH 1)
set (SYSLOG_NG_VERSION "${SYSLOG_NG_VERSION_MAJOR}.${SYSLOG_NG_VERSION_MINOR}.${SYSLOG_NG_VERSION_PATCH}")
set (SYSLOG_NG_COMBINED_VERSION ${SYSLOG_NG_VERSION})
set (SYSLOG_NG_SOURCE_REVISION ${SYSLOG_NG_VERSION})

set (SYSLOG_NG_PATH_PREFIX ${CMAKE_INSTALL_PREFIX})
set (SYSLOG_NG_PATH_SYSCONFDIR "\${prefix}/etc")
set (SYSLOG_NG_PATH_DATADIR "\${datarootdir}")
set (SYSLOG_NG_PATH_PIDFILEDIR "\${localstatedir}")
set (SYSLOG_NG_PATH_LOCALSTATEDIR "\${prefix}/var")
set (SYSLOG_NG_MODULE_PATH "\${exec_prefix}/lib/syslog-ng")
set (SYSLOG_NG_PATH_EXECPREFIX "\${prefix}")
set (SYSLOG_NG_PATH_CONFIG_INCLUDEDIR "\${datadir}/syslog-ng/include")
set (SYSLOG_NG_PATH_SCLDIR "\${datadir}/syslog-ng/include/scl")
set (SYSLOG_NG_PATH_LIBEXECDIR "\${exec_prefix}/libexec")
set (SYSLOG_NG_PATH_DATAROOTDIR "\${prefix}/share")
set (SYSLOG_NG_ENABLE_LINUX_CAPS 0)
set (SYSLOG_NG_ENABLE_TCP_WRAPPER 0)
set (SYSLOG_NG_ENABLE_GPROF 0)
set (SYSLOG_NG_ENABLE_MEMTRACE 0)
set (SYSLOG_NG_ENABLE_SYSTEMD 0)
set (SYSLOG_NG_PATH_MODULEDIR "\${exec_prefix}/lib/syslog-ng")
set (SYSLOG_NG_PACKAGE_NAME "${CMAKE_PROJECT_NAME}")
set (SYSLOG_NG_PATH_XSDDIR "\${datadir}/syslog-ng/xsd")
set (SYSLOG_NG_JAVA_MODULE_PATH "\${exec_prefix}/lib/syslog-ng/java-modules")
set (SYSLOG_NG_PATH_TOPSRC_DIR "${CMAKE_SOURCE_DIR}")
set (LIBDIR "\${exec_prefix}/lib")
set (INCLUDEDIR "\${prefix}/include")
set (TOOLSDIR "\${datadir}/syslog-ng/tools")
set (SYSLOG_NG_ENABLE_FORCED_SERVER_MODE 1)

IF(CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES RelWithDebInfo)
  set (SYSLOG_NG_ENABLE_DEBUG 1)
ELSE()
  set (SYSLOG_NG_ENABLE_DEBUG 0)
ENDIF()

# CMake will generate tests only if the enable_testing() command has been invoked.
# The CTest module invokes the command automatically when the BUILD_TESTING option is ON.
enable_testing()

include (CheckTypeSize)
include (CheckStructMember)
include (CheckSymbolExists)
include (GenerateYFromYm)
include (CheckStructHasMember)

add_definitions(-D_GNU_SOURCE=1)

include(CheckSockaddrStorage)
if (HAVE_STRUCT_SOCKADDR_STORAGE)
    set (SYSLOG_NG_HAVE_STRUCT_SOCKADDR_STORAGE 1)
else()
    set (SYSLOG_NG_HAVE_STRUCT_SOCKADDR_STORAGE 0)
endif()

include(CheckCreds)
if (HAVE_STRUCT_UCRED OR HAVE_STRUCT_CMSGCRED)
    set (SYSLOG_NG_HAVE_STRUCT_UCRED 1)
else()
    set (SYSLOG_NG_HAVE_STRUCT_UCRED 0)
endif()

include(CheckIPv6)

option(ENABLE_IPV6 "Enable IPv6" ON)
if (ENABLE_IPV6)
    set(SYSLOG_NG_ENABLE_IPV6 ${HAVE_IPV6})
endif()

check_symbol_exists (strtoll stdlib.h SYSLOG_NG_HAVE_STRTOLL)
check_symbol_exists (strtoimax inttypes.h SYSLOG_NG_HAVE_STRTOIMAX)
check_symbol_exists (inet_aton "sys/socket.h;netinet/in.h;arpa/inet.h" SYSLOG_NG_HAVE_INET_ATON)
check_symbol_exists (getutent utmp.h SYSLOG_NG_HAVE_GETUTENT)
check_symbol_exists (getutxent utmpx.h SYSLOG_NG_HAVE_GETUTXENT)

check_include_files (utmp.h SYSLOG_NG_HAVE_UTMP_H)
check_include_files (utmpx.h SYSLOG_NG_HAVE_UTMPX_H)

check_struct_has_member("struct utmpx" "ut_type" "utmpx.h" UTMPX_HAS_UT_TYPE LANGUAGE C)
check_struct_has_member("struct utmp" "ut_type" "utmp.h" UTMP_HAS_UT_TYPE LANGUAGE C)
check_struct_has_member("struct utmpx" "ut_user" "utmpx.h" UTMPX_HAS_UT_USER LANGUAGE C)
check_struct_has_member("struct utmp" "ut_user" "utmp.h" UTMP_HAS_UT_USER LANGUAGE C)

if ((UTMPX_HAS_UT_TYPE AND UTMPX_HAS_UT_USER) OR (UTMPX_HAS_UT_TYPE AND UTMP_HAS_UT_USER))
  set (SYSLOG_NG_HAVE_MODERN_UTMP 1)
endif()

set (WITH_GETTEXT "" CACHE STRING "Set the prefix where gettext is installed (e.g. /usr)")

find_package(BISON 2.4 REQUIRED)
find_package(GLIB 2.10.1 REQUIRED)
find_package(Ivykis REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(FLEX REQUIRED)
find_package(LibNet REQUIRED)
find_package(Resolv REQUIRED)
find_package(Wrap)

pkg_check_modules(LIBPCRE REQUIRED libpcre)

if (WRAP_FOUND)
  set(SYSLOG_NG_ENABLE_TCP_WRAPPER 1)
endif()

if (LIBNET_FOUND)
  set(SYSLOG_NG_ENABLE_SPOOF_SOURCE 1)
endif()

if (WITH_GETTEXT)
    set (CMAKE_PREFIX_PATH ${WITH_GETTEXT})
    find_package(Gettext REQUIRED QUIET)
    set (CMAKE_PREFIX_PATH "")
else()
    find_package(Gettext REQUIRED QUIET)
endif()

set(Eventlog_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib/eventlog/src")
set(Eventlog_LIBRARY "eventlog")
set(CORE_INCLUDE_DIRS
    ${Eventlog_INCLUDE_DIR}
    ${GLIB_INCLUDE_DIRS}
    ${Gettext_INCLUDE_DIR}
    ${Ivykis_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIRS}
    ${LIBPCRE_INCLUDE_DIRS}
)

set(CORE_LIBRARIES
    ${BISON_LIBRARIES}
    ${Eventlog_LIBRARY}
    ${FLEX_LIBRARIES}
    ${GLIB_GMODULE_LIBRARIES}
    ${GLIB_GTHREAD_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${Gettext_LIBRARIES}
    ${Ivykis_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${RESOLV_LIBRARIES}
    ${LIBPCRE_LIBRARIES}
    )

include_directories(SYSTEM ${CORE_INCLUDE_DIRS})

include_directories (${PROJECT_BINARY_DIR}/lib)
include_directories (${PROJECT_BINARY_DIR})
include_directories (${PROJECT_SOURCE_DIR}/lib)
include_directories (${PROJECT_SOURCE_DIR})
add_subdirectory(Mk)
add_subdirectory(scl)
add_subdirectory(lib)
add_subdirectory(modules)
add_subdirectory(scripts)
add_subdirectory(syslog-ng)
add_subdirectory(syslog-ng-ctl)

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/syslog-ng-config.h.in ${CMAKE_CURRENT_BINARY_DIR}/syslog-ng-config.h)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/syslog-ng-config.h DESTINATION include/syslog-ng)

install(DIRECTORY DESTINATION var)

# Package Information for pkg-config
set (PKG_CONFIG_EXEC_PREFIX ${SYSLOG_NG_PATH_EXECPREFIX})
set (PKG_CONFIG_DATAROOTDIR ${SYSLOG_NG_PATH_DATAROOTDIR})
set (PKG_CONFIG_DATADIR ${SYSLOG_NG_PATH_DATAROOTDIR})
set (PKG_CONFIG_LIBDIR ${LIBDIR})
set (PKG_CONFIG_INCLUDEDIR ${INCLUDEDIR})
set (PKG_CONFIG_TOOLSDIR ${TOOLSDIR})
set (PKG_CONFIG_MODULEDIR ${SYSLOG_NG_PATH_MODULEDIR})
set (PKG_CONFIG_CONFIG_INCLUDEDIR ${SYSLOG_NG_PATH_CONFIG_INCLUDEDIR})
set (PKG_CONFIG_SCLDIR ${SYSLOG_NG_PATH_SCLDIR})
set (PKG_CONFIG_IVYKIS ${Ivykis_INCLUDE_DIRS})
set (PKG_CONFIG_PACKAGE_VERSION ${SYSLOG_NG_VERSION})
set (PKG_CONFIG_INTERNAL_IVYKIS_CFLAGS ${Ivykis_INCLUDE_DIRS})
set (libdir "\${libdir}")
set (includedir "\${includedir}")

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/syslog-ng.pc.cmake ${CMAKE_CURRENT_BINARY_DIR}/syslog-ng.pc)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/syslog-ng.pc DESTINATION lib/pkgconfig)
