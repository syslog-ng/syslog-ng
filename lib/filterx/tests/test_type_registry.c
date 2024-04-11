/*
 * Copyright (c) 2024 shifter <shifter@axoflow.com>
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

#include <criterion/criterion.h>
#include "libtest/cr_template.h"

#include "filterx/filterx-object.h"
#include "filterx/filterx-private.h"

#include "apphook.h"


FILTERX_DEFINE_TYPE(dummy_base, FILTERX_TYPE_NAME(object));

Test(type_registry, test_type_registry_registering_existing_key_returns_false)
{
  GHashTable *ht;
  filterx_types_init_private(&ht);
  // first register returns TRUE
  cr_assert(filterx_type_register_private(ht, "base", &FILTERX_TYPE_NAME(dummy_base)));
  // second attampt of register must return FALSE
  cr_assert(!filterx_type_register_private(ht, "base", &FILTERX_TYPE_NAME(dummy_base)));
  filterx_types_deinit_private(ht);
}

Test(type_registry, test_type_registry_lookup)
{
  GHashTable *ht;
  filterx_types_init_private(&ht);

  // type not found
  FilterXType *fxtype = filterx_type_lookup_private(ht, "base");
  cr_assert(fxtype == NULL);

  // add dummy function
  cr_assert(filterx_type_register_private(ht, "base", &FILTERX_TYPE_NAME(dummy_base)));

  // lookup returns dummy function
  fxtype = filterx_type_lookup_private(ht, "base");
  cr_assert(fxtype != NULL);

  cr_assert_eq(&FILTERX_TYPE_NAME(dummy_base), fxtype);

  filterx_types_deinit_private(ht);
}

static void
setup(void)
{
  app_startup();
}

static void
teardown(void)
{
  app_shutdown();
}

TestSuite(type_registry, .init = setup, .fini = teardown);
