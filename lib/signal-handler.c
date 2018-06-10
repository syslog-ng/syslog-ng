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
#include "signal-handler.h"

#include "syslog-ng.h"
#include "children.h"

#ifndef _WIN32
#include <dlfcn.h>
#endif

static const struct sigaction *sgchld_handler;

void
trigger_sigchld_handler_chain(int signum)
{
  if (sgchld_handler && sgchld_handler->sa_handler)
    {
      sgchld_handler->sa_handler(signum);
    }
}

#ifndef _WIN32
static int
call_original_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact)
{
  static int (*real_sa)(int, const struct sigaction *, struct sigaction *);

  if (real_sa == NULL)
    {
      real_sa = dlsym(RTLD_NEXT, "sigaction");
    }
  return real_sa(signum, act, oldact);
}

static gboolean
_save_handler(const struct sigaction *act)
{
  static gboolean is_first_handler = TRUE;
  if (is_first_handler)
    {
      is_first_handler = FALSE;
      return FALSE;
    }

  sgchld_handler = act;

  child_manager_register_external_sigchld_handler(&trigger_sigchld_handler_chain);

  return TRUE;
}

/* This should be as defined in the <signal.h> */
int
sigaction(int signum, const struct sigaction *act, struct sigaction *oldact)
{

  if (signum == SIGCHLD)
    {
      if (act && act->sa_handler == SIG_DFL)
        {
          return 0;
        }

      if (_save_handler(act))
        {
          return 0;
        }
    }


  return call_original_sigaction(signum, act, oldact);
}
#endif

