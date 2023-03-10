/*
 * Copyright (c) 2023 Hofi <hofione@gmail.com>
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

#ifndef DARWINOS_SOURCEL_H_INCLUDED
#define DARWINOS_SOURCEL_H_INCLUDED

#include "driver.h"
#include "logsource.h"

LogDriver *darwinosl_sd_new(GlobalConfig *cfg);

gboolean darwinosl_sd_set_filter_predicate(LogDriver *s, const gchar *type);
void darwinosl_sd_set_reset_postion_on_filter_change(LogDriver *s, gboolean newValue);
void darwinosl_sd_set_start_from_now(LogDriver *s, gboolean newValue);
void darwinosl_sd_set_always_start_from_now(LogDriver *s, gboolean newValue);
void darwinosl_sd_set_do_not_use_bookmark(LogDriver *s, gboolean newValue);
void darwinosl_sd_set_go_reverse(LogDriver *s, gboolean newValue);
void darwinosl_sd_set_log_fetch_reopen_delay(LogDriver *s, guint newValue);

#endif /* DARWINOS_SOURCEL_H_INCLUDED */
