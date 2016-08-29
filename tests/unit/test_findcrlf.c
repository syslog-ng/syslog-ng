/*
 * Copyright (c) 2016 Balabit
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include <criterion/criterion.h>
#include <criterion/parameterized.h>

#include "find-crlf.h"
#include <stdio.h>
#include <stdlib.h>

struct findcrlf_params
{
  gchar *msg;
  gsize msg_len;
  gsize eom_ofs;
};

ParameterizedTestParameters(findcrlf, test)
{
  static struct findcrlf_params params[] =
  {
    { "a\nb\nc\n",  6,  1 },
    { "ab\nb\nc\n",  7,  2 },
    { "abc\nb\nc\n",  8,  3 },
    { "abcd\nb\nc\n",  9,  4 },
    { "abcde\nb\nc\n", 10,  5 },
    { "abcdef\nb\nc\n", 11,  6 },
    { "abcdefg\nb\nc\n", 12,  7 },
    { "abcdefgh\nb\nc\n", 13,  8 },
    { "abcdefghi\nb\nc\n", 14,  9 },
    { "abcdefghij\nb\nc\n", 15, 10 },
    { "abcdefghijk\nb\nc\n", 16, 11 },
    { "abcdefghijkl\nb\nc\n", 17, 12 },
    { "abcdefghijklm\nb\nc\n", 18, 13 },
    { "abcdefghijklmn\nb\nc\n", 19, 14 },
    { "abcdefghijklmno\nb\nc\n", 20, 15 },
    { "abcdefghijklmnop\nb\nc\n", 21, 16 },
    { "abcdefghijklmnopq\nb\nc\n", 22, 17 },
    { "abcdefghijklmnopqr\nb\nc\n", 23, 18 },
    { "abcdefghijklmnopqrs\nb\nc\n", 24, 19 },
    { "abcdefghijklmnopqrst\nb\nc\n", 25, 20 },
    { "abcdefghijklmnopqrstu\nb\nc\n", 26, 21 },
    { "abcdefghijklmnopqrstuv\nb\nc\n", 27, 22 },
    { "abcdefghijklmnopqrstuvw\nb\nc\n", 28, 23 },
    { "abcdefghijklmnopqrstuvwx\nb\nc\n", 29, 24 },
    { "abcdefghijklmnopqrstuvwxy\nb\nc\n", 30, 25 },
    { "abcdefghijklmnopqrstuvwxyz\nb\nc\n", 31, 26 },

    { "a\rb\rc\r",  6,  1 },
    { "ab\rb\rc\r",  7,  2 },
    { "abc\rb\rc\r",  8,  3 },
    { "abcd\rb\rc\r",  9,  4 },
    { "abcde\rb\rc\r", 10,  5 },
    { "abcdef\rb\rc\r", 11,  6 },
    { "abcdefg\rb\rc\r", 12,  7 },
    { "abcdefgh\rb\rc\r", 13,  8 },
    { "abcdefghi\rb\rc\r", 14,  9 },
    { "abcdefghij\rb\rc\r", 15, 10 },
    { "abcdefghijk\rb\rc\r", 16, 11 },
    { "abcdefghijkl\rb\rc\r", 17, 12 },
    { "abcdefghijklm\rb\rc\r", 18, 13 },
    { "abcdefghijklmn\rb\rc\r", 19, 14 },
    { "abcdefghijklmno\rb\rc\r", 20, 15 },
    { "abcdefghijklmnop\rb\rc\r", 21, 16 },
    { "abcdefghijklmnopq\rb\rc\r", 22, 17 },
    { "abcdefghijklmnopqr\rb\rc\r", 23, 18 },
    { "abcdefghijklmnopqrs\rb\rc\r", 24, 19 },
    { "abcdefghijklmnopqrst\rb\rc\r", 25, 20 },
    { "abcdefghijklmnopqrstu\rb\rc\r", 26, 21 },
    { "abcdefghijklmnopqrstuv\rb\rc\r", 27, 22 },
    { "abcdefghijklmnopqrstuvw\rb\rc\r", 28, 23 },
    { "abcdefghijklmnopqrstuvwx\rb\rc\r", 29, 24 },
    { "abcdefghijklmnopqrstuvwxy\rb\rc\r", 30, 25 },
    { "abcdefghijklmnopqrstuvwxyz\rb\rc\r", 31, 26 },

    { "a",  1, -1 },
    { "ab",  2, -1 },
    { "abc",  3, -1 },
    { "abcd",  4, -1 },
    { "abcde",  5, -1 },
    { "abcdef",  6, -1 },
    { "abcdefg",  7, -1 },
    { "abcdefgh",  8, -1 },
    { "abcdefghi",  9, -1 },
    { "abcdefghij", 10, -1 },
    { "abcdefghijk", 11, -1 },
    { "abcdefghijkl", 12, -1 },
    { "abcdefghijklm", 13, -1 },
    { "abcdefghijklmn", 14, -1 },
    { "abcdefghijklmno", 15, -1 },
    { "abcdefghijklmnop", 16, -1 },
    { "abcdefghijklmnopq", 17, -1 },
    { "abcdefghijklmnopqr", 18, -1 },
    { "abcdefghijklmnopqrs", 19, -1 },
    { "abcdefghijklmnopqrst", 20, -1 },
    { "abcdefghijklmnopqrstu", 21, -1 },
    { "abcdefghijklmnopqrstuv", 22, -1 },
    { "abcdefghijklmnopqrstuvw", 23, -1 },
    { "abcdefghijklmnopqrstuvwx", 24, -1 },
    { "abcdefghijklmnopqrstuvwxy", 25, -1 },
    { "abcdefghijklmnopqrstuvwxyz", 26, -1 }
  };

  return cr_make_param_array(struct findcrlf_params, params, sizeof (params) / sizeof(struct findcrlf_params));
}

ParameterizedTest(struct findcrlf_params *params, findcrlf, test)
{
  gchar *eom = find_cr_or_lf(params->msg, params->msg_len);

  cr_expect_not(params->eom_ofs == -1 && eom != NULL,
                "EOM returned is not NULL, which was expected. eom_ofs=%d, eom=%s\n",
                (gint) params->eom_ofs, eom);

  if (params->eom_ofs == -1)
    return;

  cr_expect_not(eom - params->msg != params->eom_ofs,
                "EOM is at wrong location. msg=%s, eom_ofs=%d, eom=%s\n",
                params->msg, (gint) params->eom_ofs, eom);
}
