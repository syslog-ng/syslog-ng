#include "affile_common.h"
#include "state.h"
#include "misc.h"
#include "messages.h"

#define DEFAULT_SD_OPEN_MODE    (FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE)
#define DEFAULT_SD_OPEN_FLAGS   (GENERIC_READ)
#define DEFAULT_DD_REOPEN_MODE  (FILE_SHARE_READ | FILE_SHARE_WRITE)
#define DEFAULT_DD_REOPEN_FLAGS (GENERIC_WRITE)

static const gchar* spurious_paths[] =  {"..\\", "\\..\\", "../", "/..", NULL};

typedef struct _OpenFileProperties
{
  DWORD mode;
  gboolean create_dirs;
  DWORD flags;
  DWORD create_flag;
} OpenFileProperties;

void
file_monitor_set_poll_freq(FileMonitor *monitor, gint follow_freq)
{
}

int affile_open_fd(const gchar *name, OpenFileProperties *props)
{
  int fd = -1;
  HANDLE hFile;

  hFile = CreateFile(name, props->flags, props->mode, NULL, props->create_flag, FILE_ATTRIBUTE_NORMAL, NULL);

  if (hFile == INVALID_HANDLE_VALUE)
    {
       return -1;
    }

  fd = _open_osfhandle((intptr_t)hFile,O_BINARY);

  return fd;
}

static int
affile_open_file(gchar *name, OpenFileProperties *props)
{
  int fd = -1;

  if (affile_is_spurious_path(name, spurious_paths))
    return -1;

  if (props->create_dirs && !create_containing_directory(name, -1, -1, -1))
      return -1;

  fd = affile_open_fd(name, props);

  msg_trace("affile_open_file",
            evt_tag_str("path", name),
            evt_tag_int("fd", fd),
            NULL);

  return fd;
}

static inline void
affile_sd_init_open_file_properties(AFFileSourceDriver *self, OpenFileProperties *props)
{
  props->create_flag = OPEN_EXISTING;
  props->create_dirs = FALSE;
  props->flags = DEFAULT_SD_OPEN_FLAGS;
  props->mode = DEFAULT_SD_OPEN_MODE;
}

void
affile_dd_init_reopen_file_properties(AFFileDestDriver *self, OpenFileProperties *props)
{
  props->create_flag = OPEN_ALWAYS;
  props->create_dirs = !!(self->flags & AFFILE_CREATE_DIRS);
  props->flags = DEFAULT_DD_REOPEN_FLAGS;
  props->mode = DEFAULT_DD_REOPEN_MODE;
}

gboolean
affile_sd_open_file(AFFileSourceDriver *self, gchar *name, gint *fd)
{
  OpenFileProperties props;
  affile_sd_init_open_file_properties(self, &props);
  *fd = affile_open_file(name, &props);

  return (*fd != -1);
}

gboolean
affile_dw_reopen_file(AFFileDestDriver *self, gchar *name, gint *fd)
{
  OpenFileProperties props;
  affile_dd_init_reopen_file_properties(self, &props);
  *fd = affile_open_file(name, &props);

  return (*fd != -1);
}

gboolean
affile_sd_monitor_callback(const gchar *filename, gpointer s, FileActionType action_type)
{
  AFFileSourceDriver *self = (AFFileSourceDriver*) s;
  GlobalConfig *cfg = log_pipe_get_config((LogPipe *)self);

  if (strcmp(self->filename->str, filename) != 0)
    {
      if (action_type == ACTION_DELETED)
        {
          affile_sd_reset_file_state(cfg->state, affile_sd_format_persist_name(filename));
        }
      else
        {
          affile_sd_add_file_to_the_queue(self,filename);
        }
    }
  else if (action_type == ACTION_DELETED && self->reader)
    {
      log_pipe_notify(&self->super.super.super,&self->super.super.super, NC_FILE_MOVED, self->reader);
    }
  else if (action_type == ACTION_CREATED && self->reader == NULL)
    {
      affile_sd_add_file_to_the_queue(self,filename);
    }
  if (self->reader == NULL)
    {
      gboolean end_of_list = TRUE;
      gchar *filename = affile_pop_next_file(s, &end_of_list);

      msg_trace("affile_sd_monitor_callback self->reader is NULL", evt_tag_str("file",filename), NULL);
      if (filename)
        {
          g_string_assign(self->filename, filename);
          g_free(filename);
          return affile_sd_open(s, !end_of_list);
        }
    }
  return TRUE;
}

void
affile_file_monitor_stop(AFFileSourceDriver *self)
{
  file_monitor_stop(self->file_monitor);
}

void affile_file_monitor_init(AFFileSourceDriver *self, const gchar *filename)
{
  self->file_monitor = file_monitor_new();
  self->file_list = g_queue_new();
}
