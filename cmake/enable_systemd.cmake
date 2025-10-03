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

message(STATUS "Checking systemd and journal support")

find_package(systemd)

pkg_search_module(SYSTEMD_WITH_NAMESPACE libsystemd>=245)
message(STATUS "  Found lib${SYSTEMD_WITH_NAMESPACE_LIBRARIES} version ${SYSTEMD_WITH_NAMESPACE_VERSION}")

module_switch(ENABLE_JOURNALD "Enable systemd-journal" Libsystemd_FOUND)
set(WITH_SYSTEMD_JOURNAL "system" CACHE STRING "Link against the system supplied or the wrapper library")
set_property(CACHE WITH_SYSTEMD_JOURNAL PROPERTY STRINGS system wrapper)

if(ENABLE_JOURNALD)
  if(WITH_SYSTEMD_JOURNAL STREQUAL "system" AND NOT Libsystemd_FOUND)
    message(FATAL_ERROR "systemd-journal module is enabled, but the required systemd library dependency could not be found")
  endif()

  # TODO: Add the missing configure option handling like it has for autotools
  if(DEFINED ENABLE_SYSTEMD)
    message(FATAL_ERROR "ENABLE_SYSTEMD option is defined, but the cmake build system does not support it currently.")
  endif()

  set(ENABLE_SYSTEMD ON)

  if(WITH_SYSTEMD_JOURNAL STREQUAL "system")
    if(SYSTEMD_WITH_NAMESPACE_LIBRARIES STREQUAL "systemd")
      set(SYSLOG_NG_HAVE_JOURNAL_NAMESPACES 1)
      message(STATUS "  Have journal namespaces")
    endif()

    set(SYSLOG_NG_SYSTEMD_JOURNAL_MODE SYSLOG_NG_JOURNALD_SYSTEM)
  elseif(WITH_SYSTEMD_JOURNAL STREQUAL "wrapper")
    set(SYSLOG_NG_SYSTEMD_JOURNAL_MODE SYSLOG_NG_JOURNALD_OPTIONAL)
  endif()
else()
  set(ENABLE_SYSTEMD OFF)
  set(SYSLOG_NG_SYSTEMD_JOURNAL_MODE SYSLOG_NG_JOURNALD_OFF)
endif()

set(SYSLOG_NG_ENABLE_SYSTEMD ${ENABLE_SYSTEMD})
