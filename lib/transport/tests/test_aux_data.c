/*
 * Copyright (c) 2002-2013 Balabit
 * Copyright (c) 1998-2013 Balázs Scheidler
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
#include "testutils.h"
#include "transport/transport-aux-data.h"

#define AUX_DATA_TESTCASE(x, ...) do { aux_data_testcase_begin(#x, #__VA_ARGS__); x(__VA_ARGS__); aux_data_testcase_end(); } while(0)


#define aux_data_testcase_begin(func, args)    \
  do                                                                    \
    {                                                                   \
      testcase_begin("%s(%s)", func, args);                             \
    }                                                                   \
  while (0)

#define aux_data_testcase_end()                             		\
  do                                                            	\
    {                                                           	\
      testcase_end();                                           	\
    }                                                           	\
  while (0)


static LogTransportAuxData *
construct_empty_aux(void)
{
  LogTransportAuxData *aux = g_new(LogTransportAuxData, 1);
  
  log_transport_aux_data_init(aux);
  return aux;
}

static void
free_aux(LogTransportAuxData *aux)
{
  log_transport_aux_data_destroy(aux);
  g_free(aux);
}

static LogTransportAuxData *
construct_aux_with_some_data(void)
{
  LogTransportAuxData *aux = construct_empty_aux();

  /* set peer_addr twice to validate that peer_addr is correctly reference counted */
  log_transport_aux_data_set_peer_addr_ref(aux, g_sockaddr_inet_new("1.2.3.4", 5555));
  log_transport_aux_data_set_peer_addr_ref(aux, g_sockaddr_inet_new("1.2.3.4", 5555));
  log_transport_aux_data_add_nv_pair(aux, "foo", "bar");
  return aux;
}

static void
test_aux_data_reinit_returns_aux_into_initial_state_without_leaks(void)
{
  LogTransportAuxData *aux = construct_aux_with_some_data();

  log_transport_aux_data_reinit(aux);
  assert_null(aux->peer_addr, "aux->peer_addr is not NULL after reinit");
  
  free_aux(aux);
}

static void
_concat_nvpairs_helper(const gchar *name, const gchar *value, gsize value_len, gpointer user_data)
{
  GString *concatenated = (GString *) user_data;
  
  g_string_sprintfa(concatenated, "%s=%s\n", name, value);
  assert_gint(value_len, strlen(value), "foreach() length mismatch");
}

static gchar *
_concat_nvpairs(LogTransportAuxData *aux)
{
  GString *concatenated = g_string_sized_new(0);

  log_transport_aux_data_foreach(aux, _concat_nvpairs_helper, concatenated);
  return g_string_free(concatenated, FALSE);
}

static void
assert_concatenated_nvpairs(LogTransportAuxData *aux, const gchar *expected)
{
  gchar *concatenated = _concat_nvpairs(aux);;

  assert_string(concatenated, expected, "foreach() didn't return all-added nvpairs");
  g_free(concatenated);
}

static void
test_aux_data_added_nvpairs_are_returned_by_foreach_in_order(void)
{
  LogTransportAuxData *aux = construct_aux_with_some_data();

  log_transport_aux_data_add_nv_pair(aux, "super", "lativus");
  assert_concatenated_nvpairs(aux, "foo=bar\nsuper=lativus\n");
  free_aux(aux);
}

static void
test_aux_data_copy_creates_an_identical_copy(void)
{
  LogTransportAuxData *aux = construct_aux_with_some_data();
  LogTransportAuxData aux_copy;
  gchar *orig, *copy;
  
  log_transport_aux_data_copy(&aux_copy, aux);
  
  orig = _concat_nvpairs(aux);
  copy = _concat_nvpairs(&aux_copy);
  assert_string(orig, copy, "copy incorrectly copied aux->nvpairs");
  g_free(orig);
  g_free(copy);
  log_transport_aux_data_destroy(&aux_copy);
  free_aux(aux);
}

static void
test_aux_data_copy_separates_the_copies(void)
{
  LogTransportAuxData *aux = construct_aux_with_some_data();
  LogTransportAuxData aux_copy;
  gchar *orig, *copy;
  
  log_transport_aux_data_copy(&aux_copy, aux);
  log_transport_aux_data_add_nv_pair(aux, "super", "lativus");
  
  orig = _concat_nvpairs(aux);
  copy = _concat_nvpairs(&aux_copy);
  assert_false(strcmp(orig, copy) == 0, "copy incorrectly copied aux->nvpairs as change to one of them affected the other, orig=%s, copy=%s", orig, copy);
  g_free(orig);
  g_free(copy);
  log_transport_aux_data_destroy(&aux_copy);
  free_aux(aux);
}

static void
test_add_nv_pair_to_a_NULL_aux_data_will_do_nothing(void)
{
  log_transport_aux_data_add_nv_pair(NULL, "foo", "bar");
}

static void
test_aux_data(void)
{
  AUX_DATA_TESTCASE(test_aux_data_reinit_returns_aux_into_initial_state_without_leaks);
  AUX_DATA_TESTCASE(test_aux_data_added_nvpairs_are_returned_by_foreach_in_order);
  AUX_DATA_TESTCASE(test_aux_data_copy_creates_an_identical_copy);
  AUX_DATA_TESTCASE(test_aux_data_copy_separates_the_copies);
  AUX_DATA_TESTCASE(test_add_nv_pair_to_a_NULL_aux_data_will_do_nothing);
}

int main()
{
  test_aux_data();
  return 0;
}
