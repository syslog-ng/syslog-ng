/*
 * Copyright (c) 2025 One Identity
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

#pragma once

#include <string.h>

#ifdef _WIN32

/* Windows: real compat struct + functions */
struct compat_iv_fd
{
  int   fd;
  void *cookie;
  void (*handler_in)(void *);
  void (*handler_out)(void *);
  void (*handler_err)(void *);
  /* impl is private to the platform .c file */
  void *impl;
};

typedef struct compat_iv_fd compat_iv_fd;

void compat_iv_fd_init(compat_iv_fd *w);

void compat_iv_fd_set_handler_in (compat_iv_fd *w, void (*fn)(void *));
void compat_iv_fd_set_handler_out(compat_iv_fd *w, void (*fn)(void *));
void compat_iv_fd_set_handler_err(compat_iv_fd *w, void (*fn)(void *));

int  compat_iv_fd_register_try (compat_iv_fd *w); /* 0=success (pollable), nonzero=fallback */
void compat_iv_fd_register     (compat_iv_fd *w);
void compat_iv_fd_unregister   (compat_iv_fd *w);
int  compat_iv_fd_registered   (compat_iv_fd *w); /* boolean */

void compat_iv_fd_deinit(compat_iv_fd *w);

static inline void compat_iv_fd_init_wrap(compat_iv_fd *w)
{
  memset(w, 0, sizeof(*w));
  compat_iv_fd_init(w);
}

/* Keep the call-site macro shape similar to IV_FD_INIT */
#define COMPAT_IV_FD_INIT(w) compat_iv_fd_init_wrap((w))

#else

/* POSIX: zero-overhead aliases to ivykis */
#include <iv.h>

typedef struct iv_fd compat_iv_fd;

#define compat_iv_fd_set_handler_in   iv_fd_set_handler_in
#define compat_iv_fd_set_handler_out  iv_fd_set_handler_out
#define compat_iv_fd_set_handler_err  iv_fd_set_handler_err
#define compat_iv_fd_register_try     iv_fd_register_try
#define compat_iv_fd_register         iv_fd_register
#define compat_iv_fd_unregister       iv_fd_unregister
#define compat_iv_fd_registered       iv_fd_registered
#define compat_iv_fd_deinit(w)        ((void)0)

#define COMPAT_IV_FD_INIT(w)          IV_FD_INIT((w))

#endif /* _WIN32 */