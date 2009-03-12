/*
 * Copyright (c) 2002-2009 BalaBit IT Ltd, Budapest, Hungary
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Note that this permission is granted for only version 2 of the GPL.
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
  
#ifndef ATOMIC_H_INCLUDED
#define ATOMIC_H_INCLUDED


#if ENABLE_THREADS
typedef struct
{
  gint counter;
} GAtomicCounter;

static inline void
g_atomic_counter_inc(GAtomicCounter *c)
{
  g_atomic_int_inc(&c->counter);
}

static inline gboolean
g_atomic_counter_dec_and_test(GAtomicCounter *c)
{
  return g_atomic_int_dec_and_test(&c->counter);
}

static inline gint
g_atomic_counter_get(GAtomicCounter *c)
{
  return g_atomic_int_get(&c->counter);
}

static inline void
g_atomic_counter_set(GAtomicCounter *c, gint value)
{
  /* FIXME: we should use g_atomic_int_set, but that's available starting
   * with GLib 2.10 only, and we only use this function for initialization,
   * thus atomic write is not strictly needed as there's no concurrency
   * while initializing a refcounter.
   */
  
  c->counter = value;
}

#else
typedef guint32 GAtomicCounter;

static inline void
g_atomic_counter_inc(GAtomicCounter *c)
{
  (*c)++;
}

static inline gboolean
g_atomic_counter_dec_and_test(GAtomicCounter *c)
{
  (*c)--;
  return (*c) == 0;
}

static inline gint
g_atomic_counter_get(GAtomicCounter *c)
{
  return *c;
}

static inline void
g_atomic_counter_set(GAtomicCounter *c, gint value)
{
  *c = value;
}

#endif

#endif
