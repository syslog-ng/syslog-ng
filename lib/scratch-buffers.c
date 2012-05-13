/*
 * Copyright (c) 2011 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 2011 Gergely Nagy <algernon@balabit.hu>
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
  GTrashStack *scratch_buffers;
}
TLS_BLOCK_END;

#define local_scratch_buffers	__tls_deref(scratch_buffers)

ScratchBuffer *
scratch_buffer_acquire(void)
{
  ScratchBuffer *sb;

  sb = g_trash_stack_pop(&local_scratch_buffers);
  if (!sb)
    {
      sb = g_new(ScratchBuffer, 1);
      sb->s = g_string_new(NULL);
    }
  else
    g_string_set_size(sb->s, 0);
  return sb;
}

void
scratch_buffer_release(ScratchBuffer *sb)
{
  g_trash_stack_push(&local_scratch_buffers, sb);
}

void
scratch_buffers_free(void)
{
  ScratchBuffer *sb;

  while ((sb = g_trash_stack_pop(&local_scratch_buffers)) != NULL)
    {
      g_string_free(sb->s, TRUE);
      g_free(sb);
    }
}
