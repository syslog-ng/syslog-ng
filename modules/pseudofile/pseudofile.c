/*
 * Copyright (c) 2002-2013 Balabit
 * Copyright (c) 1998-2013 Balázs Scheidler
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

#include "pseudofile.h"
#include "messages.h"
#include "scratch-buffers.h"

#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

typedef struct _PseudoFileDestDriver
{
  LogDestDriver super;
  LogTemplateOptions template_options;
  LogTemplate *template;
  gchar *pseudofile_name;
  time_t suspend_until;
} PseudoFileDestDriver;

/*
 * Locking
 * =======
 *
 * The pseudofile driver currently has a single, global lock that protects
 * writing the data into the procfile.
 *
 * We could use a per-destination lock, however in that case nothing would
 * prevent races when two pseudofile drivers open the same proc file.
 *
 * Also this driver shouldn't be that performance intensive (e.g.  pushing
 * data to a procfile more than a dozen times per second is really sick),
 * I've just measured pseudofile to be able to do 25-26k/sec easily in my
 * debug build.
 */
G_LOCK_DEFINE_STATIC(pseudofile_lock);

LogTemplateOptions *
pseudofile_dd_get_template_options(LogDriver *s)
{
  PseudoFileDestDriver *self = (PseudoFileDestDriver *) s;

  return &self->template_options;
}

void
pseudofile_dd_set_template(LogDriver *s, LogTemplate *template)
{
  PseudoFileDestDriver *self = (PseudoFileDestDriver *) s;

  log_template_unref(self->template);
  self->template = template;
}

static void
_format_message(PseudoFileDestDriver *self, LogMessage *msg, GString *formatted_message)
{
  log_template_format(self->template, msg, &self->template_options, LTZ_LOCAL, 0, NULL, formatted_message);
}

static EVTTAG *
_evt_tag_message(const GString *msg)
{
  const int max_len = 30;

  return evt_tag_printf("message", "%.*s%s",
                        (int) MIN(max_len, msg->len), msg->str,
                        msg->len > max_len ? "..." : "");
}

static gboolean
_write_message(PseudoFileDestDriver *self, const GString *msg)
{
  int fd;
  gboolean success = FALSE;
  gint rc;

  msg_debug("Posting message to pseudo file",
            evt_tag_str("pseudofile", self->pseudofile_name),
            evt_tag_str("driver", self->super.super.id),
            _evt_tag_message(msg));
  fd = open(self->pseudofile_name, O_NOCTTY | O_WRONLY | O_NONBLOCK);
  if (fd < 0)
    {
      msg_error("Error opening pseudo file",
                evt_tag_str("pseudofile", self->pseudofile_name),
                evt_tag_str("driver", self->super.super.id),
                evt_tag_error("error"),
                _evt_tag_message(msg));
      goto exit;
    }

  rc = write(fd, msg->str, msg->len);
  if (rc < 0)
    {
      msg_error("Error writing to pseudo file",
                evt_tag_str("pseudofile", self->pseudofile_name),
                evt_tag_str("driver", self->super.super.id),
                evt_tag_error("error"),
                _evt_tag_message(msg));
      goto exit;
    }
  else if (rc != msg->len)
    {
      msg_error("Partial write to pseudofile, probably the output is too much for the kernel to consume",
                evt_tag_str("pseudofile", self->pseudofile_name),
                evt_tag_str("driver", self->super.super.id),
                _evt_tag_message(msg));
      goto exit;
    }

  success = TRUE;

exit:
  if (fd >= 0)
    close(fd);

  return success;
}

static gboolean
_is_output_suspended(PseudoFileDestDriver *self, time_t now)
{
  if (self->suspend_until && self->suspend_until > now)
    return TRUE;
  return FALSE;
}

static void
_suspend_output(PseudoFileDestDriver *self, time_t now)
{
  GlobalConfig *cfg = log_pipe_get_config(&self->super.super.super);

  self->suspend_until = now + cfg->time_reopen;
}

static void
pseudofile_dd_queue(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options)
{
  PseudoFileDestDriver *self = (PseudoFileDestDriver *) s;
  GString *formatted_message = scratch_buffers_alloc();
  gboolean success;
  time_t now = msg->timestamps[LM_TS_RECVD].tv_sec;

  if (_is_output_suspended(self, now))
    goto finish;

  _format_message(self, msg, formatted_message);

  G_LOCK(pseudofile_lock);
  success = _write_message(self, formatted_message);
  G_UNLOCK(pseudofile_lock);

  if (!success)
    _suspend_output(self, now);

finish:
  log_dest_driver_queue_method(s, msg, path_options);
}

static gboolean
pseudofile_dd_init(LogPipe *s)
{
  PseudoFileDestDriver *self = (PseudoFileDestDriver *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  log_template_options_init(&self->template_options, cfg);
  return log_dest_driver_init_method(s);
}

static void
pseudofile_dd_free(LogPipe *s)
{
  PseudoFileDestDriver *self = (PseudoFileDestDriver *) s;

  log_template_options_destroy(&self->template_options);
  g_free(self->pseudofile_name);
  log_template_unref(self->template);
  log_dest_driver_free(s);
}

LogDriver *
pseudofile_dd_new(gchar *pseudofile_name, GlobalConfig *cfg)
{
  PseudoFileDestDriver *self = g_new0(PseudoFileDestDriver, 1);

  log_dest_driver_init_instance(&self->super, cfg);
  log_template_options_defaults(&self->template_options);
  self->super.super.super.init = pseudofile_dd_init;
  self->super.super.super.queue = pseudofile_dd_queue;
  self->super.super.super.free_fn = pseudofile_dd_free;
  self->pseudofile_name = g_strdup(pseudofile_name);
  return &self->super.super;
}
