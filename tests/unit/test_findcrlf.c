/*
 * Copyright (c) 2008 Balabit
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

#include "find-crlf.h"
#include <stdio.h>
#include <stdlib.h>

static void
testcase(gchar *msg, gsize msg_len, gsize eom_ofs)
{
  gchar *eom;

  eom = find_cr_or_lf(msg, msg_len);

  if (eom_ofs == -1 && eom != NULL)
    {
      fprintf(stderr, "EOM returned is not NULL, which was expected. eom_ofs=%d, eom=%s\n", (gint) eom_ofs, eom);
      exit(1);
    }
  if (eom_ofs == -1)
    return;

  if (eom - msg != eom_ofs)
    {
      fprintf(stderr, "EOM is at wrong location. msg=%s, eom_ofs=%d, eom=%s\n", msg, (gint) eom_ofs, eom);
      exit(1);
    }
}

int
main()
{
  testcase("a\nb\nc\n",  6,  1);
  testcase("ab\nb\nc\n",  7,  2);
  testcase("abc\nb\nc\n",  8,  3);
  testcase("abcd\nb\nc\n",  9,  4);
  testcase("abcde\nb\nc\n", 10,  5);
  testcase("abcdef\nb\nc\n", 11,  6);
  testcase("abcdefg\nb\nc\n", 12,  7);
  testcase("abcdefgh\nb\nc\n", 13,  8);
  testcase("abcdefghi\nb\nc\n", 14,  9);
  testcase("abcdefghij\nb\nc\n", 15, 10);
  testcase("abcdefghijk\nb\nc\n", 16, 11);
  testcase("abcdefghijkl\nb\nc\n", 17, 12);
  testcase("abcdefghijklm\nb\nc\n", 18, 13);
  testcase("abcdefghijklmn\nb\nc\n", 19, 14);
  testcase("abcdefghijklmno\nb\nc\n", 20, 15);
  testcase("abcdefghijklmnop\nb\nc\n", 21, 16);
  testcase("abcdefghijklmnopq\nb\nc\n", 22, 17);
  testcase("abcdefghijklmnopqr\nb\nc\n", 23, 18);
  testcase("abcdefghijklmnopqrs\nb\nc\n", 24, 19);
  testcase("abcdefghijklmnopqrst\nb\nc\n", 25, 20);
  testcase("abcdefghijklmnopqrstu\nb\nc\n", 26, 21);
  testcase("abcdefghijklmnopqrstuv\nb\nc\n", 27, 22);
  testcase("abcdefghijklmnopqrstuvw\nb\nc\n", 28, 23);
  testcase("abcdefghijklmnopqrstuvwx\nb\nc\n", 29, 24);
  testcase("abcdefghijklmnopqrstuvwxy\nb\nc\n", 30, 25);
  testcase("abcdefghijklmnopqrstuvwxyz\nb\nc\n", 31, 26);

  testcase("a\rb\rc\r",  6,  1);
  testcase("ab\rb\rc\r",  7,  2);
  testcase("abc\rb\rc\r",  8,  3);
  testcase("abcd\rb\rc\r",  9,  4);
  testcase("abcde\rb\rc\r", 10,  5);
  testcase("abcdef\rb\rc\r", 11,  6);
  testcase("abcdefg\rb\rc\r", 12,  7);
  testcase("abcdefgh\rb\rc\r", 13,  8);
  testcase("abcdefghi\rb\rc\r", 14,  9);
  testcase("abcdefghij\rb\rc\r", 15, 10);
  testcase("abcdefghijk\rb\rc\r", 16, 11);
  testcase("abcdefghijkl\rb\rc\r", 17, 12);
  testcase("abcdefghijklm\rb\rc\r", 18, 13);
  testcase("abcdefghijklmn\rb\rc\r", 19, 14);
  testcase("abcdefghijklmno\rb\rc\r", 20, 15);
  testcase("abcdefghijklmnop\rb\rc\r", 21, 16);
  testcase("abcdefghijklmnopq\rb\rc\r", 22, 17);
  testcase("abcdefghijklmnopqr\rb\rc\r", 23, 18);
  testcase("abcdefghijklmnopqrs\rb\rc\r", 24, 19);
  testcase("abcdefghijklmnopqrst\rb\rc\r", 25, 20);
  testcase("abcdefghijklmnopqrstu\rb\rc\r", 26, 21);
  testcase("abcdefghijklmnopqrstuv\rb\rc\r", 27, 22);
  testcase("abcdefghijklmnopqrstuvw\rb\rc\r", 28, 23);
  testcase("abcdefghijklmnopqrstuvwx\rb\rc\r", 29, 24);
  testcase("abcdefghijklmnopqrstuvwxy\rb\rc\r", 30, 25);
  testcase("abcdefghijklmnopqrstuvwxyz\rb\rc\r", 31, 26);

  testcase("a",  1, -1);
  testcase("ab",  2, -1);
  testcase("abc",  3, -1);
  testcase("abcd",  4, -1);
  testcase("abcde",  5, -1);
  testcase("abcdef",  6, -1);
  testcase("abcdefg",  7, -1);
  testcase("abcdefgh",  8, -1);
  testcase("abcdefghi",  9, -1);
  testcase("abcdefghij", 10, -1);
  testcase("abcdefghijk", 11, -1);
  testcase("abcdefghijkl", 12, -1);
  testcase("abcdefghijklm", 13, -1);
  testcase("abcdefghijklmn", 14, -1);
  testcase("abcdefghijklmno", 15, -1);
  testcase("abcdefghijklmnop", 16, -1);
  testcase("abcdefghijklmnopq", 17, -1);
  testcase("abcdefghijklmnopqr", 18, -1);
  testcase("abcdefghijklmnopqrs", 19, -1);
  testcase("abcdefghijklmnopqrst", 20, -1);
  testcase("abcdefghijklmnopqrstu", 21, -1);
  testcase("abcdefghijklmnopqrstuv", 22, -1);
  testcase("abcdefghijklmnopqrstuvw", 23, -1);
  testcase("abcdefghijklmnopqrstuvwx", 24, -1);
  testcase("abcdefghijklmnopqrstuvwxy", 25, -1);
  testcase("abcdefghijklmnopqrstuvwxyz", 26, -1);

  return 0;
}
