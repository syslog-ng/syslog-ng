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

typedef struct compat_iv_signal
{
  void *cookie;
  void (*handler)(void *cookie);   /* same sig as ivykis */
  int   signum;                    /* ignored on Win */
  void *impl;                      /* private */
} compat_iv_signal;

static inline void compat_iv_signal_init(compat_iv_signal *s)
{
  memset(s, 0, sizeof(*s));
}
static inline void compat_iv_signal_register(compat_iv_signal *s)
{
  return;
}
static inline void compat_iv_signal_unregister(compat_iv_signal *s)
{
  return;
}
static inline void compat_ignore_signal(int signum)
{
  return;
}

#define COMPAT_IV_SIGNAL_INIT(s) do { memset((s), 0, sizeof(*(s))); } while (0)

#else /* _WIN32 */

#include <iv.h>
#include <iv_signal.h>
#include <signal.h>

typedef struct iv_signal compat_iv_signal;

static inline void compat_iv_signal_init(compat_iv_signal *s)
{
  IV_SIGNAL_INIT(s);
}
static inline void compat_iv_signal_register(compat_iv_signal *s)
{
  iv_signal_register(s);
}
static inline void compat_iv_signal_unregister(compat_iv_signal *s)
{
  iv_signal_unregister(s);
}
static inline void compat_ignore_signal(int signum)
{
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = SIG_IGN;
  sigaction(signum, &sa, NULL);
}

#define COMPAT_HAVE_POSIX_SIGNALS
#define COMPAT_IV_SIGNAL_INIT(s)  IV_SIGNAL_INIT((s))

#endif

/* Helper to mirror your local helper, so mainloop doesn't need its own */
static inline void compat_register_signal_handler(compat_iv_signal *sig, int signum,
                                                  void (*handler)(void *), void *cookie)
{
  COMPAT_IV_SIGNAL_INIT(sig);
  sig->signum  = signum;
  sig->cookie  = cookie;
  sig->handler = handler;
  compat_iv_signal_register(sig);
}