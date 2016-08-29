/*
 * Copyright (c) 2010-2012 Balabit
 * Copyright (c) 2010-2012 Balázs Scheidler
 * Copyright (c) 2012 Gergely Nagy <algernon@balabit.hu>
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
 */

#include "uuid.h"

#include <openssl/rand.h>
#include <arpa/inet.h>

void
uuid_gen_random(gchar *buf, gsize buflen)
{
  union
  {
    struct
    {
      guint32 time_low;
      guint16 time_mid;
      guint16 time_hi_and_version;
      guint8  clk_seq_hi_res;
      guint8  clk_seq_low;
      guint8  node[6];
      guint16 node_low;
      guint32 node_hi;
    };
    guchar __rnd[16];
  } uuid;

  RAND_bytes(uuid.__rnd, sizeof(uuid));

  uuid.clk_seq_hi_res = (uuid.clk_seq_hi_res & ~0xC0) | 0x80;
  uuid.time_hi_and_version = htons((uuid.time_hi_and_version & ~0xF000) | 0x4000);

  g_snprintf(buf, buflen, "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             uuid.time_low, uuid.time_mid, uuid.time_hi_and_version,
             uuid.clk_seq_hi_res, uuid.clk_seq_low,
             uuid.node[0], uuid.node[1], uuid.node[2],
             uuid.node[3], uuid.node[4], uuid.node[5]);

}
