/*
 * Copyright (c) 2018 Balabit
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

#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>

#include "loggen_plugin.h"
#include "loggen_helper.h"

gboolean
thread_check_exit_criteria(ThreadData *thread_context)
{
  if (thread_context->option->number_of_messages != 0
      && thread_context->sent_messages >= thread_context->option->number_of_messages)
    {
      DEBUG("(thread %d) number of sent messages reached the defined limit (%d)\n", thread_context->index,
            thread_context->option->number_of_messages);
      return TRUE;
    }

  struct timeval now;
  gettimeofday(&now, NULL);

  if ( !thread_context->option->permanent &&
       time_val_diff_in_sec(&now, &thread_context->start_time) > thread_context->option->interval )
    {
      DEBUG("(thread %d) defined time (%d sec) ellapsed\n", thread_context->index, thread_context->option->interval);
      return TRUE;
    }

  return FALSE;
}

gboolean
thread_check_time_bucket(ThreadData *thread_context)
{
  struct timeval now;
  gettimeofday(&now, NULL);

  double diff_usec = time_val_diff_in_usec(&now, &thread_context->last_throttle_check);
  if (thread_context->buckets == 0 || diff_usec > 1e5)
    {
      /* check rate every 0.1sec */
      long new_buckets = (long)((thread_context->option->rate * diff_usec) / USEC_PER_SEC);
      if (new_buckets)
        {
          thread_context->buckets = (thread_context->option->rate < thread_context->buckets + new_buckets) ?
                                    thread_context->option->rate : thread_context->buckets + new_buckets;
          thread_context->last_throttle_check = now;
        }
    }

  if (thread_context->buckets == 0)
    {
      struct timespec tspec;
      long msec = (1000 / thread_context->option->rate) + 1;

      tspec.tv_sec = msec / 1000;
      tspec.tv_nsec = (msec % 1000) * 1e6;
      while (nanosleep(&tspec, &tspec) < 0 && errno == EINTR)
        ;
      return TRUE;
    }

  return FALSE;
}

