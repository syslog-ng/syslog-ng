
#include "afinter.h"
#include "logreader.h"

#include "messages.h"

typedef struct _AFInterSourceDriver
{
  LogDriver super;
  LogPipe *reader;
  LogReaderOptions reader_options;
} AFInterSourceDriver;

static gboolean
afinter_sd_init(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist)
{
  AFInterSourceDriver *self = (AFInterSourceDriver *) s;

  log_reader_options_init(&self->reader_options, cfg);
  if (msg_pipe[0] != -1)
    {
      self->reader = log_reader_new(fd_read_new(msg_pipe[0], FR_DONTCLOSE), LR_INTERNAL, s, &self->reader_options);
      log_pipe_append(self->reader, s);
      log_pipe_init(self->reader, NULL, NULL);
    }
  return TRUE;
}

static gboolean
afinter_sd_deinit(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist)
{
  AFInterSourceDriver *self = (AFInterSourceDriver *) s;
  
  if (self->reader)
    log_pipe_deinit(self->reader, NULL, NULL);
  return TRUE;
}

static void
afinter_sd_free(LogPipe *s)
{
  AFInterSourceDriver *self = (AFInterSourceDriver *) s;
  log_drv_free_instance(&self->super);
  log_pipe_unref(self->reader);
}

LogDriver *
afinter_sd_new(void)
{
  AFInterSourceDriver *self = g_new0(AFInterSourceDriver, 1);

  log_drv_init_instance(&self->super);
  self->super.super.init = afinter_sd_init;
  self->super.super.deinit = afinter_sd_deinit;
  self->super.super.free_fn = afinter_sd_free;
  log_reader_options_defaults(&self->reader_options);
  return &self->super;
}

