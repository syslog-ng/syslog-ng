/*
 * Copyright (c) 2008-2013 Balabit
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

#include "logproto/logproto-server.h"
#include "logmsg/logmsg.h"
#include <stdlib.h>

#include <criterion/parameterized.h>
#include <criterion/criterion.h>


struct testcase_tuple
{
  gchar *msg;
  gsize msg_len;
  gint eom_ofs;
};

ParameterizedTestParameters(findeom, test_success)
{
  static struct testcase_tuple params[] =
  {
    {"a\nb\nc\n",  6,  1},
    {"ab\nb\nc\n",  7,  2},
    {"abc\nb\nc\n",  8,  3},
    {"abcd\nb\nc\n",  9,  4},
    {"abcde\nb\nc\n", 10,  5},
    {"abcdef\nb\nc\n", 11,  6},
    {"abcdefg\nb\nc\n", 12,  7},
    {"abcdefgh\nb\nc\n", 13,  8},
    {"abcdefghi\nb\nc\n", 14,  9},
    {"abcdefghij\nb\nc\n", 15, 10},
    {"abcdefghijk\nb\nc\n", 16, 11},
    {"abcdefghijkl\nb\nc\n", 17, 12},
    {"abcdefghijklm\nb\nc\n", 18, 13},
    {"abcdefghijklmn\nb\nc\n", 19, 14},
    {"abcdefghijklmno\nb\nc\n", 20, 15},
    {"abcdefghijklmnop\nb\nc\n", 21, 16},
    {"abcdefghijklmnopq\nb\nc\n", 22, 17},
    {"abcdefghijklmnopqr\nb\nc\n", 23, 18},
    {"abcdefghijklmnopqrs\nb\nc\n", 24, 19},
    {"abcdefghijklmnopqrst\nb\nc\n", 25, 20},
    {"abcdefghijklmnopqrstu\nb\nc\n", 26, 21},
    {"abcdefghijklmnopqrstuv\nb\nc\n", 27, 22},
    {"abcdefghijklmnopqrstuvw\nb\nc\n", 28, 23},
    {"abcdefghijklmnopqrstuvwx\nb\nc\n", 29, 24},
    {"abcdefghijklmnopqrstuvwxy\nb\nc\n", 30, 25},
    {"abcdefghijklmnopqrstuvwxyz\nb\nc\n", 31, 26},
  };


  return cr_make_param_array(struct testcase_tuple, params, G_N_ELEMENTS(params));
}

ParameterizedTest(struct testcase_tuple *tup, findeom, test_success)
{
  const guchar *eom;
  const guchar *msg = (const guchar *) tup->msg;

  eom = find_eom((guchar *) msg, tup-> msg_len);

  cr_assert(eom - msg == tup->eom_ofs,
            "EOM is at wrong location. msg=%s, eom_ofs=%d, eom=%s\n",
            msg, tup->eom_ofs, eom);
}

ParameterizedTestParameters(findeom, test_failed)
{
  static struct testcase_tuple params[] =
  {
    {"a",  1, -1},
    {"ab",  2, -1},
    {"abc",  3, -1},
    {"abcd",  4, -1},
    {"abcde",  5, -1},
    {"abcdef",  6, -1},
    {"abcdefg",  7, -1},
    {"abcdefgh",  8, -1},
    {"abcdefghi",  9, -1},
    {"abcdefghij", 10, -1},
    {"abcdefghijk", 11, -1},
    {"abcdefghijkl", 12, -1},
    {"abcdefghijklm", 13, -1},
    {"abcdefghijklmn", 14, -1},
    {"abcdefghijklmno", 15, -1},
    {"abcdefghijklmnop", 16, -1},
    {"abcdefghijklmnopq", 17, -1},
    {"abcdefghijklmnopqr", 18, -1},
    {"abcdefghijklmnopqrs", 19, -1},
    {"abcdefghijklmnopqrst", 20, -1},
    {"abcdefghijklmnopqrstu", 21, -1},
    {"abcdefghijklmnopqrstuv", 22, -1},
    {"abcdefghijklmnopqrstuvw", 23, -1},
    {"abcdefghijklmnopqrstuvwx", 24, -1},
    {"abcdefghijklmnopqrstuvwxy", 25, -1},
    {"abcdefghijklmnopqrstuvwxyz", 26, -1},
  };
  return cr_make_param_array(struct testcase_tuple, params, G_N_ELEMENTS(params));
}

ParameterizedTest(struct testcase_tuple *tup, findeom, test_failed)
{
  const guchar *eom;
  const guchar *msg = (const guchar *) tup->msg;

  eom = find_eom((guchar *) msg, tup-> msg_len);

  cr_assert_null(eom,
                 "EOM returned is not NULL, which was expected. eom_ofs=%d, eom=%s\n",
                 tup->eom_ofs, eom);
}
