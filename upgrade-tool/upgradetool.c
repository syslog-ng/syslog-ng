#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <libxml/tree.h>
#include <libxml/HTMLtree.h>
#include "config_store.h"
#include "upgrade_config.h"
#ifdef _WIN32
#include "gpo_utils.h"
#include "state_upgrade.h"
#endif
#include "upgrade_log.h"
#include "misc.h"
#include "messages.h"
#include "logmsg.h"

static gboolean print_version = FALSE;
static gchar *dest_version = CURRENT_CONFIG_VERSION;
static gboolean discover_gpos = FALSE;
static gchar *update_gpo = NULL;
static gchar *binary_file = NULL;
static gchar *registry_subkey = "Local Settings";
static gchar *xml_file = NULL;
static gchar *persist_dir = NULL;

static UpgradeLog *log = NULL;

gchar *
swap_escape_characters(const gchar *message)
{
  GString *result = g_string_sized_new(1024);
  const gchar *pos = message;
  gchar *p = strstr(message, "\\x0a");
  while(p)
    {
      g_string_append_len(result, pos, p - pos);
      g_string_append_c(result, '\n');
      p += 4;
      pos = p;
      p = strstr(pos, "\\x0a");
    }
  g_string_append(result, pos);
  return g_string_free(result, FALSE);
}

void
upgrade_tool_log_function(LogMessage *msg)
{
  gchar *message = swap_escape_characters(log_msg_get_value(msg, LM_V_MESSAGE, NULL));
  if (log)
    {
      guint8 pri =  msg->pri & 7;
      if (pri == 3)
        {
          upgrade_log_error(log, message);
        }
      else if (pri == 4)
        {
          upgrade_log_warning(log, message);
        }
      else if (pri == 5)
        {
          upgrade_log_info(log, message);
        }
    }
  fprintf(stderr, "%s\n",message);
  g_free(message);
}


#define SYSLOG_NG_AGENT_XML_ROOT "SOFTWARE\\BalaBit\\syslog-ng Agent\\Local Settings\\"

#define XML_FILE_BACKUP "_backup"

ConfigStore *
get_config_store_by_xml(gchar *filename)
{
  ConfigStore *config_store = config_store_new(STORE_TYPE_XML);
  if (!config_store_parse_file(config_store, filename, NULL))
    {
      config_store_free(config_store);
      config_store = NULL;
    }
  return config_store;
}

gint
upgrade_xml(gchar *xml_file)
{
  FILE *f;
  gint result = 0;
  GString *backup_file_name = g_string_new(xml_file);
  UpgradeConfig *up = NULL;
  ConfigStore *config_store;

  g_string_append(backup_file_name, XML_FILE_BACKUP);
  config_store = get_config_store_by_xml(xml_file);

  if (!config_store)
    {
      msg_error("Invalid xml file", evt_tag_str("file", xml_file), NULL);
      g_string_free(backup_file_name, TRUE);
      return -10;
    }

  if (rename(xml_file, backup_file_name->str) != 0)
    {
      msg_error("Create backup from the xml file is failed", evt_tag_str("file", backup_file_name->str), NULL);
      g_string_free(backup_file_name, TRUE);
      config_store_free(config_store);
      return -10;
    }
  up = upgrade_config_new(SYSLOG_NG_AGENT_XML_ROOT, config_store);
  if (!upgrade_config_do_upgrade(up))
    {
      result = -12;
    }
  f = fopen(xml_file, "w");
  config_store_save_to_file(config_store, f);
  fclose(f);
  g_string_free(backup_file_name, TRUE);
  upgrade_config_free(up);
  return result;
}

static GOptionEntry options[] =
  {
    { "version", 'v', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_NONE, &print_version,
        "Displays the possible conversion paths", NULL },
    { "dest-version", 'd', 0, G_OPTION_ARG_STRING, &dest_version,
        "Attemps to convert to the given config version", "dest_version" },
    { "discover-gpo", 'g', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_NONE,
        &discover_gpos, "Print all available GPOID", NULL },
    { "upgrade-gpo", 'o', 0, G_OPTION_ARG_STRING, &update_gpo,
        "Upgrade the given GPO configuration", "GPOID" },
    { "upgrade-reg-bin", 'b', 0, G_OPTION_ARG_STRING, &binary_file,
        "Upgrade the given registry binary file", "binary_file" },
    { "upgrade-reg-key", 'k', 0, G_OPTION_ARG_STRING, &registry_subkey,
        "Upgrade the given registry subkey", "subkeyname" },
    { "upgrade-xml-file", 'x', 0, G_OPTION_ARG_STRING, &xml_file,
        "Upgrade the given xml configuration file", "xml_file" },
    { "upgrade-state", 's',  0,  G_OPTION_ARG_STRING, &persist_dir, "Move state from registry to syslog-ng.persist under the given directory", "<dir>"},
    { NULL } };

gint
main(gint argc, gchar **argv)
{
  GError *error = NULL;
  gchar *html_output;
  gchar *p;
  gint result = 0;
  GOptionContext *ctx = g_option_context_new(NULL );
  g_option_context_add_main_entries(ctx, options, NULL );
  msg_init(FALSE);
  log_msg_registry_init();
  msg_set_post_func(upgrade_tool_log_function);
  html_output = g_strdup(argv[0]);
  p = strrchr(html_output, '.');
  if (p)
    {
      *p = '\0';
    }
  p = html_output;
  html_output = g_strdup_printf("%s.html",p);
  g_free(p);
  if (!g_option_context_parse(ctx, &argc, &argv, &error))
    {
      msg_error("Can't parse command line", evt_tag_str("error", error->message), NULL);
      return -1;
    }
  if (print_version)
    {
      printf("Supported updates:\n");
      printf("\t4.0.1 -> 5.0.1\n");
      return 0;
    }
  log = upgrade_log_new(html_output);
#ifdef _WIN32
  if (discover_gpos)
    {
      GHashTable *gpo_map = GPODisover(TRUE);
      if (gpo_map)
        {
          g_hash_table_destroy(gpo_map);
        }
      return 0;
    }
#endif
  if (xml_file)
    {
      return upgrade_xml(xml_file);
    }
#ifdef _WIN32
  if (update_gpo)
    {
      return upgrade_gpo_config(update_gpo);
    }
  else if (binary_file)
    {
      return upgrade_binary_reg_file(binary_file);
    }
  else if (persist_dir)
    {
      return upgrade_registry_state(persist_dir) ? 0 : -1;
    }
  else
    {
      return upgrade_registry_path(registry_subkey);
    }
#endif
  return result;
}
