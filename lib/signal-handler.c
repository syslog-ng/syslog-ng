/*
 * Copyright (c) 2018 Balabit
 * Copyright (c) 2018 Kokan
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

#include "syslog-ng.h"

#include <signal.h>
#include <string.h>

#if defined(NSIG)
#  define SIGNAL_HANDLER_ARRAY_SIZE NSIG
#elif defined(_NSIG)
#  define SIGNAL_HANDLER_ARRAY_SIZE _NSIG
#else
#  define SIGNAL_HANDLER_ARRAY_SIZE 128
#endif

static struct sigaction external_sigactions[SIGNAL_HANDLER_ARRAY_SIZE];
static gboolean internal_sigaction_registered[SIGNAL_HANDLER_ARRAY_SIZE];

static const struct sigaction *
_get_external_sigaction(gint signum)
{
  g_assert(signum < SIGNAL_HANDLER_ARRAY_SIZE);
  return &external_sigactions[signum];
}

static void
_set_external_sigaction(gint signum, const struct sigaction *external_sigaction)
{
  g_assert(signum < SIGNAL_HANDLER_ARRAY_SIZE);
  memcpy(&external_sigactions[signum], external_sigaction, sizeof(struct sigaction));
}

static gboolean
_is_internal_sigaction_registered(gint signum)
{
  g_assert(signum < SIGNAL_HANDLER_ARRAY_SIZE);
  return internal_sigaction_registered[signum];
}

static void
_set_internal_sigaction_registered(gint signum)
{
  g_assert(signum < SIGNAL_HANDLER_ARRAY_SIZE);
  internal_sigaction_registered[signum] = TRUE;
}

void
signal_handler_exec_external_handler(gint signum)
{
  const struct sigaction *external_sigaction = _get_external_sigaction(signum);

  if (external_sigaction->sa_handler == SIG_DFL || external_sigaction->sa_handler == SIG_IGN)
    return;

  external_sigaction->sa_handler(signum);
}

#if SYSLOG_NG_HAVE_DLFCN_H

#include <dlfcn.h>

static int
_original_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact)
{
#ifdef __NetBSD__
  return __libc_sigaction14(signum, act, oldact);
#else
  static int (*real_sa)(int, const struct sigaction *, struct sigaction *);

  if (real_sa == NULL)
    real_sa = dlsym(RTLD_NEXT, "sigaction");

  return real_sa(signum, act, oldact);
#endif
}

static gint
_register_internal_sigaction(gint signum, const struct sigaction *act, struct sigaction *oldact)
{
  gint result = _original_sigaction(signum, act, oldact);

  if (result == 0)
    _set_internal_sigaction_registered(signum);

  return result;
}

static gboolean
_need_to_save_external_sigaction_handler(gint signum)
{
  /* We need to save external sigaction handlers for signums we internally set a handler to. See lib/mainloop.c */
  switch (signum)
    {
    case SIGCHLD:
    case SIGINT:
      return TRUE;
    default:
      return FALSE;
    }
}

static void
_save_external_sigaction_handler(gint signum, const struct sigaction *external_sigaction)
{
  if (!external_sigaction)
    return;

  _set_external_sigaction(signum, external_sigaction);
}

static void
_fill_oldact_with_previous_external_sigaction_handler(gint signum, struct sigaction *oldact)
{
  if (!oldact)
    return;

  memcpy(oldact, _get_external_sigaction(signum), sizeof(struct sigaction));
}

/* This should be as defined in the <signal.h> */
int
sigaction(int signum, const struct sigaction *act, struct sigaction *oldact)
{
  if (!_need_to_save_external_sigaction_handler(signum))
    return _original_sigaction(signum, act, oldact);

  /* Internal sigactions are always the first one to arrive to this function. */
  if (!_is_internal_sigaction_registered(signum))
    return _register_internal_sigaction(signum, act, oldact);

  _fill_oldact_with_previous_external_sigaction_handler(signum, oldact);
  _save_external_sigaction_handler(signum, act);

  return 0;
}

#endif
