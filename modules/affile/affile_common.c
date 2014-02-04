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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>
#include <pcre.h>
#if __FreeBSD__
#include <sys/utsname.h>
#endif

static gchar *pid_string = NULL;

static gboolean
is_string_contain_any_fragment(const gchar *str, const gchar *fragments[])
{
  int i;

  for (i=0; fragments[i]; i++)
    if (strstr(str, fragments[i]))
      return TRUE;

  return FALSE;
}

#if __FreeBSD__
static gboolean
check_os_version()
{
  struct utsname u;
  if (uname(&u) != 0)
    {
      msg_error("file(): Cannot get information about the running kernel",
                evt_tag_errno("error", errno),
                NULL);
      return FALSE;
    }

  if (strcmp(u.sysname, "FreeBSD") != 0)
    return FALSE;

  if (strncmp(u.release, "6.", 2) == 0 ||
      strncmp(u.release, "7.", 2) == 0 ||
      strncmp(u.release, "8.", 2) == 0 ||
      strncmp(u.release, "9.0", 3) == 0)
    return TRUE;

  return FALSE;
}

#endif

static inline void
affile_handle_zero_follow_freq(AFFileSourceDriver *self)
{
#if __FreeBSD__
  GlobalConfig *cfg = log_pipe_get_config((LogPipe *)self);
  if ((strcmp(self->filename_pattern->str, "/dev/klog") == 0) &&
      (cfg->version <= 0x0500) &&
      (check_os_version()))
    {
      msg_warning("Warning: syslog-ng cannot poll /dev/klog, therefore the follow_freq option has been set to 1. To avoid this warning, set follow_freq higher than 0",
                  evt_tag_str("source", self->super.super.group),
                  NULL);
      self->reader_options.follow_freq = 1000;
    }
#endif
}

inline gchar *
affile_sd_format_persist_name(const gchar *filename)
{
  static gchar persist_name[1024];

  g_snprintf(persist_name, sizeof(persist_name), "%s(%s)", FILE_CURPOS_PREFIX, filename);
  return persist_name;
}


static void
affile_sd_regex_free(pcre *regex)
{
  if (regex)
    {
      pcre_free(regex);
    }
}

static void
affile_sd_destroy_reader(gpointer user_data)
{
  LogPipe *reader = (LogPipe *) user_data;

  log_pipe_unref(reader);
}

static void
affile_sd_set_file_pos(AFFileSourceDriver *self, GlobalConfig *cfg)
{
  STATE_HANDLER_CONSTRUCTOR state_handler_constructor;
  StateHandler *state_handler;
  NameValueContainer *container;
  gpointer *state_entry;
  struct stat st;

  if (stat(self->filename->str, &st) == -1)
    {
      return;
    }

  state_handler_constructor = state_handler_get_constructor_by_prefix(FILE_CURPOS_PREFIX);
  if (!state_handler_constructor)
    {
      msg_error("Can't get persist_state_constructor for prefix",
                evt_tag_str("prefix", FILE_CURPOS_PREFIX),
                NULL);
      return;
    }
  state_handler = state_handler_constructor(cfg->state, affile_sd_format_persist_name(self->filename->str));
  if (!state_handler)
    {
      msg_error("Can't get state handler for persist name",
                evt_tag_str("persist_name",
                affile_sd_format_persist_name(self->filename->str)),
                NULL);
      return;
    }
  if (state_handler_entry_exists(state_handler))
    {
      msg_debug("State already exists",
                evt_tag_str("state_name", state_handler->name),
                NULL);
      state_handler_free(state_handler);
      return;
    }
  if (state_handler_alloc_state(state_handler) == 0)
    {
      msg_error("Can't allocate state for file",
                evt_tag_str("state_name", state_handler->name),
                NULL);
      state_handler_free(state_handler);
      return;
    }
  state_entry = state_handler_get_state(state_handler);
  memset(state_entry, 0, state_handler->size);
  container = state_handler_dump_state(state_handler);

  name_value_container_add_int(container, "version", 1);
  name_value_container_add_boolean(container, "big_endian", (G_BYTE_ORDER == G_BIG_ENDIAN));
  name_value_container_add_int64(container, "file_size", st.st_size);
  name_value_container_add_int64(container, "file_inode", st.st_ino);
  name_value_container_add_int64(container, "raw_stream_pos", st.st_size);

  state_handler_load_state(state_handler, container, NULL);

  state_handler_free(state_handler);
  name_value_container_free(container);
  return;
}

static gboolean
affile_sd_collect_files(const gchar *filename, gpointer s, FileActionType action_type)
{
  gpointer *args = (gpointer *)s;
  AFFileSourceDriver *self = (AFFileSourceDriver*)args[0];
  GlobalConfig *cfg = (GlobalConfig *)args[1];

  if (filename != END_OF_LIST)
    {
      g_string_assign(self->filename, filename);
      affile_sd_set_file_pos(self, cfg);
    }
  else
    {
      g_string_truncate(self->filename, 0);
    }
  return TRUE;
}

inline gboolean
is_wildcard_filename(const gchar *filename)
{
  return strchr(filename, '*') != NULL || strchr(filename, '?') != NULL;
}

gboolean
affile_is_spurious_path(const gchar *name, const gchar *spurious_paths[])
{
  if (is_string_contain_any_fragment(name, spurious_paths))
    {
      msg_error("Spurious path, logfile not created",
                evt_tag_str("path", name),
                NULL);
      return TRUE;
    }

  return FALSE;
}

gchar*
affile_pop_next_file(LogPipe *s, gboolean *last_item)
{
  gchar *ret = NULL;
  AFFileSourceDriver *self = (AFFileSourceDriver *) s;

  *last_item = FALSE;

  if (g_queue_is_empty(self->file_list))
    return ret;

  ret = g_queue_pop_head(self->file_list);

  if (ret == END_OF_LIST)
    {
      /*skip the *END* item and try the next file in the list*/
      ret = g_queue_pop_head(self->file_list);
      if (ret)
        *last_item = TRUE;
    }
  else if (self->file_list->length == 0)
    {
      *last_item = TRUE;
    }
  if (!ret || ret == END_OF_LIST)
    ret = NULL;

  return ret;
}

void
affile_sd_queue(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options, gpointer user_data)
{
  AFFileSourceDriver *self = (AFFileSourceDriver *) s;
  static NVHandle filename_handle = 0;
  static NVHandle filesize_handle = 0;
  static NVHandle filepos_handle = 0;
  static NVHandle sdata_filepos = 0;
  static NVHandle sdata_filesize = 0;

  log_src_driver_counter_inc(s);

  if (!filename_handle)
    {
      filename_handle = log_msg_get_value_handle("FILE_NAME");
    }

  if (!filesize_handle)
    {
      filesize_handle = log_msg_get_value_handle("FILE_SIZE");
      sdata_filesize = log_msg_get_value_handle(SDATA_FILE_SIZE);
    }

  if (!filepos_handle)
    {
      filepos_handle = log_msg_get_value_handle("FILE_CURRENT_POSITION");
      sdata_filepos = log_msg_get_value_handle(SDATA_FILE_POS);
    }

  if (filename_handle)
    {
      log_msg_set_value(msg, filename_handle, self->filename->str, self->filename->len);
    }

  if (self->is_regular)
    {
      if (filesize_handle && sdata_filesize)
        {
          log_msg_set_value_indirect(msg, filesize_handle, sdata_filesize, 0, 0, -1);
        }

      if (filepos_handle && sdata_filepos)
        {
          log_msg_set_value_indirect(msg, filepos_handle, sdata_filepos, 0, 0, -1);
        }
    }
  log_pipe_forward_msg(s, msg, path_options);
}

void
affile_sd_add_file_to_the_queue(AFFileSourceDriver *self, const gchar *filename)
{
  /* FIXME: use something else than a linear search */
  if (g_queue_find_custom(self->file_list, filename, (GCompareFunc)strcmp) == NULL)
    {
      affile_sd_monitor_pushback_filename(self, filename);
      if (filename != END_OF_LIST)
        {
          msg_debug("affile_sd_monitor_callback append", evt_tag_str("file",filename),NULL);
        }
    }
}

void
affile_sd_recover_state(LogPipe *s, GlobalConfig *cfg, LogProto *proto)
{
  AFFileSourceDriver *self = (AFFileSourceDriver *) s;

  if ((self->flags & AFFILE_PIPE) || self->reader_options.follow_freq <= 0)
    return;

  if (self->reader_options.text_encoding)
    log_proto_set_encoding(proto, self->reader_options.text_encoding);

  if (!log_proto_restart_with_state(proto, cfg->state, affile_sd_format_persist_name(self->filename->str)))
    {
      msg_error("Error converting persistent state from on-disk format, losing file position information",
                evt_tag_str("filename", self->filename->str),
                evt_tag_id(MSG_CANT_RECOVER_FILE_STATE),
                NULL);
      return;
    }
}

void
affile_sd_reset_file_state(PersistState *state, const gchar *old_persist_name)
{
  gchar *new_persist_name = g_strdup_printf("%s_DELETED",old_persist_name);
  persist_state_rename_entry(state,old_persist_name,new_persist_name);
  g_free(new_persist_name);
}

/* NOTE: runs in the main thread */
void
affile_sd_notify(LogPipe *s, LogPipe *sender, gint notify_code, gpointer user_data)
{
  AFFileSourceDriver *self = (AFFileSourceDriver *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);
  LogProtoServerOptions *options = (LogProtoServerOptions *)&self->proto_options;
  gchar *filename = NULL;
  gint fd;

  switch (notify_code)
    {
    case NC_FILE_MOVED:
      {
        msg_verbose("Follow-mode file source moved, tracking of the new file is started",
                    evt_tag_str("filename", self->filename->str),
                    NULL);
        if (affile_sd_open_file(self, self->filename->str, &fd))
          {
            LogTransport *transport;
            LogProto *proto;

            transport = log_transport_plain_new(fd, 0);
            transport->timeout = 10;

            proto = affile_sd_construct_proto(self, transport);

            affile_sd_recover_state(s, cfg, proto);

            log_reader_reopen(self->reader, proto, s, &self->reader_options, 1, SCS_FILE, self->super.super.id, self->filename->str, TRUE, (LogProtoOptions *)options);
          }
        else
          {
            log_pipe_deinit(self->reader);
            log_pipe_unref(self->reader);
            self->reader = NULL;
            affile_sd_reset_file_state(cfg->state, affile_sd_format_persist_name(self->filename->str));
          }
        break;
      }
     case NC_FILE_SKIP:
      {
        if (!self->file_monitor)
          {
            log_reader_set_immediate_check(self->reader);
            log_reader_restart(self->reader);
            break;
          }
        msg_debug("File delayed", evt_tag_str("filename", self->filename->str), NULL);
        affile_sd_monitor_pushback_filename(self, self->filename->str);
      }
    case NC_CLOSE:
    case NC_FILE_EOF:
      {
        gint fd = -1;
        gboolean end_of_list = TRUE;
        gboolean immediate_check = FALSE;
get_next_file:
        if (!self->file_monitor)
          {
            break;
          }

        filename = affile_pop_next_file(s, &end_of_list);

        if (end_of_list && notify_code == NC_FILE_SKIP)
          affile_sd_monitor_pushback_filename(self, END_OF_LIST);

        if (!filename)
          {
            if (notify_code == NC_FILE_SKIP)
              log_reader_restart(self->reader);
            break;
          }
        if (affile_sd_open_file(self, filename, &fd))
          {
            LogTransport *transport;
            LogProto *proto;


            g_string_assign(self->filename, filename);
            g_free(filename);
            filename = NULL;

            msg_debug("Monitoring new file",
                      evt_tag_str("filename", self->filename->str),
                      NULL);

            transport = log_transport_plain_new(fd, 0);
            transport->timeout = 10;

            proto = affile_sd_construct_proto(self, transport);

            if (!end_of_list)
              {
                immediate_check = TRUE;
              }

            if (notify_code == NC_FILE_SKIP)
              {
                immediate_check = TRUE;
              }

            affile_sd_recover_state(s, cfg, proto);
            log_reader_reopen(self->reader, proto, s, &self->reader_options, 1, SCS_FILE, self->super.super.id, self->filename->str, immediate_check, (LogProtoOptions *)options);
          }
         else
          {
           if (!end_of_list)
           {
             /* Can't open file it means, that this file is deleted continue to read file till eof and finally release it */
              goto get_next_file;
           }
           else
           {
             log_reader_restart(self->reader);
           }
          }
        break;
      }
    default:
      break;
    }
}

void
affile_sd_monitor_pushback_filename(AFFileSourceDriver *self, const gchar *filename)
{
  /* NOTE: the list contains an allocated copy of the filename, it needs to
   * be freed when removed from the list.
   */
  if (filename != END_OF_LIST)
    msg_trace("affile_sd_monitor_pushback_filename",
              evt_tag_str("filename", filename),
              NULL);

  if (filename == END_OF_LIST)
    g_queue_push_tail(self->file_list, END_OF_LIST);
  else
    g_queue_push_tail(self->file_list, strdup(filename));
}

gboolean
affile_sd_skip_old_messages(LogSrcDriver *s, GlobalConfig *cfg)
{
  AFFileSourceDriver *self = (AFFileSourceDriver *)s;
  if (!generate_persist_file && self->reader_options.super.read_old_records)
    {
      return TRUE;
    }
  if (self->file_monitor)
    {
      gpointer args[] = {self, cfg};
      file_monitor_set_file_callback(self->file_monitor, affile_sd_collect_files, args);
      if (!file_monitor_watch_directory(self->file_monitor, self->filename_pattern->str))
        {
          return FALSE;
        }
      affile_file_monitor_stop(self);
    }
  else
    {
      g_string_assign(self->filename, self->filename_pattern->str);
      affile_sd_set_file_pos(self, cfg);
    }
  return TRUE;
}

LogProto *
affile_sd_construct_proto(AFFileSourceDriver *self, LogTransport *transport)
{
  guint flags;
  LogProto *proto = NULL;
  GlobalConfig *cfg = log_pipe_get_config((LogPipe *)self);
  gchar *name = NULL;

  MsgFormatHandler *handler;

  flags =
    ((self->reader_options.follow_freq > 0)
     ? LPBS_IGNORE_EOF | LPBS_POS_TRACKING
     : LPBS_NOMREAD);

  handler = self->reader_options.parse_options.format_handler;
  self->proto_options.super.flags = flags;
  if ((handler && handler->construct_proto))
    proto = self->reader_options.parse_options.format_handler->construct_proto(log_pipe_get_config((LogPipe *)self),&self->reader_options.parse_options, transport, flags);
  else if (!self->proto_factory)
    {
      if (self->reader_options.padding)
        {
          self->proto_options.super.size = self->reader_options.padding;
          name = "record";
        }
      else
        {
          self->proto_options.super.size = self->reader_options.msg_size;
          name = "file-reader";
        }
      self->proto_factory = log_proto_get_factory(cfg,LPT_SERVER,name);
    }
  if (self->proto_factory)
    {
          proto = self->proto_factory->create(transport, &self->proto_options, log_pipe_get_config((LogPipe *)self));
    }
  else if (proto == NULL)
    {
      msg_error("Can't find proto plugin", evt_tag_str("proto_name",name), evt_tag_id(MSG_CANT_FIND_PROTO), NULL);
    }
  return proto;
}

LogDriver *
affile_sd_new(gchar *filename, guint32 flags)
{
  AFFileSourceDriver *self = g_new0(AFFileSourceDriver, 1);

  log_src_driver_init_instance(&self->super);
  self->filename_pattern = g_string_new(filename);
  self->filename = g_string_sized_new(512);
  self->flags = flags;
  self->super.super.super.init = affile_sd_init;
  self->super.super.super.queue = affile_sd_queue;
  self->super.super.super.deinit = affile_sd_deinit;
  self->super.super.super.notify = affile_sd_notify;
  self->super.super.super.free_fn = affile_sd_free;
  self->super.skip_old_messages = affile_sd_skip_old_messages;
  log_reader_options_defaults(&self->reader_options);
  self->reader_options.parse_options.flags |= LP_LOCAL;

  affile_file_monitor_init(self, filename);
  if ( pid_string == NULL )
    pid_string = get_pid_string();

  if ((self->flags & AFFILE_PIPE))
    {
      static gboolean warned = FALSE;

      if (configuration && get_version_value(configuration->version) < 0x0302)
        {
          if (!warned)
            {
              msg_warning("WARNING: the expected message format is being changed for pipe() to improve "
                          "syslogd compatibity with syslog-ng " VERSION_3_2 ". If you are using custom "
                          "applications which bypass the syslog() API, you might "
                          "need the 'expect-hostname' flag to get the old behaviour back", NULL);
              warned = TRUE;
            }
        }
      else
        {
          self->reader_options.parse_options.flags &= ~LP_EXPECT_HOSTNAME;
        }
    }

  if (configuration && get_version_value(configuration->version) < 0x0300)
    {
      static gboolean warned = FALSE;

      if (!warned)
        {
          msg_warning("WARNING: file source: default value of follow_freq in file sources has changed in " VERSION_3_0 " to '1' for all files except /proc/kmsg",
                      NULL);
          warned = TRUE;
        }
    }
  else
    {
      if ((self->flags & AFFILE_PIPE) == 0)
        {
          if (0 ||
#if __linux__
              (strcmp(filename, "/proc/kmsg") == 0) ||
#elif __FreeBSD__
              (strcmp(filename, "/dev/klog") == 0) ||
#endif
               0)
            {
              self->reader_options.follow_freq = 0;
            }
          else
            {
              self->reader_options.follow_freq = 1000;
              self->reader_options.flags |= LR_PREEMPT;
            }
        }
    }
#if __linux__
  if (strcmp(filename, "/proc/kmsg") == 0)
    {
      self->flags |= AFFILE_PRIVILEGED;
    }
#endif

  if (pid_string == NULL)
     {
       pid_string = get_pid_string();
     }
  return &self->super.super;
}

void
affile_sd_free(LogPipe *s)
{
  AFFileSourceDriver *self = (AFFileSourceDriver *) s;
  gpointer i = NULL;
  if (self->file_list)
    {
      while ((i = g_queue_pop_head(self->file_list)) != NULL)
        {
          if (i != END_OF_LIST)
            g_free(i);
        }
      g_queue_free(self->file_list);
      self->file_list = NULL;
    }

  if (self->file_monitor)
    {
      file_monitor_free(self->file_monitor);
      self->file_monitor = NULL;
    }
  if(self->filename)
    {
      g_string_free(self->filename, TRUE);
      self->filename = NULL;
    }
  if (self->filename_pattern)
    {
      g_string_free(self->filename_pattern, TRUE);
      self->filename_pattern = NULL;
    }
  g_assert(!self->reader);

  log_reader_options_destroy(&self->reader_options);

  log_src_driver_free(s);
}


gboolean
affile_sd_open(LogPipe *s, gboolean immediate_check)
{
  gint fd;
  gboolean file_opened, open_deferred = FALSE;
  GlobalConfig *cfg = log_pipe_get_config(s);
  AFFileSourceDriver *self = (AFFileSourceDriver *) s;
  LogProtoServerOptions *options = (LogProtoServerOptions *)&self->proto_options;

  if (!log_src_driver_init_method(s))
    return FALSE;

  log_reader_options_init(&self->reader_options, cfg, self->super.super.group);

  file_opened = affile_sd_open_file(self, self->filename->str, &fd);
  if (!file_opened && self->reader_options.follow_freq > 0)
    {
      msg_info("Follow-mode file source not found, deferring open",
               evt_tag_str("filename", self->filename->str),
               evt_tag_id(MSG_FILE_SOURCE_NOT_FOUND),
               NULL);
      open_deferred = TRUE;
      fd = -1;
    }
  if (file_opened || open_deferred)
    {
      LogTransport *transport = NULL;
      LogProto *proto = NULL;

      self->reader = cfg_persist_config_fetch(cfg, affile_sd_format_persist_name(self->filename->str));
      if (self->reader == NULL)
        {
          transport = log_transport_plain_new(fd, 0);
          transport->timeout = 10;

          proto = affile_sd_construct_proto(self, transport);
          if (proto == NULL)
            {
              close(fd);
              return FALSE;
            }
          /* FIXME: we shouldn't use reader_options to store log protocol parameters */
          self->reader = log_reader_new(proto);
        }
      else
        {
          if (fd > 0)
            {
              close(fd);
            }
        }
      log_reader_set_options(self->reader, s, &self->reader_options, 1, SCS_FILE, self->super.super.id, self->filename->str, (LogProtoOptions *) options);
      log_reader_set_follow_filename(self->reader, self->filename->str);

      if (immediate_check)
        log_reader_set_immediate_check(self->reader);
      /* NOTE: if the file could not be opened, we ignore the last
       * remembered file position, if the file is created in the future
       * we're going to read from the start. */

      log_pipe_append(self->reader, s);
      if (!log_pipe_init(self->reader, cfg))
        {
          msg_error("Error initializing log_reader, closing fd",
                    evt_tag_int("fd", fd),
                    evt_tag_id(MSG_CANT_INITIALIZE_READER),
                    NULL);
          log_pipe_unref(self->reader);
          self->reader = NULL;
          close(fd);
          return FALSE;
        }

      if (proto)
        affile_sd_recover_state(s, cfg, proto);
    }
    else
    {
      msg_error("Error opening file for reading",
                evt_tag_str("filename", self->filename->str),
                evt_tag_errno(EVT_TAG_OSERROR, errno),
                evt_tag_id(MSG_CANT_OPEN_FILE),
                NULL);
      return self->super.super.optional;
    }
  return TRUE;
}

gboolean
affile_sd_init(LogPipe *s)
{
  AFFileSourceDriver *self = (AFFileSourceDriver *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);


  log_proto_check_server_options((LogProtoServerOptions *)&self->proto_options);
  log_reader_options_init(&self->reader_options, cfg, self->super.super.group);

  if (self->reader_options.follow_freq == 0)
    {
      affile_handle_zero_follow_freq(self);
    }

  if (self->file_monitor)
    {
      /* watch_directory will use the callback, so set it first */
      file_monitor_set_file_callback(self->file_monitor, affile_sd_monitor_callback, self);

      file_monitor_set_poll_freq(self->file_monitor, self->reader_options.follow_freq);

      if (!file_monitor_watch_directory(self->file_monitor, self->filename_pattern->str))
        {
          msg_error("Error starting filemonitor",
                    evt_tag_str("filemonitor", self->filename_pattern->str),
                    NULL);
          return FALSE;
        }
      else if (self->reader == NULL)
        {
          gboolean end_of_list = TRUE;
          gchar *filename = affile_pop_next_file(s, &end_of_list);
          if (filename)
            {
              g_string_assign(self->filename, filename);
              g_free(filename);
              return affile_sd_open(s, !end_of_list);
            }
        }
      return TRUE;
    }
  else
    {
      g_string_assign(self->filename, self->filename_pattern->str);
      return affile_sd_open(s, FALSE);
    }
}

gboolean
affile_sd_deinit(LogPipe *s)
{
  AFFileSourceDriver *self = (AFFileSourceDriver *) s;
  LogProtoServerOptions *options = (LogProtoServerOptions *)&self->proto_options;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (self->reader)
    {
      log_pipe_deinit(self->reader);
      cfg_persist_config_add(cfg, affile_sd_format_persist_name(self->filename->str), self->reader, affile_sd_destroy_reader, FALSE);
      self->reader = NULL;
    }

  if (self->file_monitor)
    {
      file_monitor_deinit(self->file_monitor);
    }

  affile_sd_regex_free(options->opts.prefix_matcher);
  affile_sd_regex_free(options->opts.garbage_matcher);
  if (options->opts.prefix_pattern)
    g_free(options->opts.prefix_pattern);
  if (options->opts.garbage_pattern)
    g_free(options->opts.garbage_pattern);

  if (!log_src_driver_deinit_method(s))
    return FALSE;

  return TRUE;
}

void
affile_sd_set_recursion(LogDriver *s, const gint recursion)
{
  AFFileSourceDriver *self = (AFFileSourceDriver *) s;
  if (self->file_monitor)
    self->file_monitor->recursion = recursion;
}


gboolean
affile_sd_set_multi_line_prefix(LogDriver *s, gchar *prefix)
{
  AFFileSourceDriver *self = (AFFileSourceDriver *) s;
  LogProtoServerOptions *options = (LogProtoServerOptions *)&self->proto_options;
  const gchar *error;
  gint erroroffset;
  int pcreoptions = 0;

  if (options->opts.prefix_pattern)
    g_free(options->opts.prefix_pattern);
  options->opts.prefix_pattern = g_strdup(prefix);
  options->opts.prefix_matcher = pcre_compile(prefix, pcreoptions, &error, &erroroffset, NULL);
  if (!options->opts.prefix_matcher)
    {
      msg_error("Bad regexp",evt_tag_str("multi_line_prefix", prefix), NULL);
      return FALSE;
    }

  return TRUE;
}

gboolean
affile_sd_set_multi_line_garbage(LogDriver *s, gchar *garbage)
{
  AFFileSourceDriver *self = (AFFileSourceDriver *) s;
  LogProtoServerOptions *options = (LogProtoServerOptions *)&self->proto_options;
  const gchar *error;
  gint erroroffset;
  int pcreoptions = 0;

  if (options->opts.garbage_pattern)
    g_free(options->opts.garbage_pattern);
  options->opts.garbage_pattern = g_strdup(garbage);
  options->opts.garbage_matcher = pcre_compile(garbage, pcreoptions, &error, &erroroffset, NULL);
  if (!options->opts.garbage_matcher)
    {
      msg_error("Bad regexp",evt_tag_str("multi_line_garbage", garbage), NULL);
      return FALSE;
    }

  return TRUE;
}



/*

DESTINATION

*/
/*
 * Threading notes:
 *
 * Apart from standard initialization/deinitialization (normally performed
 * by the main thread when syslog-ng starts up) the following processes are
 * performed in various threads.
 *
 *   - queue runs in the thread of the source thread that generated the message
 *   - if the message is to be written to a not-yet-opened file, a new gets
 *     opened and stored in the writer_hash hashtable (initiated from queue,
 *     but performed in the main thread, but more on that later)
 *   - currently opened destination files are checked regularly and closed
 *     if they are idle for a given amount of time (time_reap) (this is done
 *     in the main thread)
 *
 * Some of these operations have to be performed in the main thread, others
 * are done in the queue call.
 *
 * References
 * ==========
 *
 * The AFFileDestDriver instance is registered into the current
 * configuration, thus its presence is always given, it cannot go away while
 * syslog-ng is running.
 *
 * AFFileDestWriter instances are created dynamically when a new file is
 * opened. A reference is stored in the writer_hash hashtable. This is then:
 *    - looked up in _queue() (in the source thread)
 *    - cleaned up in reap callback (in the main thread)
 *
 * writer_hash is locked (currently a simple mutex) using
 * AFFileDestDriver->lock.  The "queue" method cannot hold the lock while
 * forwarding it to the next pipe, thus a reference is taken under the
 * protection of the lock, keeping a the next pipe alive, even if that would
 * go away in a parallel reaper process.
 */

static gchar *
affile_dw_format_persist_name(AFFileDestWriter *self)
{
  static gchar persist_name[1024];

  g_snprintf(persist_name, sizeof(persist_name),
             "affile_dw_queue(%s)",
             self->filename);
  return persist_name;
}

static void affile_dd_reap_writer(AFFileDestDriver *self, AFFileDestWriter *dw);

void
affile_dw_arm_reaper(AFFileDestWriter *self)
{
  /* not yet reaped, set up the next callback */
  iv_validate_now();
  if (iv_timer_registered(&self->reap_timer))
    {
      iv_timer_unregister(&self->reap_timer);
    }
  self->reap_timer.expires = iv_now;
  timespec_add_msec(&self->reap_timer.expires, self->owner->time_reap * 1000 / 2);
  iv_timer_register(&self->reap_timer);
}

static void
affile_dw_reap(gpointer s)
{
  AFFileDestWriter *self = (AFFileDestWriter *) s;

  main_loop_assert_main_thread();

  g_static_mutex_lock(&self->lock);
  if (!log_writer_has_pending_writes((LogWriter *) self->writer) &&
      !self->queue_pending &&
      (cached_g_current_time_sec() - self->last_msg_stamp) >= self->owner->time_reap)
    {
      g_static_mutex_unlock(&self->lock);
      msg_verbose("Destination timed out, reaping",
                  evt_tag_str("template", self->owner->filename_template->template),
                  evt_tag_str("filename", self->filename),
                  NULL);
      affile_dd_reap_writer(self->owner, self);
    }
  else
    {
      g_static_mutex_unlock(&self->lock);
      affile_dw_arm_reaper(self);
    }
}

static gboolean
affile_dw_init(LogPipe *s)
{
  AFFileDestWriter *self = (AFFileDestWriter *)s;
  if (!self->writer)
    {
      guint32 flags;

      flags = LW_FORMAT_FILE | ((self->owner->flags & AFFILE_PIPE) ? 0 : LW_SOFT_FLOW_CONTROL);

      self->writer = log_writer_new(flags);
    }
  log_writer_set_options((LogWriter *) self->writer, s, &self->owner->writer_options, 1,
                         self->owner->flags & AFFILE_PIPE ? SCS_PIPE : SCS_FILE,
                         self->owner->super.super.id, self->filename, NULL);

  log_writer_set_queue(self->writer, log_dest_driver_acquire_queue(&self->owner->super, affile_dw_format_persist_name(self)));
  if (!log_pipe_init(self->writer, NULL))
    {
      msg_error("Error initializing log writer", NULL);
      log_pipe_unref(self->writer);
      self->writer = NULL;
      return FALSE;
    }

  log_pipe_append(&self->super, self->writer);

  return affile_dw_reopen(self);
}

static gboolean
affile_dw_deinit(LogPipe *s)
{
  AFFileDestWriter *self = (AFFileDestWriter *) s;

  main_loop_assert_main_thread();
  if (self->writer)
    {
      log_pipe_deinit(self->writer);
    }
  log_dest_driver_release_queue(&self->owner->super, log_writer_get_queue(self->writer));
  log_writer_set_queue(self->writer, NULL);

  if (iv_timer_registered(&self->reap_timer))
    iv_timer_unregister(&self->reap_timer);
  return TRUE;
}

/*
 * NOTE: the caller (e.g. AFFileDestDriver) holds a reference to @self, thus
 * @self may _never_ be freed, even if the reaper timer is elapsed in the
 * main thread.
 */
static void
affile_dw_queue(LogPipe *s, LogMessage *lm, const LogPathOptions *path_options, gpointer user_data)
{
  AFFileDestWriter *self = (AFFileDestWriter *) s;

  if (!server_mode && lm->saddr && lm->saddr->sa.sa_family != AF_UNIX)
    {
      msg_error("syslog-ng running in client/relay mode, network messages cannot be written to files", NULL);
      log_msg_drop(lm, path_options);
      return;
    }

  g_static_mutex_lock(&self->lock);
  self->last_msg_stamp = cached_g_current_time_sec();
  if (self->last_open_stamp == 0)
    self->last_open_stamp = self->last_msg_stamp;

  if (!log_writer_opened((LogWriter *) self->writer) &&
      !self->reopen_pending &&
      (self->last_open_stamp < self->last_msg_stamp - self->time_reopen))
    {
      self->reopen_pending = TRUE;
      /* if the file couldn't be opened, try it again every time_reopen seconds */
      g_static_mutex_unlock(&self->lock);
      affile_dw_reopen(self);
      g_static_mutex_lock(&self->lock);
      self->reopen_pending = FALSE;
    }
  g_static_mutex_unlock(&self->lock);

  log_pipe_forward_msg(&self->super, lm, path_options);
}

static void
affile_dw_set_owner(AFFileDestWriter *self, AFFileDestDriver *owner)
{
  if (self->owner)
    log_pipe_unref(&self->owner->super.super.super);
  log_pipe_ref(&owner->super.super.super);
  self->owner = owner;
  if (self->writer)
    log_writer_set_options((LogWriter *) self->writer, &self->super, &owner->writer_options, 1, SCS_FILE, self->owner->super.super.id, self->filename, NULL);

}

static void
affile_dw_free(LogPipe *s)
{
  AFFileDestWriter *self = (AFFileDestWriter *) s;

  log_pipe_unref(self->writer);
  self->writer = NULL;
  g_free(self->filename);
  log_pipe_unref(&self->owner->super.super.super);
  log_pipe_free_method(s);
}

static void
affile_dw_notify(LogPipe *s, LogPipe *sender, gint notify_code, gpointer user_data)
{
  switch(notify_code)
  {
    case NC_REOPEN_REQUIRED:
      {
        affile_dw_reopen((AFFileDestWriter *)s);
        break;
      }
  }
}

static AFFileDestWriter *
affile_dw_new(AFFileDestDriver *owner, const gchar *filename)
{
  AFFileDestWriter *self = g_new0(AFFileDestWriter, 1);

  log_pipe_init_instance(&self->super);

  self->super.init = affile_dw_init;
  self->super.deinit = affile_dw_deinit;
  self->super.free_fn = affile_dw_free;
  self->super.queue = affile_dw_queue;
  self->super.notify = affile_dw_notify;
  log_pipe_ref(&owner->super.super.super);
  self->owner = owner;
  self->time_reopen = 60;

  IV_TIMER_INIT(&self->reap_timer);
  self->reap_timer.cookie = self;
  self->reap_timer.handler = affile_dw_reap;

  /* we have to take care about freeing filename later.
     This avoids a move of the filename. */
  self->filename = g_strdup(filename);
  g_static_mutex_init(&self->lock);
  return self;
}

void
affile_dd_set_file_uid(LogDriver *s, const gchar *file_uid)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;

  self->file_uid = 0;
  if (!resolve_user(file_uid, &self->file_uid))
    {
      msg_error("Error resolving user",
                 evt_tag_str("user", file_uid),
                 NULL);
    }
}

void
affile_dd_set_file_gid(LogDriver *s, const gchar *file_gid)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;

  self->file_gid = 0;
  if (!resolve_group(file_gid, &self->file_gid))
    {
      msg_error("Error resolving group",
                 evt_tag_str("group", file_gid),
                 NULL);
    }
}

void
affile_dd_set_file_perm(LogDriver *s, gint file_perm)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;

  self->file_perm = file_perm;
}

void
affile_dd_set_dir_uid(LogDriver *s, const gchar *dir_uid)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;

  self->dir_uid = 0;
  if (!resolve_user(dir_uid, &self->dir_uid))
    {
      msg_error("Error resolving user",
                 evt_tag_str("user", dir_uid),
                 NULL);
    }
}

void
affile_dd_set_dir_gid(LogDriver *s, const gchar *dir_gid)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;

  self->dir_gid = 0;
  if (!resolve_group(dir_gid, &self->dir_gid))
    {
      msg_error("Error resolving group",
                 evt_tag_str("group", dir_gid),
                 NULL);
    }
}

void
affile_dd_set_dir_perm(LogDriver *s, gint dir_perm)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;

  self->dir_perm = dir_perm;
}

void
affile_dd_set_create_dirs(LogDriver *s, gboolean create_dirs)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;

  if (create_dirs)
    self->flags |= AFFILE_CREATE_DIRS;
  else
    self->flags &= ~AFFILE_CREATE_DIRS;
}

void
affile_dd_set_fsync(LogDriver *s, gboolean fsync)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;
  if (fsync)
    self->flags |= AFFILE_FSYNC;
  else
    self->flags &= ~AFFILE_FSYNC;
}

void
affile_dd_set_overwrite_if_older(LogDriver *s, gint overwrite_if_older)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;

  self->overwrite_if_older = overwrite_if_older;
}

void
affile_dd_set_local_time_zone(LogDriver *s, const gchar *local_time_zone)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;

  self->local_time_zone = g_strdup(local_time_zone);
}

static inline gchar *
affile_dd_format_persist_name(AFFileDestDriver *self)
{
  static gchar persist_name[1024];

  g_snprintf(persist_name, sizeof(persist_name), "affile_dd_writers(%s)", self->filename_template->template);
  return persist_name;
}

static void
affile_dd_reap_writer(AFFileDestDriver *self, AFFileDestWriter *dw)
{
  main_loop_assert_main_thread();

  if ((self->flags & AFFILE_NO_EXPAND) == 0)
    {
      g_static_mutex_lock(&self->lock);
      /* remove from hash table */
      g_hash_table_remove(self->writer_hash, dw->filename);
      g_static_mutex_unlock(&self->lock);
    }
  else
    {
      g_static_mutex_lock(&self->lock);
      g_assert(dw == self->single_writer);
      self->single_writer = NULL;
      g_static_mutex_unlock(&self->lock);
    }

  log_pipe_deinit(&dw->super);
  log_pipe_unref(&dw->super);
}


/**
 * affile_dd_reuse_writer:
 *
 * This function is called as a g_hash_table_foreach() callback to set the
 * owner of each writer, previously connected to an AFileDestDriver instance
 * in an earlier configuration. This way AFFileDestWriter instances are
 * remembered accross reloads.
 *
 **/
static void
affile_dd_reuse_writer(gpointer key, gpointer value, gpointer user_data)
{
  AFFileDestDriver *self = (AFFileDestDriver *) user_data;
  AFFileDestWriter *writer = (AFFileDestWriter *) value;

  affile_dw_set_owner(writer, self);
  if (!log_pipe_init(&writer->super, NULL))
    {
      msg_error("Can't reinitialize the writer",
                NULL);
      writer->owner = NULL;
      log_pipe_unref(&self->super.super.super);
      g_hash_table_remove(self->writer_hash,key);
    }
}


static gboolean
affile_dd_init(LogPipe *s)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (!log_dest_driver_init_method(s))
    return FALSE;

  if (cfg->create_dirs)
    self->flags |= AFFILE_CREATE_DIRS;
  if (self->file_uid == -1)
    self->file_uid = cfg->file_uid;
  if (self->file_gid == -1)
    self->file_gid = cfg->file_gid;
  if (self->file_perm == -1)
    self->file_perm = cfg->file_perm;
  if (self->dir_uid == -1)
    self->dir_uid = cfg->dir_uid;
  if (self->dir_gid == -1)
    self->dir_gid = cfg->dir_gid;
  if (self->dir_perm == -1)
    self->dir_perm = cfg->dir_perm;
  if (self->time_reap == -1)
    self->time_reap = cfg->time_reap;

  log_writer_options_init(&self->writer_options, cfg, 0);
  log_template_options_init(&self->template_fname_options, cfg);

  if ((self->flags & AFFILE_NO_EXPAND) == 0)
    {
      self->writer_hash = cfg_persist_config_fetch(cfg, affile_dd_format_persist_name(self));
      if (self->writer_hash)
        g_hash_table_foreach(self->writer_hash, affile_dd_reuse_writer, self);
    }
  else
    {
      self->single_writer = cfg_persist_config_fetch(cfg, affile_dd_format_persist_name(self));
      if (self->single_writer)
        {
          affile_dw_set_owner(self->single_writer, self);
          if (!log_pipe_init(&self->single_writer->super, cfg))
            {
              msg_error("Can't reinitialize the writer",
                NULL);
              self->single_writer->owner = NULL;
              log_pipe_unref(&self->super.super.super);
              log_pipe_unref(&self->single_writer->super);
              self->single_writer = NULL;
            }
        }
    }


  return TRUE;
}


/**
 * This is registered as a destroy-notify callback for an AFFileDestWriter
 * instance. It destructs and frees the writer instance.
 **/
static void
affile_dd_destroy_writer(gpointer value)
{
  AFFileDestWriter *writer = (AFFileDestWriter *) value;

  main_loop_assert_main_thread();
  log_pipe_deinit(&writer->super);
  log_pipe_unref(&writer->super);
}

/*
 * This function is called as a g_hash_table_foreach_remove() callback to
 * free the specific AFFileDestWriter instance in the hashtable.
 */
static gboolean
affile_dd_destroy_writer_hr(gpointer key, gpointer value, gpointer user_data)
{
  affile_dd_destroy_writer(value);
  return TRUE;
}

/**
 * affile_dd_destroy_writer_hash:
 * @value: GHashTable instance passed as a generic pointer
 *
 * Destroy notify callback for the GHashTable storing AFFileDestWriter instances.
 **/
static void
affile_dd_destroy_writer_hash(gpointer value)
{
  GHashTable *writer_hash = (GHashTable *) value;

  g_hash_table_foreach_remove(writer_hash, affile_dd_destroy_writer_hr, NULL);
  g_hash_table_destroy(writer_hash);
}

static void
affile_dd_deinit_writer(gpointer key, gpointer value, gpointer user_data)
{
  log_pipe_deinit((LogPipe *) value);
}

static gboolean
affile_dd_deinit(LogPipe *s)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);
  /* NOTE: we free all AFFileDestWriter instances here as otherwise we'd
   * have circular references between AFFileDestDriver and file writers */
  if (self->single_writer)
    {
      g_assert(self->writer_hash == NULL);

      log_pipe_deinit(&self->single_writer->super);
      cfg_persist_config_add(cfg, affile_dd_format_persist_name(self), self->single_writer, affile_dd_destroy_writer, FALSE);
      self->single_writer = NULL;
    }
  else if (self->writer_hash)
    {
      g_assert(self->single_writer == NULL);

      g_hash_table_foreach(self->writer_hash, affile_dd_deinit_writer, NULL);
      cfg_persist_config_add(cfg, affile_dd_format_persist_name(self), self->writer_hash, affile_dd_destroy_writer_hash, FALSE);
      self->writer_hash = NULL;
    }

  if (!log_dest_driver_deinit_method(s))
    return FALSE;

  return TRUE;
}

/*
 * This function is ran in the main thread whenever a writer is not yet
 * instantiated.  Returns a reference to the newly constructed LogPipe
 * instance where the caller needs to forward its message.
 */
static LogPipe *
affile_dd_open_writer(gpointer args[])
{
  AFFileDestDriver *self = args[0];
  GlobalConfig *cfg = log_pipe_get_config(&self->super.super.super);
  AFFileDestWriter *next;

  main_loop_assert_main_thread();
  if (self->flags & AFFILE_NO_EXPAND)
    {
      if (!self->single_writer)
       {
         next = affile_dw_new(self, self->filename_template->template);
          if (next && log_pipe_init(&next->super, cfg))
           {
             log_pipe_ref(&next->super);
             g_static_mutex_lock(&self->lock);
             self->single_writer = next;
             g_static_mutex_unlock(&self->lock);
           }
         else
           {
             log_pipe_unref(&next->super);
             next = NULL;
           }
       }
      else
        {
          next = self->single_writer;
          log_pipe_ref(&next->super);
        }
    }
  else
    {
      GString *filename = args[1];

      /* hash table construction is serialized, as we only do that in the main thread. */
      if (!self->writer_hash)
        self->writer_hash = g_hash_table_new(g_str_hash, g_str_equal);

      /* we don't need to lock the hashtable as it is only written in
       * the main thread, which we're running right now.  lookups in
       * other threads must be locked. writers must be locked even in
       * this thread to exclude lookups in other threads.  */

      next = g_hash_table_lookup(self->writer_hash, filename->str);
      if (!next)
       {
         next = affile_dw_new(self, filename->str);
          if (!log_pipe_init(&next->super, cfg))
           {
             log_pipe_unref(&next->super);
             next = NULL;
           }
         else
           {
             log_pipe_ref(&next->super);
             g_static_mutex_lock(&self->lock);
              g_hash_table_insert(self->writer_hash, next->filename, next);
              g_static_mutex_unlock(&self->lock);
            }
        }
      else
        {
          log_pipe_ref(&next->super);
        }
    }

  if (next)
    {
      next->queue_pending = TRUE;
      /* we're returning a reference */
      return &next->super;
    }
  return NULL;
}

static void
affile_dd_queue(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options, gpointer user_data)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;
  AFFileDestWriter *next;
  gpointer args[2] = { self, NULL };

  log_dest_driver_counter_inc(s);

  if (self->flags & AFFILE_NO_EXPAND)
    {
      /* no need to lock the check below, the worst case that happens is
       * that we go to the mainloop to return the same information, but this
       * is not fast path anyway */

      g_static_mutex_lock(&self->lock);
      if (!self->single_writer)
        {
          g_static_mutex_unlock(&self->lock);
          next = main_loop_call((void *(*)(void *)) affile_dd_open_writer, args, TRUE);
        }
      else
        {
          /* we need to lock single_writer in order to get a reference and
           * make sure it is not a stale pointer by the time we ref it */
          next = self->single_writer;
          next->queue_pending = TRUE;
          log_pipe_ref(&next->super);
          g_static_mutex_unlock(&self->lock);
        }
    }
  else
    {
      GString *filename;

      filename = g_string_sized_new(32);
      log_template_format(self->filename_template, msg, &self->template_fname_options, LTZ_LOCAL, 0, NULL, filename);

      g_static_mutex_lock(&self->lock);
      if (self->writer_hash)
        next = g_hash_table_lookup(self->writer_hash, filename->str);
      else
        next = NULL;

      if (next)
        {
          log_pipe_ref(&next->super);
          next->queue_pending = TRUE;
          g_static_mutex_unlock(&self->lock);
        }
      else
       {
         g_static_mutex_unlock(&self->lock);
         args[1] = filename;
         next = main_loop_call((void *(*)(void *)) affile_dd_open_writer, args, TRUE);
       }
      g_string_free(filename, TRUE);
    }
  if (next)
    {
      log_pipe_queue(&next->super, msg, path_options);
      next->queue_pending = FALSE;
      log_pipe_unref(&next->super);
    }
  else
    log_msg_drop(msg, path_options);
}

static void
affile_dd_free(LogPipe *s)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;

  /* NOTE: this must be NULL as deinit has freed it, otherwise we'd have circular references */
  g_assert(self->single_writer == NULL && self->writer_hash == NULL);

  log_template_options_destroy(&self->template_fname_options);
  log_template_unref(self->filename_template);
  log_writer_options_destroy(&self->writer_options);
  log_dest_driver_free(s);
}

LogDriver *
affile_dd_new(gchar *filename, guint32 flags)
{
  AFFileDestDriver *self = g_new0(AFFileDestDriver, 1);

  log_dest_driver_init_instance(&self->super);
  self->super.super.super.init = affile_dd_init;
  self->super.super.super.deinit = affile_dd_deinit;
  self->super.super.super.queue = affile_dd_queue;
  self->super.super.super.free_fn = affile_dd_free;
  self->filename_template = log_template_new(configuration, NULL);
  log_template_compile(self->filename_template, filename, NULL);
  self->flags = flags;
  self->file_uid = self->file_gid = -1;
  self->file_perm = -1;
  self->dir_uid = self->dir_gid = -1;
  self->dir_perm = -1;
  log_writer_options_defaults(&self->writer_options);
  self->writer_options.mark_mode = MM_NONE;
  if (strchr(filename, '$') == NULL)
    {
      self->flags |= AFFILE_NO_EXPAND;
    }
  self->time_reap = -1;
  log_template_options_defaults(&self->template_fname_options);
  g_static_mutex_init(&self->lock);
  return &self->super.super;
}


gboolean
affile_dw_reopen(AFFileDestWriter *self)
{
  int fd;
  struct stat st;
  GlobalConfig *cfg = log_pipe_get_config(&self->owner->super.super.super);
  LogProto *proto = NULL;

  if (cfg)
    self->time_reopen = cfg->time_reopen;

  msg_verbose("Initializing destination file writer",
              evt_tag_str("template", self->owner->filename_template->template),
              evt_tag_str("filename", self->filename),
              NULL);


  self->last_open_stamp = self->last_msg_stamp;
  if (self->owner->overwrite_if_older > 0 &&
      stat(self->filename, &st) == 0 &&
      st.st_mtime < time(NULL) - self->owner->overwrite_if_older)
    {
      msg_info("Destination file is older than overwrite_if_older(), overwriting",
                 evt_tag_str("filename", self->filename),
                 evt_tag_int("overwrite_if_older", self->owner->overwrite_if_older),
                 NULL);
      unlink(self->filename);
    }

    if (affile_dw_reopen_file(self->owner, self->filename, &fd))
    {
      guint write_flags;

      write_flags = ((self->owner->flags & AFFILE_PIPE) ? LTF_PIPE : LTF_APPEND);

      self->owner->proto_options.super.size = self->owner->writer_options.flush_lines;
      self->owner->proto_options.super.flags |= ((self->owner->flags & AFFILE_FSYNC) ? LPBS_FSYNC : 0);
      if (!self->owner->proto_factory)
        {
          self->owner->proto_factory = log_proto_get_factory(cfg,LPT_CLIENT,(self->owner->flags & AFFILE_PIPE) ? "stream-newline" : "file-writer" );
        }
      if (self->owner->proto_factory)
        {
          proto = self->owner->proto_factory->create(log_transport_plain_new(fd, write_flags), &self->owner->proto_options, cfg);
        }
      else
        {
          msg_error("Can't find proto",evt_tag_str("proto_name","file-writer"),NULL);
          close(fd);
          return FALSE;
        }

      main_loop_call((void * (*)(void *)) affile_dw_arm_reaper, self, TRUE);
    }
  else
    {
      msg_error("Error opening file for writing",
                evt_tag_str("filename", self->filename),
                evt_tag_errno(EVT_TAG_OSERROR, errno),
                NULL);
    }

  log_writer_reopen(self->writer, proto, NULL);

  return TRUE;
}
