/*
 * Copyright (c) 2002, 2003, 2004 BalaBit IT Ltd, Budapest, Hungary
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Note that this permission is granted for only version 2 of the GPL.
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "afstreams.h"
#include "messages.h"
#include "logreader.h"


typedef struct _AFStreamsSourceDriver
{
  LogDriver super;
  GString *dev_filename;
  GString *door_filename;
  gint door_fd;
  LogPipe *reader;
  LogReaderOptions reader_options;
} AFStreamsSourceDriver;


#if ENABLE_SUN_STREAMS

#include <sys/types.h>
#include <sys/stat.h>
#include <stropts.h>
#include <sys/strlog.h>
#include <fcntl.h>
#include <string.h>

#if ENABLE_SUN_DOOR && HAVE_DOOR_H
#include <door.h>
#endif


static size_t
streams_read_read_method(FDRead *self, void *buf, size_t buflen, GSockAddr **sa)
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
  if ((res & (MORECTL+MOREDATA)) == 0)
    {
      len = g_snprintf(buf, buflen, "<%d>%.*s", lc.pri, data.len, data.buf);
      return MIN(len, buflen);
    }
  else
    {
      msg_error("Insufficient buffer space for retrieving STREAMS log message",
                evt_tag_printf("res", "%x", res),
                NULL);
    }
  return 0;
}

FDRead *
streams_read_new(gint fd)
{
  FDRead *self = g_new0(FDRead, 1);
  
  self->fd = fd;
  self->cond = G_IO_IN;
  self->read = streams_read_read_method;
  return self;
}

void 
afstreams_sd_set_sundoor(LogDriver *s, gchar *filename)
{
  AFStreamsSourceDriver *self = (AFStreamsSourceDriver *) s;
  
  self->door_filename = g_string_new(filename);
}

static void 
afstreams_sd_door_server_proc(void *cookie, char *argp, size_t arg_size, door_desc_t *dp, size_t n_desc)
{
  door_return(NULL, 0, NULL, 0);
  return;
}

static gboolean
afstreams_sd_init(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist)
{
  AFStreamsSourceDriver *self = (AFStreamsSourceDriver *) s;
  gint fd;
  
  log_reader_options_init(&self->reader_options, cfg);
  
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
                    evt_tag_errno(EVT_TAG_OSERROR, errno),
                    NULL);
          close(fd);
          return FALSE;
        }
      self->reader = log_reader_new(streams_read_new(fd), LR_LOCAL | LR_NOMREAD, s, &self->reader_options);
      log_pipe_append(self->reader, s);
      
      if (self->door_filename)
        {
          struct stat st;
          
          if (stat(self->door_filename->str, &st) == -1)
            {
              /* file does not exist, create it */
              fd = creat(self->door_filename->str, 0666);
              if (fd == -1)
                {
                  msg_error("Error creating syslog door file",
                            evt_tag_str(EVT_TAG_FILENAME, self->door_filename->str),
                            evt_tag_errno(EVT_TAG_OSERROR, errno),
                            NULL);
                  close(fd);
                  return FALSE;
                }
            }
          fdetach(self->door_filename->str);
          self->door_fd = door_create(afstreams_sd_door_server_proc, NULL, 0);
          if (self->door_fd == -1)
            {
              msg_error("Error creating syslog door",
                        evt_tag_str(EVT_TAG_FILENAME, self->door_filename->str),
                        evt_tag_errno(EVT_TAG_OSERROR, errno),
                        NULL);
              close(self->door_fd);
              return FALSE;
            }
          g_fd_set_cloexec(self->door_fd, TRUE);
          if (fattach(self->door_fd, self->door_filename->str) == -1)
            {
              msg_error("Error attaching syslog door",
                        evt_tag_str(EVT_TAG_FILENAME, self->door_filename->str),
                        evt_tag_errno(EVT_TAG_OSERROR, errno),
                        NULL);
              close(self->door_fd);
              return FALSE;
            }
        }
      if (!log_pipe_init(self->reader, NULL, NULL))
        {
          msg_error("Error initializing log_reader, closing fd",
                    evt_tag_int("fd", fd),
                    NULL);
          log_pipe_unref(self->reader);
          self->reader = NULL;
          close(fd);
          return FALSE;
        }

    }
  else
    {
      msg_error("Error opening syslog device",
                evt_tag_str(EVT_TAG_FILENAME, self->dev_filename->str),
                evt_tag_errno(EVT_TAG_OSERROR, errno),
                NULL);
      return FALSE;
    }
  return TRUE;
}

static gboolean
afstreams_sd_deinit(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist)
{
  AFStreamsSourceDriver *self = (AFStreamsSourceDriver *) s;

  if (self->reader)
    log_pipe_deinit(self->reader, NULL, NULL);
  door_revoke(self->door_fd);
  close(self->door_fd);
  return TRUE;
}

static void
afstreams_sd_free(LogPipe *s)
{
  AFStreamsSourceDriver *self = (AFStreamsSourceDriver *) s;

  if (self->dev_filename)
    g_string_free(self->dev_filename, TRUE);
  if (self->door_filename)
    g_string_free(self->door_filename, TRUE);
  log_pipe_unref(self->reader);

  log_drv_free_instance(&self->super);
  g_free(s);
}

LogDriver *
afstreams_sd_new(gchar *filename)
{
  AFStreamsSourceDriver *self = g_new0(AFStreamsSourceDriver, 1);

  log_drv_init_instance(&self->super);

  self->dev_filename = g_string_new(filename);
  self->super.super.init = afstreams_sd_init;
  self->super.super.deinit = afstreams_sd_deinit;
  self->super.super.free_fn = afstreams_sd_free;
  log_reader_options_defaults(&self->reader_options);
  return &self->super;
}

#else

void 
afstreams_sd_set_sundoor(LogDriver *self, gchar *filename)
{
}

LogDriver *
afstreams_sd_new(gchar *filename)
{
  AFStreamsSourceDriver *self = g_new0(AFStreamsSourceDriver, 1);

  msg_error("STREAMS support not compiled in", NULL);
  return &self->super;
}

#endif
