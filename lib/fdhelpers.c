/*
 * Copyright (c) 2002-2012 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */
#include "fdhelpers.h"

#ifndef _WIN32
  #include <unistd.h>
  #include <fcntl.h>
#else
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <windows.h>
  #include <io.h>
  #include <errno.h>
#endif

gboolean
g_fd_set_nonblock(int fd, gboolean enable)
{
#ifndef _WIN32
  int flags;

  if ((flags = fcntl(fd, F_GETFL)) == -1)
    return FALSE;
  if (enable)
    flags |= O_NONBLOCK;
  else
    flags &= ~O_NONBLOCK;

  if (fcntl(fd, F_SETFL, flags) < 0)
    {
      return FALSE;
    }
  return TRUE;
#else
  /* Windows: only sockets support nonblocking reliably */
  u_long mode = enable ? 1UL : 0UL;
  if (ioctlsocket((SOCKET)(uintptr_t)fd, FIONBIO, &mode) == 0)
    return TRUE;

  /* Not a socket (or invalid): no generic nonblocking for files/pipes */
  errno = ENOSYS;
  return FALSE;
#endif
}

gboolean
g_fd_set_cloexec(int fd, gboolean enable)
{
#ifndef _WIN32
  int flags;

  if ((flags = fcntl(fd, F_GETFD)) == -1)
    return FALSE;
  if (enable)
    flags |= FD_CLOEXEC;
  else
    flags &= ~FD_CLOEXEC;

  if (fcntl(fd, F_SETFD, flags) < 0)
    {
      return FALSE;
    }
  return TRUE;
#else
  /* Best effort: set/clear inherit flag on underlying OS handle (files/pipes) */
  intptr_t osfh = _get_osfhandle(fd);
  if (osfh != -1 && osfh != (intptr_t)INVALID_HANDLE_VALUE)
    {
      DWORD cur = 0;
      if (!GetHandleInformation((HANDLE)osfh, &cur))
        return FALSE;
      DWORD want = enable ? (cur | HANDLE_FLAG_INHERIT) : (cur & ~HANDLE_FLAG_INHERIT);
      return SetHandleInformation((HANDLE)osfh, HANDLE_FLAG_INHERIT, want & HANDLE_FLAG_INHERIT) ? TRUE : FALSE;
    }

  /* If it's actually a SOCKET disguised as an int, try best-effort */
  HANDLE h = (HANDLE)(uintptr_t)fd;
  SetHandleInformation(h, HANDLE_FLAG_INHERIT, enable ? HANDLE_FLAG_INHERIT : 0);
  /* Don’t fail hard: inheritance of sockets is rare in this codebase */
  return TRUE;
#endif
}
