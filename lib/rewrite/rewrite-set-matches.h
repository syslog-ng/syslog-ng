/*
 * Copyright (c) 2021 Balázs Scheidler <bazsi77@gmail.com>
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

#ifndef REWRITE_SET_MATCHES_H_INCLUDED
#define REWRITE_SET_MATCHES_H_INCLUDED

#include "rewrite/rewrite-expr.h"

/* LogRewriteSet */
LogRewrite *log_rewrite_set_matches_new(LogTemplate *new_value, GlobalConfig *cfg);
LogTemplateOptions *log_rewrite_set_matches_get_template_options(LogRewrite *s);

#endif
