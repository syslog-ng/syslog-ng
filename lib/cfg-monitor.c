/*
 * Copyright (c) 2023 László Várady
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

#include "cfg-monitor.h"
#include "messages.h"
#include "resolved-configurable-paths.h"
#include "timeutils/misc.h"

#include <iv.h>

#if SYSLOG_NG_HAVE_INOTIFY
#include <iv_inotify.h>
#endif

#define CFG_MONITOR_POLL_FREQ (15 * 60)

typedef struct _CfgMonitorCallbackListItem
{
  CfgMonitorEventCB cb;
  gpointer cb_data;
} CfgMonitorCallbackListItem;

struct _CfgMonitor
{
  GList *callbacks;

  struct iv_timer poll_timer;
  time_t last_mtime;

#if SYSLOG_NG_HAVE_INOTIFY
  gboolean in_started;
  const gchar *cfgfile_basename;
  struct iv_inotify inotify;
  struct iv_inotify_watch in_watch;
#endif
};


void
cfg_monitor_add_watch(CfgMonitor *self, CfgMonitorEventCB cb,  gpointer cb_data)
{
  if (!cb)
    return;

  CfgMonitorCallbackListItem *item = g_new(CfgMonitorCallbackListItem, 1);
  item->cb = cb;
  item->cb_data = cb_data;

  self->callbacks = g_list_prepend(self->callbacks, item);
}

static gint
_compare_callback(gconstpointer a, gconstpointer b)
{
  const CfgMonitorCallbackListItem *item_a = (const CfgMonitorCallbackListItem *) a;
  const CfgMonitorCallbackListItem *item_b = (const CfgMonitorCallbackListItem *) b;

  gboolean equal = item_a->cb == item_b->cb && item_a->cb_data == item_b->cb_data;
  return equal ? 0 : -1;
}

void
cfg_monitor_remove_watch(CfgMonitor *self, CfgMonitorEventCB cb, gpointer cb_data)
{
  CfgMonitorCallbackListItem item = { cb, cb_data };
  GList *i = g_list_find_custom(self->callbacks, &item, _compare_callback);

  if (!i)
    return;

  CfgMonitorCallbackListItem *removed_item = i->data;
  self->callbacks = g_list_delete_link(self->callbacks, i);
  g_free(removed_item);
}

static void
_run_callbacks(CfgMonitor *self, const CfgMonitorEvent *event)
{
  for (GList *i = self->callbacks; i; i = i->next)
    {
      CfgMonitorCallbackListItem *item = i->data;
      item->cb(event, item->cb_data);
    }
}

static void
_run_callbacks_if_main_config_was_modified(CfgMonitor *self)
{
  const gchar *main_cfg_file = resolved_configurable_paths.cfgfilename;
  struct stat st = {0};
  stat(main_cfg_file, &st);

  if (self->last_mtime >= st.st_mtime)
    return;

  CfgMonitorEvent event =
  {
    .name = main_cfg_file,
    .event = MODIFIED,
    .st = st,
  };

  _run_callbacks(self, &event);
  self->last_mtime = st.st_mtime;
}

static void
_poll_start(CfgMonitor *self)
{
  if (iv_timer_registered(&self->poll_timer))
    return;

  iv_validate_now();
  self->poll_timer.expires = iv_now;
  timespec_add_msec(&self->poll_timer.expires, CFG_MONITOR_POLL_FREQ * 1000);
  iv_timer_register(&self->poll_timer);
}

static void
_poll_stop(CfgMonitor *self)
{
  if (iv_timer_registered(&self->poll_timer))
    iv_timer_unregister(&self->poll_timer);
}

static void
_poll_timer_tick(gpointer c)
{
  CfgMonitor *self = (CfgMonitor *) c;

  _run_callbacks_if_main_config_was_modified(self);

  _poll_start(self);
}

#if SYSLOG_NG_HAVE_INOTIFY

static void _inotify_event_handler(void *c, struct inotify_event *event)
{
  CfgMonitor *self = (CfgMonitor *) c;

  if (g_strcmp0(self->cfgfile_basename, event->name) == 0)
    _run_callbacks_if_main_config_was_modified(self);
}

/* moving/recreating the config directory itself is not implemented */
static gboolean
_inotify_start(CfgMonitor *self)
{
  if (self->in_started)
    return FALSE;

  IV_INOTIFY_INIT(&self->inotify);
  if (iv_inotify_register(&self->inotify) != 0)
    {
      msg_warning("Error creating configuration monitor instance, can not register inotify", evt_tag_error("errno"));
      return FALSE;
    }

  gchar *config_dir = g_path_get_dirname(resolved_configurable_paths.cfgfilename);

  IV_INOTIFY_WATCH_INIT(&self->in_watch);
  self->in_watch.inotify = &self->inotify;
  self->in_watch.pathname = config_dir;
  self->in_watch.mask = IN_CREATE | IN_MOVED_TO | IN_CLOSE_WRITE;
  self->in_watch.handler = _inotify_event_handler;
  self->in_watch.cookie = self;

  if (iv_inotify_watch_register(&self->in_watch) != 0)
    {
      iv_inotify_unregister(&self->inotify);
      g_free(config_dir);
      msg_warning("Error start configuration monitor, can not register inotify watch", evt_tag_error("errno"));
      return FALSE;
    }

  g_free(config_dir);

  self->cfgfile_basename = g_path_get_basename(resolved_configurable_paths.cfgfilename);

  self->in_started = TRUE;
  return TRUE;
}

static void
_inotify_stop(CfgMonitor *self)
{
  if (!self->in_started)
    return;

  iv_inotify_watch_unregister(&self->in_watch);
  iv_inotify_unregister(&self->inotify);

  g_free((gchar *) self->cfgfile_basename);

  self->in_started = FALSE;
}

#else
#define _inotify_start(self) (FALSE)
#define _inotify_stop(self)
#endif

void cfg_monitor_start(CfgMonitor *self)
{
  if (!_inotify_start(self))
    _poll_start(self);

  _run_callbacks_if_main_config_was_modified(self);
}

void cfg_monitor_stop(CfgMonitor *self)
{
  _inotify_stop(self);
  _poll_stop(self);
}

void
cfg_monitor_free(CfgMonitor *self)
{
  g_list_free_full(self->callbacks, g_free);
  g_free(self);
}

CfgMonitor *
cfg_monitor_new(void)
{
  CfgMonitor *self = g_new0(CfgMonitor, 1);

  IV_TIMER_INIT(&self->poll_timer);
  self->poll_timer.handler = _poll_timer_tick;
  self->poll_timer.cookie = self;

  return self;
}
