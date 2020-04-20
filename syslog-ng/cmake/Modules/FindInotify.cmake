#.rest:
# FindInotify
# --------------
#
# Try to find inotify on this system. This finds:
#  - libinotiy on Unix like systems, or 
#  - the kernel's inotify on Linux systems.
#
# This will define the following variables:
#
# ``Inotify_FOUND``
#    True if inotify is available
# ``Inotify_LIBRARIES``
#    This has to be passed to target_link_libraries()
# ``Inotify_INCLUDE_DIRS``
#    This has to be passed to target_include_directories()
#
# On Linux, the libraries and include directories are empty,
# even though Inotify_FOUND may be set to TRUE. This is because
# no special includes or libraries are needed. On other systems
# these may be needed to use inotify.

#=============================================================================
# Copyright 2016 Tobias C. Berner <tcberner@FreeBSD.org>
# Copyright 2017 Adriaan de Groot <groot@kde.org>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#=============================================================================

find_path(Inotify_INCLUDE_DIRS sys/inotify.h)
if(Inotify_INCLUDE_DIRS)
# On Linux there is no library to link against, on the BSDs there is.
# On the BSD's, inotify is implemented through a library, libinotify.
    if( CMAKE_SYSTEM_NAME MATCHES "Linux")
        set(Inotify_FOUND TRUE)

        set(Inotify_INCLUDE_DIRS "")
    else()
        find_library(Inotify_LIBRARIES NAMES inotify)
        include(FindPackageHandleStandardArgs)
        find_package_handle_standard_args(Inotify
            FOUND_VAR
                Inotify_FOUND
            REQUIRED_VARS
                Inotify_LIBRARIES
                Inotify_INCLUDE_DIRS
        )
        mark_as_advanced(Inotify_LIBRARIES Inotify_INCLUDE_DIRS)
        include(FeatureSummary)
        set_package_properties(Inotify PROPERTIES
            URL "https://github.com/libinotify-kqueue/"
            DESCRIPTION "inotify API on the *BSD family of operating systems."
        )
    endif()
else()
   set(Inotify_FOUND FALSE)
endif()

mark_as_advanced(Inotify_LIBRARIES Inotify_INCLUDE_DIRS) 
