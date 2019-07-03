/*
 * Copyright (c) 2019 Balazs Scheidler <bazsi77@gmail.com>
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
#ifndef REWRITE_SET_TIMEZONE_H_INCLUDED
#define REWRITE_SET_TIMEZONE_H_INCLUDED

#include "rewrite/rewrite-expr.h"

void rewrite_set_time_zone_set_zone_template_ref(LogRewrite *s, LogTemplate *zone_template);
void rewrite_set_time_zone_set_time_stamp(LogRewrite *s, gint stamp);

LogRewrite *rewrite_set_time_zone_new(GlobalConfig *cfg);

#endif
