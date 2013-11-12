/*
 * COPYRIGHTHERE
 */
#include "filemonitor.h"
#include "messages.h"
#include "mainloop.h"
#include "timeutils.h"
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <iv.h>
#include <stdlib.h>

#if ENABLE_MONITOR_INOTIFY
# ifdef HAVE_SYS_INOTIFY_H
#  include <sys/inotify.h>
# elif HAVE_INOTIFYTOOLS_INOTIFY_H
#  include <inotifytools/inotify.h>
# endif
#endif

/*
 Resolve . and ..
 Resolve symlinks
 Resolve tricki symlinks like a -> ../a/../a/./b
*/
gchar*
resolve_to_absolute_path(const gchar *path, const gchar *basedir)
{
  gchar *buffer = g_malloc(PATH_MAX);
  gchar cwd[PATH_MAX];

  getcwd(cwd, PATH_MAX);
  chdir(basedir);

  gchar *result = realpath(path, buffer);
  if (result == NULL)
    {
      g_free(buffer);
      if (errno == ENOENT)
        {
          result = g_strdup(path);
        }
      else
        {
          msg_error("Can't resolve to absolute path", evt_tag_str("path", path), evt_tag_errno("error", errno), NULL);
        }
    }
  chdir(cwd);
  return result;
}

/**************************************************************************/
typedef struct _MonitorBase
{
  gchar *base_dir;
  FileMonitor *file_monitor;
  gboolean (* callback)(FileMonitor *,struct _MonitorBase *);
} MonitorBase;


static gboolean file_monitor_chk_file(FileMonitor * monitor, MonitorBase *source, const gchar *filename);

#if ENABLE_MONITOR_INOTIFY
typedef struct _MonitorInotify
{
  MonitorBase super;
  struct iv_fd fd_watch;
  guint32 watchd;
} MonitorInotify;

static gboolean
file_monitor_process_inotify_event(FileMonitor *monitor, MonitorInotify *self)
{
  struct inotify_event events[32];
  gint byte_read = 0;
  gint i = 0;
  gchar *path = NULL;

  memset(events, 0, sizeof(events) );
  /* Read some events from the kernel buffer */
  byte_read = read(self->fd_watch.fd, &events, sizeof(events));

  if (byte_read == -1)
    {
      msg_error("Failed to read inotify event", evt_tag_errno("read", errno), NULL);
      return FALSE;
    }
  else if (byte_read == 0)
    {
      return TRUE;
    }

  for (i = 0; i < 32; i++)
    {
      if (events[i].wd == self->watchd)
        {
          msg_debug("inotify process event",
                   evt_tag_str("filename", events[i].name),
                   NULL);
          path = resolve_to_absolute_path(events[i].name, self->super.base_dir);
          if (g_file_test(path, G_FILE_TEST_IS_DIR))
            {
              if (monitor->recursion)
                file_monitor_watch_directory(monitor, path);
            }
          else
            {
              /* file or symlink */
              file_monitor_chk_file(monitor, &self->super, events[i].name);
            }
          g_free(path);
        }

      if ((i+1)*sizeof(struct inotify_event) >= byte_read)
        break;
    }
  return TRUE;
}

static void
monitor_inotify_process_input(gpointer s)
{
  MonitorInotify* self = (MonitorInotify *)s;
  file_monitor_process_inotify_event(self->super.file_monitor,self);
}

static void
monitor_inotify_init_watches(MonitorInotify *self)
{

  IV_FD_INIT(&self->fd_watch);
  self->fd_watch.cookie = self;
  self->fd_watch.handler_in = monitor_inotify_process_input;
}

static gboolean
monitor_inotify_init(MonitorInotify *self, const gchar *base_dir)
{
  main_loop_assert_main_thread();
  gint watchd = -1;
  /* Init inotify interface */
  gint ifd = inotify_init();
  monitor_inotify_init_watches(self);
  if (ifd == -1)
    return FALSE;
  /* Set the base directory */
  self->super.base_dir = g_strdup(base_dir);
  self->fd_watch.fd = ifd;
  watchd = inotify_add_watch(self->fd_watch.fd, base_dir, IN_MODIFY | IN_MOVED_TO | IN_CREATE);
  if (watchd == -1)
    return FALSE;
  self->watchd = watchd;
  iv_fd_register(&self->fd_watch);
  return TRUE;
}

static void
monitor_source_inotify_free(gpointer s)
{
  MonitorInotify *self = (MonitorInotify*) s;
  if (iv_fd_registered(&self->fd_watch))
    {
      iv_fd_unregister(&self->fd_watch);
    }
  if (self->fd_watch.fd != -1)
    {
      /* release watchd */
      inotify_rm_watch(self->fd_watch.fd, self->watchd);
      close(self->fd_watch.fd);
      self->fd_watch.fd = -1;
      self->watchd = 0;
    }
  /* Here we have to unregister the ivykis callings */
  if (self->super.base_dir)
    g_free(self->super.base_dir);
}

static MonitorBase *
monitor_inotify_new(FileMonitor *monitor)
{
  MonitorInotify *self = g_new0(MonitorInotify,1);
  self->super.base_dir = NULL;
  self->fd_watch.fd = -1;
  self->super.file_monitor = monitor;
  return &self->super;
}

#endif

void
file_monitor_set_file_callback(FileMonitor *self, FileMonitorCallbackFunc file_callback, gpointer user_data)
{
  self->file_callback = file_callback;
  self->user_data = user_data;
}

void
file_monitor_set_destroy_callback(FileMonitor *self, GSourceFunc destroy_callback, gpointer user_data)
{
  self->destroy_callback = destroy_callback;
  self->user_data = user_data;
}

void
file_monitor_set_poll_freq(FileMonitor *self, gint poll_freq)
{
  self->poll_freq = poll_freq;
}

/**
 * file_monitor_chk_file:
 *
 * This function checks if the given filename matches the filters.
 **/
static gboolean
file_monitor_chk_file(FileMonitor * monitor, MonitorBase *source, const gchar *filename)
{
  gboolean ret = FALSE;
  gchar *path = g_build_filename(source->base_dir, filename, NULL);

  if (g_file_test(path, G_FILE_TEST_EXISTS) &&
      g_pattern_match_string(monitor->compiled_pattern, filename) &&
      monitor->file_callback != NULL)
    {
      /* FIXME: resolve symlink */
      /* callback to affile */
      msg_debug("file_monitor_chk_file filter passed", evt_tag_str("file",path),NULL);
      monitor->file_callback(path, monitor->user_data, ACTION_NONE);
      ret = TRUE;
    }
  g_free(path);
  return ret;
}

/**
 *
 * This function performs the initial iteration of a monitored directory.
 * Once this finishes we closely watch all events that change the directory
 * contents.
 **/
static gboolean
file_monitor_list_directory(FileMonitor *self, MonitorBase *source, const gchar *basedir)
{
  GDir *dir = NULL;
  GError *error = NULL;
  const gchar *file_name = NULL;
  /* try to open diretory */
  dir = g_dir_open(basedir, 0, &error);
  if (dir == NULL)
    {
      g_clear_error(&error);
      return FALSE;
    }

  while ((file_name = g_dir_read_name(dir)) != NULL)
    {
      gchar * path = resolve_to_absolute_path(file_name, basedir);
      if (g_file_test(path, G_FILE_TEST_IS_DIR))
        {
          /* Recursion is enabled */
          if (self->recursion)
            file_monitor_watch_directory(self, path); /* construct a new source to monitor the directory */
        }
      else
        {
          /* if file or symlink, match with the filter pattern */
          file_monitor_chk_file(self, source, file_name);
        }
      g_free(path);
    }
  g_dir_close(dir);
  if (self->file_callback != NULL)
    self->file_callback(END_OF_LIST, self->user_data, ACTION_NONE);
  return TRUE;
}


/**
 * file_monitor_is_monitored:
 *
 * Check if the directory specified in filename is already in the monitored
 * list.
 **/
gboolean
file_monitor_is_dir_monitored(FileMonitor *self, const gchar *filename)
{
  GSList *source;

  source = self->sources;
  while (source != NULL)
    {
      const gchar *chk_dir = ((MonitorBase*) source->data)->base_dir;
      if (strcmp(filename, chk_dir) == 0)
        return TRUE;
      else
        source = g_slist_next(source);
    }
  return FALSE;
}

#if ENABLE_MONITOR_INOTIFY

MonitorBase *
file_monitor_create_inotify(FileMonitor *self, const gchar *base_dir)
{
  MonitorBase *source = monitor_inotify_new(self);

  if (!monitor_inotify_init((MonitorInotify*) source, base_dir))
    {
      monitor_source_inotify_free(source);
      return NULL;
    }
  return source;
}

void
file_monitor_start_inotify(FileMonitor *self, MonitorBase *source, const gchar *base_dir)
{
  file_monitor_list_directory(self, source, base_dir);
  //g_source_set_callback(&source->super, (GSourceFunc) file_monitor_process_inotify_event, self, NULL);
}

void
file_monitor_inotify_destroy(gpointer source,gpointer monitor)
{
  MonitorInotify *self = (MonitorInotify *)source;
  if (iv_fd_registered(&self->fd_watch))
    {
      iv_fd_unregister(&self->fd_watch);
    }
  if (self->fd_watch.fd != -1)
    {
      /* release watchd */
      inotify_rm_watch(self->fd_watch.fd, self->watchd);
      close(self->fd_watch.fd);
      self->fd_watch.fd = -1;
      self->watchd = 0;
    }
  if (self->super.base_dir)
    g_free(self->super.base_dir);
}
#endif

typedef struct _MonitorPoll
{
  MonitorBase super;
  GDir *dir;
  gint poll_freq;
  struct iv_timer poll_timer;
} MonitorPoll;

static void file_monitor_poll_timer_callback(gpointer s);

void
file_monitor_poll_destroy(gpointer source,gpointer monitor)
{
  MonitorPoll *self = (MonitorPoll *)source;
  if(iv_timer_registered(&self->poll_timer))
    {
      iv_timer_unregister(&self->poll_timer);
    }
  if (self->super.base_dir)
    g_free(self->super.base_dir);
}

MonitorPoll *
monitor_source_poll_new(guint32 poll_freq, FileMonitor *monitor)
{
  MonitorPoll *self = g_new0(MonitorPoll,1);
  self->super.base_dir = NULL;
  self->dir = NULL;
  self->poll_freq = poll_freq;
  IV_TIMER_INIT(&self->poll_timer);
  self->poll_timer.cookie = self;
  self->poll_timer.handler = file_monitor_poll_timer_callback;

  self->super.file_monitor = monitor;
  return self;
}

static gboolean
file_monitor_process_poll_event(FileMonitor *monitor, MonitorPoll *source)
{
  GError *err = NULL;
  const gchar * file_name = NULL;
  gchar *path = NULL;

  if (source->dir == NULL)
    {
      source->dir = g_dir_open(source->super.base_dir, 0, &err);
      if (source->dir == NULL)
        {
          msg_error("failed to open directory",
                    evt_tag_str("basedir", source->super.base_dir),
                    evt_tag_str("message", err->message),
                    NULL);
          g_error_free(err);
          return FALSE;
        }
    }

  file_name = g_dir_read_name(source->dir);

  if (file_name == NULL)
    {
      /* The next poll event will reopen the directory and check the files again */
      g_dir_close(source->dir);
      if (monitor->file_callback != NULL)
          monitor->file_callback(END_OF_LIST, monitor->user_data, ACTION_NONE);
      source->dir = NULL;
      return TRUE;
    }

  msg_debug("poll process event",
            evt_tag_str("basedir", source->super.base_dir),
            evt_tag_str("filename", file_name),
            NULL);

  path = resolve_to_absolute_path(file_name, source->super.base_dir);
  if (g_file_test(path, G_FILE_TEST_IS_DIR))
    {
      if (monitor->recursion)
        file_monitor_watch_directory(monitor, path);
    }
  else
    {
      /* file or symlink */
      file_monitor_chk_file(monitor, &source->super, file_name);
    }
  g_free(path);
  return TRUE;
}

static void
file_monitor_poll_timer_callback(gpointer s)
{
  MonitorPoll *self = (MonitorPoll *)s;
  self->poll_freq = self->super.file_monitor->poll_freq;
  file_monitor_process_poll_event(self->super.file_monitor, self);
  if(iv_timer_registered(&self->poll_timer))
    iv_timer_unregister(&self->poll_timer);
  iv_validate_now();
  self->poll_timer.expires = iv_now;
  timespec_add_msec(&self->poll_timer.expires, self->poll_freq);
  iv_timer_register(&self->poll_timer);
}

static gboolean
file_monitor_poll_init(MonitorPoll *self, const gchar *base_dir)
{
  self->super.base_dir = g_strdup(base_dir);
  return TRUE;
}

static void
monitor_poll_free(MonitorPoll *self)
{
  if (self->dir)
    g_dir_close(self->dir);
  if (self->super.base_dir)
    g_free(self->super.base_dir);
  if (iv_timer_registered(&self->poll_timer))
    iv_timer_unregister(&self->poll_timer);
}

MonitorBase *
file_monitor_create_poll(FileMonitor *self, const gchar *base_dir)
{
  MonitorPoll *source = monitor_source_poll_new(self->poll_freq, self);
  if(!file_monitor_poll_init(source,base_dir))
    {
      monitor_poll_free(source);
      return NULL;
    }
  return &source->super;
}

void
file_monitor_start_poll(FileMonitor *self, MonitorBase *source, const gchar *base_dir)
{
  MonitorPoll *p_source = (MonitorPoll *)source;
  file_monitor_list_directory(self, source, base_dir);
  if(iv_timer_registered(&p_source->poll_timer))
    iv_timer_unregister(&p_source->poll_timer);
  iv_validate_now();
  p_source->poll_timer.expires = iv_now;
  timespec_add_msec(&p_source->poll_timer.expires, p_source->poll_freq);
  iv_timer_register(&p_source->poll_timer);
}

void
file_monitor_start(FileMonitor *self, MonitorBase *source, const gchar *base_dir)
{
  switch(self->monitor_type)
    {
#if ENABLE_MONITOR_INOTIFY
      case MONITOR_INOTIFY:
         file_monitor_start_inotify(self, source, base_dir);
         break;
#endif
       case MONITOR_POLL:
         file_monitor_start_poll(self, source, base_dir);
         break;
       default:
         g_assert_not_reached();
    }
}

gboolean
file_monitor_watch_directory(FileMonitor *self, const gchar *filename)
{
  MonitorBase *source = NULL;
  gchar *base_dir;
  g_assert(self);
  g_assert(filename);

  if (!g_file_test(filename, G_FILE_TEST_IS_DIR))
    {
      /* if the filename is not a directory then remove the file part and try only the directory part */
      gchar *dir_part = g_path_get_dirname (filename);

      if (g_path_is_absolute (dir_part))
        {
          base_dir = resolve_to_absolute_path(dir_part, G_DIR_SEPARATOR_S);
        }
      else
        {
          gchar *wd = g_get_current_dir();
          base_dir = resolve_to_absolute_path(dir_part, wd);
          g_free(wd);
        }

      g_free(dir_part);

      if (!self->compiled_pattern)
        {
          gchar *pattern = g_path_get_basename(filename);
          self->compiled_pattern = g_pattern_spec_new(pattern);
          g_free(pattern);
        }
    }
  else
    {
       base_dir = g_strdup(filename);
    }

  if (base_dir == NULL || !g_path_is_absolute (base_dir))
    {
      msg_error("Can't monitor directory, because it can't be resolved as absolute path", evt_tag_str("base_dir", base_dir), NULL);
      return FALSE;
    }

  if (file_monitor_is_dir_monitored(self, base_dir))
    {
      /* We are monitoring this directory already, so ignore it */
      g_free(base_dir);
      return TRUE;
    }
  else
    {
      msg_debug("Monitoring new directory", evt_tag_str("basedir", base_dir), NULL);
    }
  switch(self->monitor_type)
    {
      /* Setup inotify, if supported */
#if ENABLE_MONITOR_INOTIFY
      case MONITOR_INOTIFY:
        source = file_monitor_create_inotify(self, base_dir);
        break;
#endif
      case MONITOR_POLL:
        source = file_monitor_create_poll(self, base_dir);
        break;
      default:
#if ENABLE_MONITOR_INOTIFY
        self->monitor_type = MONITOR_INOTIFY;
        source = file_monitor_create_inotify(self, base_dir);
#endif
        if (!source)
          {
            self->monitor_type = MONITOR_POLL;
            source = file_monitor_create_poll(self, base_dir);
            if (!source)
              {
                self->monitor_type = MONITOR_NONE;
              }
          }
    }

  if (source == NULL)
    {
      msg_error("Failed to initialize file monitoring", evt_tag_str("basedir", base_dir), NULL);
      g_free(base_dir);
      return FALSE;
    }

  self->sources = g_slist_append(self->sources, source);
  file_monitor_start(self, source, base_dir);
  g_free(base_dir);
  return TRUE;
}


/**
 * file_monitor_new:
 *
 * This function constructs a new FileMonitor object.
 **/
FileMonitor *
file_monitor_new(void)
{
  FileMonitor *self = g_new0(FileMonitor, 1);

  self->sources = NULL;
  self->file_callback = NULL;
  self->recursion = FALSE;
  self->monitor_type = MONITOR_NONE;
  self->poll_freq = 1000; /* 1 sec is the default*/
  return self;
}


void
file_monitor_deinit(FileMonitor *self)
{
  if (self->sources) {
    if (self->monitor_type == MONITOR_POLL)
      {
        g_slist_foreach(self->sources, file_monitor_poll_destroy, NULL);
      }
    else
      {
#if ENABLE_MONITOR_INOTIFY
        g_slist_foreach(self->sources, file_monitor_inotify_destroy, NULL);
#else
        abort();
#endif
      }
  }
}

void
file_monitor_free(FileMonitor *self)
{
  if (self->compiled_pattern)
    {
      g_pattern_spec_free(self->compiled_pattern);
      self->compiled_pattern = NULL;
    }
  if (self->sources)
    {
      GSList *source_list = self->sources;
      while(source_list)
        {
          g_free(source_list->data);
          source_list = source_list->next;
        }
      g_slist_free(self->sources);
      self->sources = NULL;
    }
  g_free(self);
}
