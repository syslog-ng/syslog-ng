/*
 * Copyright (c) 2017 Balabit
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

#include "geoip-helper.h"

#include <messages.h>

gboolean
is_country_type(GeoIPDBTypes database_type)
{

  switch (database_type)
    {
    case GEOIP_LARGE_COUNTRY_EDITION:
    case GEOIP_COUNTRY_EDITION:
    case GEOIP_PROXY_EDITION:
    case GEOIP_NETSPEED_EDITION:
      return TRUE;

    case GEOIP_CITY_EDITION_REV0:
    case GEOIP_CITY_EDITION_REV1:
      return FALSE;

    default:
      g_assert_not_reached();
      return FALSE;
    }
}
