/*
 * Copyright (c) 2002-2012 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
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

#include "apphook.h"
#include "messages.h"
#include "children.h"
#include "dnscache.h"
#include "alarms.h"
#include "stats/stats-registry.h"
#include "metrics/metrics.h"
#include "healthcheck/healthcheck-stats.h"
#include "logmsg/logmsg.h"
#include "logsource.h"
#include "logwriter.h"
#include "afinter.h"
#include "template/globals.h"
#include "hostname.h"
#include "mainloop-call.h"
#include "service-management.h"
#include "crypto.h"
#include "value-pairs/value-pairs.h"
#include "scratch-buffers.h"
#include "mainloop.h"
#include "secret-storage/nondumpable-allocator.h"
#include "secret-storage/secret-storage.h"
#include "timeutils/timeutils.h"
#include "msg-stats.h"
#include "timeutils/cache.h"
#include "multi-line/multi-line-factory.h"
#include "filterx/filterx-globals.h"
#include "transport/transport-globals.h"

#include <iv.h>
#include <iv_work.h>

typedef struct _ApplicationHookEntry
{
  gint type;
  ApplicationHookRunMode run_mode;
  ApplicationHookFunc func;
  gpointer user_data;
} ApplicationHookEntry;

typedef struct _ApplicationThreadHookEntry
{
  ApplicationThreadHookFunc func;
  gpointer user_data;
} ApplicationThreadHookEntry;

static GList *application_hooks = NULL;
static GList *application_thread_init_hooks = NULL;
static GList *application_thread_deinit_hooks = NULL;
static gint current_state = AH_STARTUP;

gboolean
app_is_starting_up(void)
{
  return current_state < AH_RUNNING;
}

gboolean
app_is_shutting_down(void)
{
  return current_state >= AH_PRE_SHUTDOWN;
}

void
register_application_hook(gint type, ApplicationHookFunc func, gpointer user_data, ApplicationHookRunMode run_mode)
{
  if (type > __AH_STATE_MAX || current_state < type)
    {
      ApplicationHookEntry *entry = g_new0(ApplicationHookEntry, 1);

      entry->type = type;
      entry->func = func;
      entry->user_data = user_data;
      entry->run_mode = run_mode;

      application_hooks = g_list_prepend(application_hooks, entry);
    }
  else
    {
      /* the requested hook has already passed, call the requested function immediately */
      msg_debug("Application hook registered after the given point passed",
                evt_tag_int("current", current_state),
                evt_tag_int("hook", type));
      func(type, user_data);
    }
}

void
register_application_thread_init_hook(ApplicationThreadHookFunc func, gpointer user_data)
{
  ApplicationThreadHookEntry *entry = g_new0(ApplicationThreadHookEntry, 1);

  entry->func = func;
  entry->user_data = user_data;

  application_thread_init_hooks = g_list_prepend(application_thread_init_hooks, entry);
}

void
register_application_thread_deinit_hook(ApplicationThreadHookFunc func, gpointer user_data)
{
  ApplicationThreadHookEntry *entry = g_new0(ApplicationThreadHookEntry, 1);

  entry->func = func;
  entry->user_data = user_data;

  application_thread_deinit_hooks = g_list_prepend(application_thread_deinit_hooks, entry);
}

static void
run_application_thread_init_hooks(void)
{
  for (GList *elem = application_thread_init_hooks; elem; elem = elem->next)
    {
      ApplicationThreadHookEntry *hook = (ApplicationThreadHookEntry *) elem->data;
      hook->func(hook->user_data);
    }
}

static void
run_application_thread_deinit_hooks(void)
{
  for (GList *elem = application_thread_deinit_hooks; elem; elem = elem->next)
    {
      ApplicationThreadHookEntry *hook = (ApplicationThreadHookEntry *) elem->data;
      hook->func(hook->user_data);
    }
}

static void
_update_current_state(gint type)
{
  if (type > __AH_STATE_MAX)
    return;

  g_assert(current_state <= type);
  current_state = type;
}

static void
run_application_hook(gint type)
{
  GList *l, *l_next;

  _update_current_state(type);

  msg_debug("Running application hooks", evt_tag_int("hook", type));
  for (l = application_hooks; l; l = l_next)
    {
      ApplicationHookEntry *e = l->data;

      if (e->type == type)
        {
          l_next = l->next;
          e->func(type, e->user_data);

          if (e->run_mode == AHM_RUN_ONCE)
            {
              application_hooks = g_list_remove_link(application_hooks, l);
              g_free(e);
              g_list_free_1(l);
            }
          else
            {
              g_assert(e->run_mode == AHM_RUN_REPEAT);
            }
        }
      else
        {
          l_next = l->next;
        }
    }

}

static void
app_fatal(const char *msg)
{
  fprintf(stderr, "%s\n", msg);
}

#define construct_nondumpable_logger(logger) \
void \
nondumpable_allocator_ ## logger (gchar *summary, gchar *reason) \
{ \
  logger(summary, evt_tag_str("reason", reason)); \
}

construct_nondumpable_logger(msg_debug);
construct_nondumpable_logger(msg_fatal);

void
app_startup(void)
{
  msg_init(FALSE);
  iv_set_fatal_msg_handler(app_fatal);
  iv_init();
  crypto_init();
  hostname_global_init();
  dns_caching_global_init();
  dns_caching_thread_init();
  afinter_global_init();
  child_manager_init();
  alarm_init();
  main_loop_thread_resource_init();
  stats_init();
  metrics_global_init();
  healthcheck_stats_global_init();
  tzset();
  log_msg_global_init();
  log_source_global_init();
  log_template_global_init();
  value_pairs_global_init();
  service_management_init();
  scratch_buffers_allocator_init();
  nondumpable_setlogger(nondumpable_allocator_msg_debug, nondumpable_allocator_msg_fatal);
  secret_storage_init();
  scratch_buffers_global_init();
  msg_stats_init();
  timeutils_global_init();
  multi_line_global_init();
  filterx_global_init();
  log_transport_global_init();
}

void
app_post_daemonized(void)
{
  run_application_hook(AH_POST_DAEMONIZED);
}

void
app_running(void)
{
  run_application_hook(AH_RUNNING);
}


void
app_pre_shutdown(void)
{
  run_application_hook(AH_PRE_SHUTDOWN);
}

void
app_shutdown(void)
{
  msg_stats_deinit();
  run_application_hook(AH_SHUTDOWN);

  log_transport_global_deinit();
  filterx_global_deinit();
  multi_line_global_deinit();
  main_loop_thread_resource_deinit();
  secret_storage_deinit();
  scratch_buffers_allocator_deinit();
  scratch_buffers_global_deinit();
  value_pairs_global_deinit();
  log_template_global_deinit();
  log_msg_global_deinit();

  afinter_global_deinit();
  metrics_global_deinit();
  stats_destroy();
  child_manager_deinit();
  g_list_foreach(application_hooks, (GFunc) g_free, NULL);
  g_list_free(application_hooks);
  g_list_free_full(application_thread_init_hooks, g_free);
  g_list_free_full(application_thread_deinit_hooks, g_free);
  dns_caching_thread_deinit();
  dns_caching_global_deinit();
  hostname_global_deinit();
  crypto_deinit();
  msg_deinit();


  /* NOTE: the iv_deinit() call should come here, but there's some exit
   * synchronization issue in libivykis that causes use-after-free with the
   * thread-local-state for the main thread and iv_work_pool worker threads.
   * I've dropped a mail to Lennert about the issue, but I'm commenting this
   * out for now to avoid it biting someone. Bazsi, 2013/12/23.
   *
   *

    iv_deinit();

   */
}

void
app_config_pre_pre_init(void)
{
  run_application_hook(AH_CONFIG_PRE_PRE_INIT);
}

void
app_config_pre_init(void)
{
  run_application_hook(AH_CONFIG_PRE_INIT);
}

void
app_config_stopped(void)
{
  run_application_hook(AH_CONFIG_STOPPED);
}

void
app_config_changed(void)
{
  run_application_hook(AH_CONFIG_CHANGED);
}

void
app_reopen_files(void)
{
  run_application_hook(AH_REOPEN_FILES);
}

void
app_thread_start(void)
{
  scratch_buffers_allocator_init();
  dns_caching_thread_init();
  main_loop_call_thread_init();
  run_application_thread_init_hooks();
}

void
app_thread_stop(void)
{
  run_application_thread_deinit_hooks();
  main_loop_call_thread_deinit();
  dns_caching_thread_deinit();
  scratch_buffers_allocator_deinit();
  timeutils_cache_deinit();
}
