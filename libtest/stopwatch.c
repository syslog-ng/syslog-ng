/*
 * Copyright (c) 2016 Balabit
 * Copyright (c) 2016 Peter Gyorko
 * Copyright (c) 2016 Balázs Scheidler
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
#include "stopwatch.h"
#include <stdio.h>
#include <sys/time.h>

struct timeval start_time_val;

void
start_stopwatch(void)
{
  gettimeofday(&start_time_val, NULL);
}

void
stop_stopwatch_and_display_result(gint iterations, const gchar *message_template, ...)
{
  va_list args;
  guint64 diff;
  struct timeval end_time_val;
  gettimeofday(&end_time_val, NULL);

  va_start(args, message_template);
  vprintf(message_template, args);
  va_end(args);

  diff = (end_time_val.tv_sec - start_time_val.tv_sec) * 1000000 + end_time_val.tv_usec - start_time_val.tv_usec;
  printf("; %.2f iterations/sec", iterations * 1e6 / diff);
  printf(", runtime=%lu.%06lus\n", diff / 1000000, diff % 1000000);
}
