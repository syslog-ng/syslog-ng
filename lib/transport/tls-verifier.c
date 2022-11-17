/*
 * Copyright (c) 2002-2011 Balabit
 * Copyright (c) 1998-2011 Balázs Scheidler
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
#include "tls-verifier.h"

/* TLSVerifier */

TLSVerifier *
tls_verifier_new(TLSSessionVerifyFunc verify_func, gpointer verify_data,
                 GDestroyNotify verify_data_destroy)
{
  TLSVerifier *self = g_new0(TLSVerifier, 1);

  g_atomic_counter_set(&self->ref_cnt, 1);
  self->verify_func = verify_func;
  self->verify_data = verify_data;
  self->verify_data_destroy = verify_data_destroy;
  return self;
}

TLSVerifier *
tls_verifier_ref(TLSVerifier *self)
{
  g_assert(!self || g_atomic_counter_get(&self->ref_cnt) > 0);

  if (self)
    g_atomic_counter_inc(&self->ref_cnt);

  return self;
}

static void
_tls_verifier_free(TLSVerifier *self)
{
  g_assert(self);

  if (self)
    {
      if (self->verify_data && self->verify_data_destroy)
        self->verify_data_destroy(self->verify_data);
      g_free(self);
    }
}

void
tls_verifier_unref(TLSVerifier *self)
{
  g_assert(!self || g_atomic_counter_get(&self->ref_cnt));

  if (self && (g_atomic_counter_dec_and_test(&self->ref_cnt)))
    _tls_verifier_free(self);
}
