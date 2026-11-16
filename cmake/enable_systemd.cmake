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

module_switch(ENABLE_SYSTEMD "Enable systemd support" Libsystemd_FOUND)

# The systemd-journal source additionally requires systemd support itself.
module_switch(ENABLE_JOURNALD "Enable systemd-journal" Libsystemd_FOUND ENABLE_SYSTEMD)
set(WITH_SYSTEMD_JOURNAL "auto" CACHE STRING "Link against the system supplied or the wrapper library, or auto-detect")
set_property(CACHE WITH_SYSTEMD_JOURNAL PROPERTY STRINGS system wrapper auto)

if(ENABLE_JOURNALD AND NOT ENABLE_SYSTEMD)
  message(FATAL_ERROR "systemd-journal module is enabled, but systemd support (ENABLE_SYSTEMD) is disabled")
endif()

if(ENABLE_JOURNALD)
  # "auto" only distinguishes based on the modern unified libsystemd; unlike
  # autotools it does not separately probe the legacy split
  # libsystemd-daemon/libsystemd-journal libraries.
  set(_with_systemd_journal "${WITH_SYSTEMD_JOURNAL}")

  if(_with_systemd_journal STREQUAL "auto")
    if(Libsystemd_FOUND)
      set(_with_systemd_journal "system")
    else()
      set(_with_systemd_journal "wrapper")
    endif()

    message(STATUS "  systemd-journal mode (auto): ${_with_systemd_journal}")
  endif()

  if(_with_systemd_journal STREQUAL "system" AND NOT Libsystemd_FOUND)
    message(FATAL_ERROR "systemd-journal module is enabled, but the required systemd library dependency could not be found")
  endif()

  if(_with_systemd_journal STREQUAL "system")
    if(SYSTEMD_WITH_NAMESPACE_LIBRARIES STREQUAL "systemd")
      set(SYSLOG_NG_HAVE_JOURNAL_NAMESPACES 1)
      message(STATUS "  Have journal namespaces")
    endif()

    set(SYSLOG_NG_SYSTEMD_JOURNAL_MODE SYSLOG_NG_JOURNALD_SYSTEM)
  elseif(_with_systemd_journal STREQUAL "wrapper")
    set(SYSLOG_NG_SYSTEMD_JOURNAL_MODE SYSLOG_NG_JOURNALD_OPTIONAL)
  endif()
else()
  set(SYSLOG_NG_SYSTEMD_JOURNAL_MODE SYSLOG_NG_JOURNALD_OFF)
endif()

set(SYSLOG_NG_ENABLE_SYSTEMD ${ENABLE_SYSTEMD})
