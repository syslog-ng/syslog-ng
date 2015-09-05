/*
 * Copyright (c) 2002-2013 BalaBit IT Ltd, Budapest, Hungary
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


#include "apphook.h"
#include "gprocess.h"
#include "capability_lib.h"

#include <stdio.h>
#include <string.h>

static GList *cap_modify = NULL;
static int cap_save_counter = 0;
static int cap_restore_counter = 0;

static gboolean
_g_process_cap_modify(int capability, int onoff)
{
  CapModify *cap = g_new0(CapModify, 1);
  cap->capability = capability;
  cap->onoff = onoff;
  cap_modify = g_list_append(cap_modify, cap);
  fprintf(stderr, "cap_modify:%d switch:%s\n", capability, onoff == 1 ? "on":"off");
  return TRUE;
}

static cap_t
_g_process_cap_save(void)
{
  cap_save_counter++;
  fprintf(stderr, "cap_save:%d\n", cap_save_counter);
  return NULL;
}

static void
_g_process_cap_restore(cap_t r)
{
  cap_restore_counter++;
  fprintf(stderr, "cap_restore:%d\n", cap_restore_counter);
}

void
capability_mocking_reinit()
{
  g_free(g_process_capability);
  g_process_capability = g_new(Capability, 1);
  g_process_capability->g_process_cap_modify = _g_process_cap_modify;
  g_process_capability->g_process_cap_save = _g_process_cap_save;
  g_process_capability->g_process_cap_restore = _g_process_cap_restore;
  g_process_capability->have_capsyslog = FALSE;
}

void
cap_counter_reset()
{
  if (cap_modify)
    {
      g_list_free_full(cap_modify, g_free);
      g_list_free(cap_modify);
    }
  cap_modify = NULL;
  cap_save_counter = 0;
  cap_restore_counter = 0;
}

gboolean
check_cap_count(int capability, int onoff, int count)
{
  CapModify cap;
  cap.capability = capability;
  cap.onoff = onoff;
  GList *p;

  gint lcount = 0;
  for (p = g_list_first(cap_modify); p; p = g_list_next(p))
    {
      if (!memcmp(&cap, p->data, sizeof(CapModify)))
        {
          lcount++;
        }
    }

  return count == lcount;
}

guint
is_number_of_cap_modify_zero()
{
  return (g_list_first(cap_modify) == NULL);
}

gboolean
check_cap_save(int number)
{
  return cap_save_counter == number;
}

gboolean
check_cap_restore(int number)
{
  return cap_restore_counter == number;
}
