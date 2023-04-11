/*
 * Copyright (c) 2023 Attila Szakacs
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

#ifndef DISKQ_GLOBAL_METRICS_H_
#define DISKQ_GLOBAL_METRICS_H_

#include "syslog-ng.h"

void diskq_global_metrics_init(void);
void diskq_global_metrics_file_acquired(const gchar *abs_filename);
void diskq_global_metrics_file_released(const gchar *abs_filename);

#endif /* DISKQ_GLOBAL_METRICS_H_ */
