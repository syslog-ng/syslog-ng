/*
 * Copyright (c) 2014 Balabit
 * Copyright (c) 2014 Zoltan Fried
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

#ifndef FILTER_NETMASK6_H_INCLUDED
#define FILTER_NETMASK6_H_INCLUDED

#include "filter-expr.h"

FilterExprNode *filter_netmask6_new(const gchar *cidr);
void get_network_address(const struct in6_addr *address, int prefix, struct in6_addr *network);

#endif
