# ############################################################################
# Copyright (c) 2017 Balabit
# Copyright (c) 2022 One Identity
# Copyright (c) 2025 Istvan Hoffmann <hofione@gmail.com>
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
# ############################################################################

add_definitions(-D_GNU_SOURCE=1)
add_definitions(-D_LARGEFILE64_SOURCE=1)

set(CMAKE_REQUIRED_DEFINITIONS "-D_GNU_SOURCE=1")
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

include(openssl_functions)
