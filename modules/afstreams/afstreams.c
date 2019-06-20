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

#include "afstreams.h"
#include "messages.h"
#include "logreader.h"
#include "fdhelpers.h"
#include "apphook.h"
#include "stats/stats-registry.h"
#include "poll-fd-events.h"
#include "logproto/logproto-dgram-server.h"

typedef struct _AFStreamsSourceDriver
{
  LogSrcDriver super;
  GString *dev_filename;
  GString *door_filename;
  gint door_fd;
  LogReader *reader;
  LogReaderOptions reader_options;
} AFStreamsSourceDriver;


#include <sys/types.h>
#include <sys/stat.h>
#include <stropts.h>
#include <sys/strlog.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#if SYSLOG_NG_HAVE_DOOR_H
#include <door.h>
#endif


static gssize
log_transport_streams_read(LogTransport *self, void *buf, gsize buflen, GSockAddr **sa)
{
  struct strbuf ctl, data;
  struct log_ctl lc;
  gint flags;
  gint res;
  gchar tmpbuf[buflen];
  gint len;

  ctl.maxlen = ctl.len = sizeof(lc);
  ctl.buf = (char *) &lc;
  data.maxlen = buflen;
  data.len = 0;
  data.buf = tmpbuf;
  flags = 0;

  res = getmsg(self->fd, &ctl, &data, &flags);
  if (res == -1)
    return -1;
  else if ((res & (MORECTL+MOREDATA)) == 0)
    {
      len = g_snprintf(buf, buflen, "<%d>%.*s", lc.pri, data.len, data.buf);
      return MIN(len, buflen);
    }
  else
    {
      msg_error("Insufficient buffer space for retrieving STREAMS log message",
                evt_tag_printf("res", "%x", res));
    }
  return 0;
}

LogTransport *
log_transport_streams_new(gint fd)
{
  LogTransport *self = g_new0(LogTransport, 1);

  log_transport_init_instance(self, fd);
  self->cond = G_IO_IN;
  self->read = log_transport_streams_read;
  self->free_fn = log_transport_free_method;
  return self;
}

void
afstreams_sd_set_sundoor(LogDriver *s, gchar *filename)
{
  AFStreamsSourceDriver *self = (AFStreamsSourceDriver *) s;

  self->door_filename = g_string_new(filename);
}

static void
afstreams_sd_door_server_proc(void *cookie, char *argp, size_t arg_size, door_desc_t *dp, uint_t n_desc)
{
  door_return(NULL, 0, NULL, 0);
  return;
}

static void
afstreams_init_door(int hook_type G_GNUC_UNUSED, gpointer user_data)
{
  AFStreamsSourceDriver *self = (AFStreamsSourceDriver *) user_data;
  struct stat st;
  gint fd;

  if (stat(self->door_filename->str, &st) == -1)
    {
      /* file does not exist, create it */
      fd = creat(self->door_filename->str, 0666);
      if (fd == -1)
        {
          msg_error("Error creating syslog door file",
                    evt_tag_str(EVT_TAG_FILENAME, self->door_filename->str),
                    evt_tag_error(EVT_TAG_OSERROR));
          close(fd);
          return;
        }
    }
  fdetach(self->door_filename->str);
  self->door_fd = door_create(afstreams_sd_door_server_proc, NULL, 0);
  if (self->door_fd == -1)
    {
      msg_error("Error creating syslog door",
                evt_tag_str(EVT_TAG_FILENAME, self->door_filename->str),
                evt_tag_error(EVT_TAG_OSERROR));
      return;
    }
  g_fd_set_cloexec(self->door_fd, TRUE);
  if (fattach(self->door_fd, self->door_filename->str) == -1)
    {
      msg_error("Error attaching syslog door",
                evt_tag_str(EVT_TAG_FILENAME, self->door_filename->str),
                evt_tag_error(EVT_TAG_OSERROR));
      close(self->door_fd);
      self->door_fd = -1;
      return;
    }
}

static gboolean
afstreams_sd_init(LogPipe *s)
{
  AFStreamsSourceDriver *self = (AFStreamsSourceDriver *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);
  gint fd;

  if (!log_src_driver_init_method(s))
    return FALSE;

  log_reader_options_init(&self->reader_options, cfg, self->super.super.group);

  fd = open(self->dev_filename->str, O_RDONLY | O_NOCTTY | O_NONBLOCK);
  if (fd != -1)
    {
      struct strioctl ioc;

      g_fd_set_cloexec(fd, TRUE);
      memset(&ioc, 0, sizeof(ioc));
      ioc.ic_cmd = I_CONSLOG;
      if (ioctl(fd, I_STR, &ioc) < 0)
        {
          msg_error("Error in ioctl(I_STR, I_CONSLOG)",
                    evt_tag_str(EVT_TAG_FILENAME, self->dev_filename->str),
                    evt_tag_error(EVT_TAG_OSERROR));
          close(fd);
          return FALSE;
        }
      g_fd_set_nonblock(fd, TRUE);
      self->reader = log_reader_new(cfg);
      log_reader_open(self->reader, log_proto_dgram_server_new(log_transport_streams_new(fd),
                                                               &self->reader_options.proto_options.super), poll_fd_events_new(fd));
      log_reader_set_options(self->reader,
                             s,
                             &self->reader_options,
                             self->super.super.id,
                             self->dev_filename->str);
      log_pipe_append((LogPipe *) self->reader, s);

      if (self->door_filename)
        {

          /* door creation is deferred, because it creates threads which is
           * not inherited through forks, and syslog-ng forks during
           * startup, but _after_ the configuration was initialized */

          register_application_hook(AH_POST_DAEMONIZED, afstreams_init_door, self);
        }
      if (!log_pipe_init((LogPipe *) self->reader))
        {
          msg_error("Error initializing log_reader, closing fd",
                    evt_tag_int("fd", fd));
          log_pipe_unref((LogPipe *) self->reader);
          self->reader = NULL;
          close(fd);
          return FALSE;
        }

    }
  else
    {
      msg_error("Error opening syslog device",
                evt_tag_str(EVT_TAG_FILENAME, self->dev_filename->str),
                evt_tag_error(EVT_TAG_OSERROR));
      return FALSE;
    }
  return TRUE;
}

static gboolean
afstreams_sd_deinit(LogPipe *s)
{
  AFStreamsSourceDriver *self = (AFStreamsSourceDriver *) s;

  if (self->reader)
    {
      log_pipe_deinit((LogPipe *) self->reader);
      log_pipe_unref((LogPipe *) self->reader);
      self->reader = NULL;
    }
  if (self->door_fd != -1)
    {
      door_revoke(self->door_fd);
      close(self->door_fd);
    }

  if (!log_src_driver_deinit_method(s))
    return FALSE;

  return TRUE;
}

static void
afstreams_sd_free(LogPipe *s)
{
  AFStreamsSourceDriver *self = (AFStreamsSourceDriver *) s;

  log_reader_options_destroy(&self->reader_options);
  if (self->dev_filename)
    g_string_free(self->dev_filename, TRUE);
  if (self->door_filename)
    g_string_free(self->door_filename, TRUE);

  log_src_driver_free(s);
}

LogDriver *
afstreams_sd_new(gchar *filename, GlobalConfig *cfg)
{
  AFStreamsSourceDriver *self = g_new0(AFStreamsSourceDriver, 1);

  log_src_driver_init_instance(&self->super, cfg);

  self->dev_filename = g_string_new(filename);
  self->super.super.super.init = afstreams_sd_init;
  self->super.super.super.deinit = afstreams_sd_deinit;
  self->super.super.super.free_fn = afstreams_sd_free;
  log_reader_options_defaults(&self->reader_options);
  self->reader_options.parse_options.flags |= LP_LOCAL;
  self->reader_options.parse_options.flags &= ~LP_EXPECT_HOSTNAME;
  self->reader_options.super.stats_level = STATS_LEVEL1;
  self->reader_options.super.stats_source = stats_register_type("sun-streams");
  return &self->super.super;
}
