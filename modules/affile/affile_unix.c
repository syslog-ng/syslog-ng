#include "affile.h"
#include "affile_common.h"
#include "driver.h"
#include "messages.h"
#include "misc.h"
#include "serialize.h"
#include "gprocess.h"
#include "stats.h"
#include "mainloop.h"
#include "filemonitor.h"
#include "versioning.h"
#include "state.h"
#include "cfg.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>
#include <pcre.h>

static const gchar* spurious_paths[] = {"../", "/..", NULL};

#define DEFAULT_SD_OPEN_FLAGS (O_RDONLY | O_NOCTTY | O_NONBLOCK | O_LARGEFILE)
#define DEFAULT_SD_OPEN_FLAGS_PIPE (O_RDWR | O_NOCTTY | O_NONBLOCK | O_LARGEFILE)
#define DEFAULT_DW_REOPEN_FLAGS (O_WRONLY | O_CREAT | O_NOCTTY | O_NONBLOCK | O_LARGEFILE)
#define DEFAULT_DW_REOPEN_FLAGS_PIPE (O_RDWR | O_NOCTTY | O_NONBLOCK | O_LARGEFILE)

typedef struct _Permission
{
  gint uid;
  gint gid;
  gint mode;
} Permission;

typedef struct _OpenFileProperties
{
  Permission file_access;
  Permission dir_access;
  gboolean create_dirs;
  gboolean privileged;
  gboolean is_pipe;
  gint flags;
} OpenFileProperties;

static inline gboolean
affile_set_caps(gchar *name, OpenFileProperties *props, cap_t *act_caps)
{
  *act_caps = g_process_cap_save();

  if (props->privileged)
    {
      g_process_cap_modify(CAP_DAC_READ_SEARCH, TRUE);
      g_process_cap_modify(CAP_SYSLOG, TRUE);
    }
  else
    {
      g_process_cap_modify(CAP_DAC_OVERRIDE, TRUE);
    }

  if (props->create_dirs &&
     !create_containing_directory(name,
                                  props->dir_access.uid,
                                  props->dir_access.gid,
                                  props->dir_access.mode))
    {
      g_process_cap_restore(*act_caps);
      return FALSE;
    }

  return TRUE;
}

static inline void
affile_set_fd_permission(OpenFileProperties *props, int fd)
{
  if (fd != -1)
    {
      g_fd_set_cloexec(fd, TRUE);

      g_process_cap_modify(CAP_CHOWN, TRUE);
      g_process_cap_modify(CAP_FOWNER, TRUE);
      set_permissions_fd(fd,
                         props->file_access.uid,
                         props->file_access.gid,
                         props->file_access.mode);
    }
}

int affile_open_fd(const gchar *name, OpenFileProperties *props)
{
  int fd;
  fd = open(name, props->flags, props->file_access.mode < 0 ? 0600 : props->file_access.mode);
  if (props->is_pipe && fd < 0 && errno == ENOENT)
    {
      if (mkfifo(name, 0666) >= 0)
        fd = open(name, props->flags, 0666);
    }
  return fd;
}

static void
affile_check_file_type(const gchar *name, OpenFileProperties *props)
{
  struct stat st;
  if (stat(name, &st) >= 0)
    {
      if (props->is_pipe && !S_ISFIFO(st.st_mode))
        {
          msg_warning("WARNING: you are using the pipe driver, underlying file is not a FIFO, it should be used by file()",
                    evt_tag_str("filename", name),
                    NULL);
        }
      else if (!props->is_pipe && S_ISFIFO(st.st_mode))
        {
          msg_warning("WARNING: you are using the file driver, underlying file is a FIFO, it should be used by pipe()",
                      evt_tag_str("filename", name),
                      NULL);
        }
    }
}

static int
affile_open_file(gchar *name, OpenFileProperties *props)
{
  int fd = -1;
  cap_t saved_caps;

  if (affile_is_spurious_path(name, spurious_paths))
    return -1;

  if (!affile_set_caps(name, props, &saved_caps))
    return -1;

  affile_check_file_type(name, props);

  fd = affile_open_fd(name, props);

  affile_set_fd_permission(props, fd);

  g_process_cap_restore(saved_caps);

  msg_trace("affile_open_file",
            evt_tag_str("path", name),
            evt_tag_int("fd",fd),
            NULL);

  return fd;
}

static inline void
affile_sd_init_open_file_properties(AFFileSourceDriver *self, OpenFileProperties *props)
{
  props->is_pipe = !!(self->flags & AFFILE_PIPE);
  props->privileged = !!(self->flags & AFFILE_PRIVILEGED);
  props->create_dirs = FALSE;

  props->file_access.uid  = -1;
  props->file_access.gid  = -1;
  props->file_access.mode = -1;

  props->dir_access.uid  = 0;
  props->dir_access.gid  = 0;
  props->dir_access.mode = 0;

  if (props->is_pipe)
  {
    props->flags = DEFAULT_SD_OPEN_FLAGS_PIPE;
  }
  else
  {
    props->flags = DEFAULT_SD_OPEN_FLAGS;
  }
}

void
affile_dd_init_reopen_file_properties(AFFileDestDriver *self, OpenFileProperties *props)
{
  props->is_pipe = !!(self->flags & AFFILE_PIPE);
  props->create_dirs = !!(self->flags & AFFILE_CREATE_DIRS);
  props->privileged = FALSE;

  props->file_access.uid = self->file_uid;
  props->file_access.gid = self->file_gid;
  props->file_access.mode = self->file_perm;

  props->dir_access.uid = self->dir_uid;
  props->dir_access.gid = self->dir_gid;
  props->dir_access.mode = self->dir_perm;

  if (props->is_pipe) {
    props->flags = DEFAULT_DW_REOPEN_FLAGS_PIPE;
  }
  else
  {
    props->flags = DEFAULT_DW_REOPEN_FLAGS;
  }
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

  if (strcmp(self->filename->str, filename) != 0)
    {
      affile_sd_add_file_to_the_queue(self, filename);
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


inline void
affile_file_monitor_stop(AFFileSourceDriver *self)
{
/*nop*/
}

void
affile_file_monitor_init(AFFileSourceDriver *self, const gchar *filename)
{
  if (is_wildcard_filename(filename))
    {
      self->file_monitor = file_monitor_new();
      self->file_list = g_queue_new();
    }
  else
    {
      self->file_monitor = NULL;
      self->file_list = NULL;
    }
}
