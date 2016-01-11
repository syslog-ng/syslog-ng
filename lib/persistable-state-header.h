/*
 * Copyright (c) 2002-2013 Balabit
 * Copyright (c) 1998-2013 Balázs Scheidler
 * Copyright (c) 2012-2013 Viktor Juhasz
 * Copyright (c) 2012-2013 Viktor Tusa
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

#ifndef _PERSISTABLE_STATE_HEADER_H
#define _PERSISTABLE_STATE_HEADER_H

typedef struct _PersistableStateHeader
{
  guint8 version;
  /* this indicates that the members in the struct are stored in
   * big-endian byte order. if the byte ordering of the struct doesn't
   * match the current CPU byte ordering, then the members are
   * byte-swapped when the state is loaded.
   *
   * Every State structure must have it's own byte-order swapping function
   */
  guint8 big_endian:1;
} PersistableStateHeader;

#endif
