# ############################################################################
# Copyright (c) 2016 Balabit
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

check_symbol_exists(TCP_KEEPIDLE "sys/socket.h;netinet/in.h;netinet/tcp.h" HAVE_TCP_KEEPIDLE)
check_symbol_exists(TCP_KEEPINTVL "sys/socket.h;netinet/in.h;netinet/tcp.h" HAVE_TCP_KEEPINTVL)
check_symbol_exists(TCP_KEEPCNT "sys/socket.h;netinet/in.h;netinet/tcp.h" HAVE_TCP_KEEPCNT)

if(HAVE_TCP_KEEPIDLE AND HAVE_TCP_KEEPINTVL AND HAVE_TCP_KEEPCNT)
    set(SYSLOG_NG_HAVE_TCP_KEEPALIVE_TIMERS 1)
endif()
