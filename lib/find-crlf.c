/*
 * Copyright (c) 2002-2012 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
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
#include "find-crlf.h"

#include <string.h>
/**
 * This is an optimized version of finding either a CR or LF or NUL
 * character in a buffer.  It is used to find these line terminators in
 * syslog traffic.
 *
 * It uses an algorithm very similar to what there's in libc memchr/strchr.
 **/
gchar *
find_cr_or_lf(gchar *s, gsize n)
{
  gchar *char_ptr;
  gulong *longword_ptr;
  gulong longword, magic_bits, cr_charmask, lf_charmask;
  const char CR = '\r';
  const char LF = '\n';

  /* align input to long boundary */
  for (char_ptr = s; n > 0 && ((gulong) char_ptr & (sizeof(longword) - 1)) != 0; ++char_ptr, n--)
    {
      if (*char_ptr == CR || *char_ptr == LF)
        return char_ptr;
      else if (*char_ptr == 0)
        return NULL;
    }

  longword_ptr = (gulong *) char_ptr;

#if GLIB_SIZEOF_LONG == 8
  magic_bits = 0x7efefefefefefeffL;
#elif GLIB_SIZEOF_LONG == 4
  magic_bits = 0x7efefeffL;
#else
#error "unknown architecture"
#endif
  memset(&cr_charmask, CR, sizeof(cr_charmask));
  memset(&lf_charmask, LF, sizeof(lf_charmask));

  while (n > sizeof(longword))
    {
      longword = *longword_ptr++;
      if ((((longword + magic_bits) ^ ~longword) & ~magic_bits) != 0 ||
          ((((longword ^ cr_charmask) + magic_bits) ^ ~(longword ^ cr_charmask)) & ~magic_bits) != 0 ||
          ((((longword ^ lf_charmask) + magic_bits) ^ ~(longword ^ lf_charmask)) & ~magic_bits) != 0)
        {
          gint i;

          char_ptr = (gchar *) (longword_ptr - 1);

          for (i = 0; i < sizeof(longword); i++)
            {
              if (*char_ptr == CR || *char_ptr == LF)
                return char_ptr;
              else if (*char_ptr == 0)
                return NULL;
              char_ptr++;
            }
        }
      n -= sizeof(longword);
    }

  char_ptr = (gchar *) longword_ptr;

  while (n-- > 0)
    {
      if (*char_ptr == CR || *char_ptr == LF)
        return char_ptr;
      else if (*char_ptr == 0)
        return NULL;
      ++char_ptr;
    }

  return NULL;
}
