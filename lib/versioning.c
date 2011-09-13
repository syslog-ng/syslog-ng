/*
 * Copyright (c) 2002-2011 BalaBit IT Ltd, Budapest, Hungary
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
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
#include <glib.h>
#include "versioning.h"

/*
  return the converted version value (OSE <-> PE versions)
*/
gint
get_version_value(gint version)
{
  gint ver_main = (version >> 8) & 0xFF;
  gint ver_sub = version & 0xFF;
  if (ver_main == 3 && ver_sub == 0)
    {
      /* no change */
    }
  else if (ver_main == 3 && ver_sub == 3)
    {
      ver_main = 4;
      ver_sub = 1;
    }
  else if (ver_main == 3 && (ver_sub == 1 || ver_sub == 2))
    {
      ver_main = 4;
      ver_sub = 0;
    }
  else if (ver_main == 4 && ver_sub == 1)
    {
      /* no change */
    }

  return (ver_main << 8) | ver_sub;
}
