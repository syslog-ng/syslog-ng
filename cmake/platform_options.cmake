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

# OS specific other comile/linker options might be needed
if(CMAKE_SYSTEM_NAME STREQUAL "HP-UX")
  # Check if compiler is GCC
  if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_definitions(-U_XOPEN_SOURCE -U_XOPEN_SOURCE_EXTENDED _HPUX_SOURCE)
    add_link_options(-lcl)

    set(SYSLOG_NG_HAVE_BROKEN_PREAD=1)
    message(WARNING "NOTE: on HP-UX with gcc, you might need to edit sys/socket.h manually or you'll get compilation errors")
  endif()

elseif(CMAKE_SYSTEM_NAME STREQUAL "AIX")
  # Try to detect if large file support is available
  check_symbol_exists(lseek64 "unistd.h" HAVE_LSEEK64)

  if(HAVE_LSEEK64)
    add_compile_definitions(_LARGE_FILES=1)
  endif()

  # NOTE: The -brtl option to the linker must be set before calling the
  # configure script, as otherwise the generated libtool will behave
  # differently. We need the runtime linker during execution (hence the
  # -brtl) to load external modules. Also, please note that with -brtl the
  # linker behaves similarly to what is expected on other UNIX systems,
  # without it, it refuses to link an .so in if there's no reference from
  # the main program, even if there is a proper -llibname option.
  add_link_options(-Wl,-brtl)

# NOTE: There is no common, global LDFLAGS implementation for modules in cmake, but hhe currently produced modules are loadable .a modules now already
# set(MODULE_LDFLAGS "-avoid-version -module")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  # NOTE: These are not needed in cmake, 1st should be explicitly set in CMakeLists.txt and not set by default, the 2nd is controlled by add_library(foo SHARED/STATIC...)
  # set(MODULE_LDFLAGS "-avoid-version -dylib")
  # NOTE: BUILD_WITH_INSTALL_RPATH/SKIP_BUILD_RPATH controls this on macOS
  # TEST_NO_INSTALL_FLAG="-no-fast-install"

  # NOTE: This is now seems to be an Apple/Xcode "only" issue, but probably much better a clang one, so later we might want to add this globally
  # XCode15 new linker has some issues (e.g. https://developer.apple.com/forums/thread/737707)
  # Let the user switch back to classic linking if needed (https://developer.apple.com/documentation/xcode-release-notes/xcode-15-release-notes#Linking)
  option(FORCE_CLASSIC_LINKING "Enable classic linking" OFF)

  if(FORCE_CLASSIC_LINKING)
    # XCode15 new linker has some issues (e.g. https://developer.apple.com/forums/thread/737707)
    # switch back to classic linking till not fixed (https://developer.apple.com/documentation/xcode-release-notes/xcode-15-release-notes#Linking)
    SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-ld_classic")
  endif()

  # Expose the modern, RFC 3542 IPv6 socket API instead of the deprecated RFC 2292 one
  add_compile_definitions(__APPLE_USE_RFC_3542)

elseif(CMAKE_SYSTEM_NAME STREQUAL "OSF1")
  add_compile_definitions(_XOPEN_SOURCE=500 _XOPEN_SOURCE_EXTENDED _OSF_SOURCE _POSIX_C_SOURCE)
endif()
