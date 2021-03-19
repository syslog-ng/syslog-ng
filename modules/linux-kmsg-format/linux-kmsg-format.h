/*
 * Copyright (c) 2012 Balabit
 * Copyright (c) 2012 Gergely Nagy <algernon@balabit.hu>
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

#ifndef LINUX_KMSG_FORMAT_H_INCLUDED
#define LINUX_KMSG_FORMAT_H_INCLUDED

#include "msg-format.h"

gboolean linux_kmsg_format_handler(const MsgFormatOptions *parse_options,
                                   LogMessage *msg,
                                   const guchar *data, gsize length,
                                   gsize *problem_position);

void linux_msg_format_init (void);

#endif
