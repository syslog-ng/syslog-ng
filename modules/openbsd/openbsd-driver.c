/*
 * Copyright (c) 2018 Balabit
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

#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include "openbsd-driver.h"

#include "logsource.h"
#include "messages.h"
#include "logreader.h"
#include "stats/stats-registry.h"
#include "poll-fd-events.h"
#include "logproto/logproto-dgram-server.h"
#include "transport/transport-socket.h"

#define OPENBSD_LOG_DEV  "/dev/klog"

typedef struct _openBSDDriver
{
  LogSrcDriver     super;
  LogReaderOptions reader_options;
  LogReader        *reader;
  int              klog;
  int              pair[2];
} OpenBSDDriver;

static void
_openbsd_sd_free(LogPipe *s)
{
  OpenBSDDriver *self = (OpenBSDDriver *)s;
  log_reader_options_destroy(&self->reader_options);
  log_src_driver_free(s);
}

static int
openbsd_create_newsyslog_socket(OpenBSDDriver *self)
{
  if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, self->pair) == -1)
    {
      msg_error("openBSD source: cannot bind socket fd",
                evt_tag_error(EVT_TAG_OSERROR));
      return 0;
    }

  if ((self->klog = open(OPENBSD_LOG_DEV, O_RDONLY, 0)) == -1)
    {
      msg_error("openBSD source: cannot open log device",
                evt_tag_str(EVT_TAG_FILENAME, OPENBSD_LOG_DEV),
                evt_tag_error(EVT_TAG_OSERROR));
      return 0;
    }

  if (ioctl(self->klog, LIOCSFD, &(self->pair[1])) == -1)
    {
      msg_error("openBSD source: cannot do ioctl LIOCSFD",
                evt_tag_error(EVT_TAG_OSERROR));
      return 0;
    }

  close(self->pair[1]);
  self->pair[1] = NULL;

  return self->pair[0];
}

static void
openbsd_close_newsyslog_socket(OpenBSDDriver *self)
{
  if (self->pair[0])
    close(self->pair[0]);
  self->pair[0] = NULL;

  if (self->pair[1])
    close(self->pair[1]);
  self->pair[1] = NULL;

  if (self->klog)
    close(self->klog);
  self->klog    = NULL;
}

static gboolean
_openbsd_sd_init(LogPipe *s)
{
  OpenBSDDriver *self = (OpenBSDDriver *)s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (!log_src_driver_init_method(s))
    return FALSE;

  log_reader_options_init(&self->reader_options, cfg, self->super.super.group);

  int syslog_fd = openbsd_create_newsyslog_socket(self);
  if (!syslog_fd)
    {
      msg_error("openBSD source: can't open newsyslog");
      openbsd_close_newsyslog_socket(self);
      return FALSE;
    }

  self->reader = log_reader_new(cfg);
  log_reader_open(self->reader, log_proto_dgram_server_new(log_transport_stream_socket_new(syslog_fd),
                                                           &self->reader_options.proto_options.super),
                  poll_fd_events_new(syslog_fd));

  log_reader_set_options(self->reader,
                         s,
                         &self->reader_options,
                         self->super.super.id,
                         OPENBSD_LOG_DEV);
  log_pipe_append((LogPipe *) self->reader, s);

  if (!log_pipe_init((LogPipe *) self->reader))
    {
      msg_error("Error initializing log_reader, closing fd",
                evt_tag_int("fd", self->pair[0]));
      log_pipe_unref((LogPipe *) self->reader);
      self->reader = NULL;
      openbsd_close_newsyslog_socket(self);
      return FALSE;
    }

  return TRUE;
}

static gboolean
_openbsd_sd_deinit(LogPipe *s)
{
  OpenBSDDriver *self = (OpenBSDDriver *)s;

  if (self->reader)
    {
      log_pipe_deinit((LogPipe *) self->reader);
      log_pipe_unref((LogPipe *) self->reader);
      self->reader = NULL;
      openbsd_close_newsyslog_socket(self);
    }

  if (!log_src_driver_deinit_method(s))
    return FALSE;


  return TRUE;
}

LogDriver *
openbsd_sd_new(GlobalConfig *cfg)
{
  OpenBSDDriver *self = g_new0(OpenBSDDriver, 1);

  log_src_driver_init_instance(&self->super, cfg);

  self->super.super.super.init    = _openbsd_sd_init;
  self->super.super.super.deinit  = _openbsd_sd_deinit;
  self->super.super.super.free_fn = _openbsd_sd_free;
  log_reader_options_defaults(&self->reader_options);
  self->reader_options.parse_options.flags |= LP_LOCAL;
  self->reader_options.parse_options.flags &= ~LP_EXPECT_HOSTNAME;
  self->reader_options.super.stats_level = STATS_LEVEL1;
  self->reader_options.super.stats_source = SCS_OPENBSD;

  return &self->super.super;
}
