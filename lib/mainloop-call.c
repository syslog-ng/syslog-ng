/*
 * Copyright (c) 2002-2013 Balabit
 * Copyright (c) 1998-2013 Balázs Scheidler
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
#include "mainloop-call.h"
#include "tls-support.h"

#include <iv.h>
#include <iv_list.h>
#include <iv_event.h>

/************************************************************************************
 * Cross-thread function calls into the main loop
 ************************************************************************************/
typedef struct _MainLoopTaskCallSite MainLoopTaskCallSite;
struct _MainLoopTaskCallSite
{
  struct iv_list_head list;
  MainLoopTaskFunc func;
  gpointer user_data;
  gpointer result;
  gboolean pending;
  gboolean wait;
  GCond cond;
  GMutex lock;
};

TLS_BLOCK_START
{
  MainLoopTaskCallSite call_info;
}
TLS_BLOCK_END;

#define call_info  __tls_deref(call_info)

static GMutex main_task_lock;
static struct iv_list_head main_task_queue = IV_LIST_HEAD_INIT(main_task_queue);
static struct iv_event main_task_posted;

gpointer
main_loop_call(MainLoopTaskFunc func, gpointer user_data, gboolean wait)
{
  if (main_loop_is_main_thread())
    return func(user_data);

  g_mutex_lock(&main_task_lock);

  /* check if a previous call is being executed */
  g_mutex_lock(&call_info.lock);
  if (call_info.pending)
    {
      /* yes, it is still running, indicate that we need to be woken up */
      call_info.wait = TRUE;
      g_mutex_unlock(&call_info.lock);

      while (call_info.pending)
        g_cond_wait(&call_info.cond, &main_task_lock);
    }
  else
    {
      g_mutex_unlock(&call_info.lock);
    }

  /* call_info.lock is no longer needed, since we're the only ones using call_info now */
  INIT_IV_LIST_HEAD(&call_info.list);
  call_info.pending = TRUE;
  call_info.func = func;
  call_info.user_data = user_data;
  call_info.wait = wait;
  iv_list_add(&call_info.list, &main_task_queue);
  iv_event_post(&main_task_posted);
  if (wait)
    {
      while (call_info.pending)
        g_cond_wait(&call_info.cond, &main_task_lock);
    }
  g_mutex_unlock(&main_task_lock);
  return call_info.result;
}

static void
main_loop_call_handler(gpointer user_data)
{
  g_mutex_lock(&main_task_lock);
  while (!iv_list_empty(&main_task_queue))
    {
      MainLoopTaskCallSite *site;
      gpointer result;

      site = iv_list_entry(main_task_queue.next, MainLoopTaskCallSite, list);
      iv_list_del_init(&site->list);
      g_mutex_unlock(&main_task_lock);

      result = site->func(site->user_data);

      g_mutex_lock(&site->lock);
      site->result = result;
      site->pending = FALSE;
      g_mutex_unlock(&site->lock);

      g_mutex_lock(&main_task_lock);
      if (site->wait)
        g_cond_signal(&site->cond);
    }
  g_mutex_unlock(&main_task_lock);
}

void
main_loop_call_thread_init(void)
{
  g_cond_init(&call_info.cond);
  g_mutex_init(&call_info.lock);
}

void
main_loop_call_thread_deinit(void)
{
  g_cond_clear(&call_info.cond);
  g_mutex_clear(&call_info.lock);
}

void
main_loop_call_init(void)
{
  IV_EVENT_INIT(&main_task_posted);
  main_task_posted.cookie = NULL;
  main_task_posted.handler = main_loop_call_handler;
  iv_event_register(&main_task_posted);
}

void
main_loop_call_deinit(void)
{
  iv_event_unregister(&main_task_posted);
}

