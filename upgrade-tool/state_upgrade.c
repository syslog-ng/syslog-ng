#include "state_upgrade.h"
#include "config_store.h"
#include "state.h"
#include "cfg.h"
#include "plugin.h"
#include "mainloop.h"
#include "messages.h"

#include <sys/stat.h>

#define STATES_ROOT "SOFTWARE\\Balabit\\syslog-ng Agent\\States\\"

#define FILE_STATES STATES_ROOT "FileSources"
#define EVENT_STATES STATES_ROOT "EventSources"

#define FILE_STATE_PREFIX "affile_sd_curpos"

#define EVT_PREFIX "evtlog_reader_curpos"
#define EVTX_PREFIX "evtxlog_reader_curpos"

GlobalConfig *configuration;

StateHandler *
create_state_handler_with_state(gchar *prefix, gchar *persist_name)
{
  STATE_HANDLER_CONSTRUCTOR constructor = NULL;
  StateHandler *handler = NULL;

  constructor = state_handler_get_constructor_by_prefix(prefix);

  if (!constructor)
    {
      msg_error("Can't get state handler constructor", evt_tag_str("prefix", prefix), NULL);
      goto exit;
    }

  handler = constructor(configuration->state, persist_name);
  if (!handler)
    {
      msg_error("Can't create state handler", evt_tag_str("persist_name", persist_name), NULL);
      goto exit;
    }
  if (!state_handler_entry_exists(handler))
    {
      if (state_handler_alloc_state(handler) == 0)
        {
          msg_error("Can't allocate state", evt_tag_str("persist_name", persist_name), NULL);
          state_handler_free(handler);
          handler = NULL;
          goto exit;
        }
    }
exit:
  return handler;
}

gboolean
upgrade_tool_generate_file_state(ConfigStore *store, gchar *root)
{
  gchar *filename = NULL;
  gchar *lower_filename = NULL;
  gchar *persist_name = NULL;
  NameValueContainer *container = NULL;
  StateHandler *handler = NULL;
  GError *err = NULL;
  gboolean result = TRUE;

  guint64 filepos;
  struct stat st;
  if (!config_store_open(store, root))
    {
      goto exit;
    }

  filename = config_store_get_string(store, "CurrentFileName");
  lower_filename = g_utf8_strdown(filename, -1);

  if (stat(filename, &st) == -1)
    {
      goto exit;
    }

  filepos = config_store_get_number(store, "CurrentPositionL") + ((guint64)(config_store_get_number(store, "CurrentPositionH")) << 32);
  persist_name = g_strdup_printf(FILE_STATE_PREFIX"(%s)",lower_filename);

  handler = create_state_handler_with_state(FILE_STATE_PREFIX, persist_name);
  if (!handler)
    {
      result = FALSE;
      goto exit;
    }

  container = state_handler_dump_state(handler);

  name_value_container_add_int(container, "version", 1);
  name_value_container_add_boolean(container, "big_endian", FALSE);
  name_value_container_add_int64(container, "raw_stream_pos", filepos);
  name_value_container_add_int64(container, "file_size", st.st_size);
  name_value_container_add_int64(container, "file_inode", 1);
  name_value_container_add_int(container, "run_id", 0);

  if (!state_handler_load_state(handler, container, &err))
    {
      msg_error("Can't load state", evt_tag_str("error", err->message), NULL);
      result = FALSE;
    }
exit:
  g_free(persist_name);
  g_free(filename);
  g_free(lower_filename);
  if (handler)
    {
      state_handler_free(handler);
    }
  if (container)
    {
      name_value_container_free(container);
    }
  if (err)
    {
      g_error_free(err);
    }
  return result;
}

gboolean
upgrade_file_sources_states(ConfigStore *store, gchar *root)
{
  GList *file_states;
  GList *iterator;
  gboolean result = TRUE;
  if (!config_store_open(store, root))
    {
      return TRUE;
    }

  file_states =  config_store_enumerate_keys(store);
  iterator = file_states;
  while (iterator)
    {
      gchar *path = g_strdup_printf("%s\\%s", root, (gchar *)iterator->data);
      if (config_store_open(store, path))
        {
          gchar *filename = config_store_get_string(store, "CurrentFileName");
          if (!filename)
            {
              result = result && upgrade_file_sources_states(store, path);
            }
          else
            {
              result = result && upgrade_tool_generate_file_state(store, path);
            }
          g_free(path);
        }
      iterator = iterator->next;
    }
  return result;
}

gboolean
upgrade_tool_generate_evenlog_state(ConfigStore *store, gchar *root, gchar *channel_name)
{
  gboolean result = TRUE;
  guint32 pos;
  gchar *bookmark;
  NameValueContainer *container = NULL;
  StateHandler *handler = NULL;
  GError *err = NULL;
  gchar *persist_name;
  gchar *prefix = NULL;

  if (!config_store_open(store, root))
    {
      goto exit;
    }

  bookmark = config_store_get_string(store, "XmlBookmark");
  pos = config_store_get_number(store, "CurrentPositionL");
  prefix = bookmark ? EVTX_PREFIX : EVT_PREFIX;

  persist_name = g_strdup_printf("%s(%s)", prefix, channel_name);

  handler = create_state_handler_with_state(prefix, persist_name);
  if (!handler)
    {
      result = FALSE;
      goto exit;
    }

  container = state_handler_dump_state(handler);

  name_value_container_add_int(container, "version", 1);
  name_value_container_add_boolean(container, "big_endian", FALSE);
  if (bookmark)
    {
      name_value_container_add_string(container, "bookmark_xml", bookmark);
    }
  else
    {
      name_value_container_add_int(container, "record_id", pos);
    }

  if (!state_handler_load_state(handler, container, &err))
    {
      msg_error("Can't load state", evt_tag_str("error",err->message), NULL);
      result = FALSE;
    }

exit:
  g_free(persist_name);
  g_free(bookmark);
  if (handler)
    {
      state_handler_free(handler);
    }
  if (err)
    {
      g_error_free(err);
    }
  return result;
}

gboolean
upgrade_eventlog_sources_states(ConfigStore *store, gchar *root)
{
  GList *event_states;
  GList *iterator;
  gboolean result = TRUE;

  if (!config_store_open(store, root))
    {
      return TRUE;
    }
  event_states =  config_store_enumerate_keys(store);
  iterator = event_states;
  while (iterator)
    {
      gchar *path = g_strdup_printf("%s\\%s", root, (gchar *)iterator->data);
      if (config_store_open(store, path))
        {
          result = result && upgrade_tool_generate_evenlog_state(store, path, (gchar *)iterator->data);
        }
      g_free(path);
      iterator = iterator->next;
    }
  return result;
}

gboolean
upgrade_registry_state(gchar *persist_dir)
{
  gboolean result = TRUE;
  ConfigStore *store = config_store_new(STORE_TYPE_REGISTRY);
  gchar *persist_filename = g_strdup_printf("%s\\syslog-ng.persist", persist_dir);

  if (!config_store_open(store, STATES_ROOT))
    {
      msg_error("Can't open states key", evt_tag_str("key", STATES_ROOT), NULL);
      result = TRUE;
      goto exit;
    }
  if (g_file_test(persist_filename, G_FILE_TEST_EXISTS))
    {
      msg_error("Abort state upgrade, because persist file already exists", evt_tag_str("persist file", persist_filename), NULL);
      result = FALSE;
      goto exit;
    }

  if (!g_threads_got_initialized)
    {
      g_thread_init(NULL);
    }
  main_thread_handle = g_thread_self();

  configuration = cfg_new(0);
  configuration->state = persist_state_new(persist_filename);
  persist_state_set_mode(configuration->state, persist_mode_edit);
  persist_state_start(configuration->state);
  if (config_store_open(store, FILE_STATES))
    {
      if (!plugin_load_module("basic-proto", configuration, NULL ))
        {
          msg_error("Can't load basic-proto plugin", NULL);
          result = FALSE;
        }
      else
        {
          result = upgrade_file_sources_states(store, FILE_STATES);
        }
    }
  if (config_store_open(store, EVENT_STATES))
    {
      if (!plugin_load_module("eventlog", configuration, NULL ))
        {
          msg_error("Can't load eventlog plugin", NULL);
          result = FALSE;
        }
      else
        {
          result = upgrade_eventlog_sources_states(store, EVENT_STATES);
        }
    }
exit:
  if (configuration)
    {
      if (result)
        {
          persist_state_commit(configuration->state);
          config_store_open(store, STATES_ROOT);
          config_store_delete_key(store);
        }
      else
        {
          persist_state_cancel(configuration->state);
        }
      cfg_free(configuration);
    }
  config_store_free(store);
  return result;
}
