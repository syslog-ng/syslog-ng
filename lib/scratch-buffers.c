/*
 * Copyright (c) 2011-2013 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 2011-2013 Gergely Nagy <algernon@balabit.hu>
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

#include "tls-support.h"
#include "scratch-buffers.h"
#include "misc.h"

TLS_BLOCK_START
{
  GTrashStack *sb_gstrings;
  GList *sb_registry;
}
TLS_BLOCK_END;

/* GStrings */

#define local_sb_gstrings        __tls_deref(sb_gstrings)

GTrashStack *
sb_gstring_acquire_buffer(void)
{
  SBGString *sb;

  sb = g_trash_stack_pop(&local_sb_gstrings);
  if (!sb)
    {
      sb = g_new(SBGString, 1);
      g_string_steal(sb_gstring_string(sb));
    }
  else
    g_string_set_size(sb_gstring_string(sb), 0);

  return (GTrashStack *) sb;
}

void
sb_gstring_release_buffer(GTrashStack *s)
{
  SBGString *sb = (SBGString *) s;

  g_trash_stack_push(&local_sb_gstrings, sb);
}

void
sb_gstring_free_stack(void)
{
  SBGString *sb;

  while ((sb = g_trash_stack_pop(&local_sb_gstrings)) != NULL)
    {
      g_free(sb_gstring_string(sb)->str);
      g_free(sb);
    }
}

ScratchBufferStack SBGStringStack = {
  .acquire_buffer = sb_gstring_acquire_buffer,
  .release_buffer = sb_gstring_release_buffer,
  .free_stack = sb_gstring_free_stack
};

/* Global API */

#define local_sb_registry  __tls_deref(sb_registry)

void
scratch_buffers_register(ScratchBufferStack *stack)
{
  local_sb_registry = g_list_append(local_sb_registry, stack);
}

void
scratch_buffers_init(void)
{
  local_sb_registry = NULL;
  scratch_buffers_register(&SBGStringStack);
}

static void
scratch_buffers_free_stack(gpointer data, gpointer user_data)
{
  ScratchBufferStack *s = (ScratchBufferStack *) data;

  s->free_stack();
}

void
scratch_buffers_free(void)
{
  g_list_foreach(local_sb_registry, scratch_buffers_free_stack, NULL);
  g_list_free(local_sb_registry);
}
