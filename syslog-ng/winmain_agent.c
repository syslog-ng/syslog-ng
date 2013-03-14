/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2010 Balázs Scheidler
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include "syslog-ng.h"
#include "cfg.h"
#include "messages.h"
#include "memtrace.h"
#include "children.h"
#include "misc.h"
#include "stats.h"
#include "apphook.h"
#include "alarms.h"
#include "logqueue.h"
#include "gprocess.h"
#include "control.h"
#include "timeutils.h"
#include "logsource.h"
#include "mainloop.h"
#include "plugin.h"
#include "reloc.h"
#include "winservice.h"
#include "minidump.h"

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <signal.h>
#include <unistd.h>
#include <fcntl.h>

#if HAVE_GETOPT_H
#include <getopt.h>
#endif

static const gchar *agent_registry_config = "@version: 4.2\n@module eventlog\n@module agent-config type(REGISTRY) name(ac)\noptions{ threaded(yes); };\nac()\n";
static const gchar *agent_xml_config_format = "@version: 4.2\n@module eventlog\n@module agent-config type(XML) file(\"%s\") name(ac)\noptions { threaded(yes); }; \nac()\n";

gboolean event_log = FALSE;
extern gboolean debug_flag;
extern gboolean verbose_flag;
extern gchar *preprocess_into;
extern gboolean generate_persist_file;
gboolean remove_service = FALSE;
gchar *xml_config_file_name = NULL;
gchar **other_options = NULL;
gboolean show_version = FALSE;
gboolean help = FALSE;
gboolean quiet = FALSE;
static gchar *install_dat_filename;
static gchar *installer_version = NULL;
static gboolean service_manipulation = FALSE;

#define AGENT_SERVICE_NAME "syslog-ng Agent"
#define AGENT_SERVICE_DESCRIPTION "Collects log messages from eventlog containers and log files, and forwards them to a central syslog-ng server."
#define AGENT_SERVICE_DISPLAY_NAME "syslog-ng Agent"

static gboolean
install_service_callback(const gchar *option_name, const gchar *value, gpointer data, GError **error)
{
  gchar *service_args = NULL;
  if (value)
    {
      service_args = g_strdup_printf("/C %s",value);
    }
  install_service(AGENT_SERVICE_NAME,
                         AGENT_SERVICE_DISPLAY_NAME,
                         AGENT_SERVICE_DESCRIPTION,
                         SERVICE_AUTO_START,
                         service_args,
                         NULL,
                         NULL,
                         NULL,
                         FALSE);
  fprintf(stderr,"Service installed\n");
  service_manipulation = TRUE;
  return TRUE;
}

static GOptionEntry debug_options[] =
{
  { "Eventlog", 'E', 0, G_OPTION_ARG_NONE, &event_log, "Start the syslog-ng Agent in debug mode and send the messages to the Application eventlog container.",NULL},
  { "Debug",    'D', 0, G_OPTION_ARG_NONE, &debug_flag, "Start the syslog-ng Agent in debug mode. The debug messages can be captured using the Dbgview application.", NULL},
  { NULL },
};

static GOptionEntry service_options[] =
{
  {"Install",   'I', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, install_service_callback, "Register the syslog-ng Agent as a service. If XML configuration file was given as parameter, the service will be registered to use it.",NULL},
  {"Remove",    'R', 0, G_OPTION_ARG_NONE, &remove_service, "Remove the syslog-ng Agent service", NULL},
  {NULL},
};

static GOptionEntry application_options[] =
{
  {"help",            'h', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &help, NULL, NULL},
  {"help",            '?', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &help, NULL, NULL},
  {"Config",          'C', 0, G_OPTION_ARG_FILENAME, &xml_config_file_name, "Start the syslog-ng Agent using the specified XML configuration file.",NULL},
  {"Version",         'V', 0, G_OPTION_ARG_NONE, &show_version, "Display version information.",NULL},
  {"cfgfile",         'f', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_STRING, &cfgfilename, "Set config file name, default=" PATH_SYSLOG_NG_CONF, "<config>" },
  {"quiet",           'Q', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &quiet , NULL, NULL},
  {"preprocess-into",  0,  G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_STRING, &preprocess_into, "Write the preprocessed configuration file to the file specified", "output" },
  {"skip-old-msg",    's', 0, G_OPTION_ARG_NONE, &generate_persist_file, "Skip old messages", NULL},
  {NULL},
};

#define INSTALL_DAT_INSTALLER_VERSION "INSTALLER_VERSION"

gboolean
get_installer_version(gchar **inst_version)
{
  gchar line[1024];
  gboolean result = FALSE;
  FILE *f_install = fopen(install_dat_filename, "r");

  if (!f_install)
    return FALSE;

  while (fgets(line, sizeof(line), f_install) != NULL)
    {
      if (strncmp(line, INSTALL_DAT_INSTALLER_VERSION, strlen(INSTALL_DAT_INSTALLER_VERSION)) == 0)
        {
          gchar *pos = strchr(line, '=');
          if (pos)
            {
              *inst_version = strdup(pos+1);
              result = TRUE;
              break;
            }
        }
    }
  fclose(f_install);
  return result;
}

#define ON_OFF_STR(x) (x ? "on" : "off")

void
version(void)
{
  if (!get_installer_version(&installer_version) || installer_version == NULL)
    {
      installer_version = VERSION;
    }
  printf("syslog-ng Agent for windows " COMBINED_VERSION "\n"
         "Installer-Version: %s\n"
         "Revision: " SOURCE_REVISION "\n"
#if WITH_COMPILE_DATE
         "Compile-Date: " __DATE__ " " __TIME__ "\n"
#endif
         ,
         installer_version
         );
}

int
agent_service_main()
{
  gchar *config_string;
  gint rc = 0;
  register_minidump_writer();
  if (xml_config_file_name)
    {
      gchar *correct_path_name = escape_windows_path(xml_config_file_name);
      config_string = g_strdup_printf(agent_xml_config_format,correct_path_name);
      g_free(correct_path_name);
    }
  else
    {
      config_string = g_strdup(agent_registry_config);
    }
  rc = main_loop_init(config_string);
  SetEvent(main_loop_initialized);
  if (rc)
    {
      goto exit;
    }
  else
    {
      if (preprocess_into)
        {
          goto exit;
        }
    }
  /* we are running as a non-root user from this point */
  app_post_daemonized();
  app_post_config_loaded();
  /* from now on internal messages are written to the system log as well */
  rc = main_loop_run();

  app_shutdown();
exit:
  return rc;
}

int
main(int argc, char *argv[])
{
  GOptionContext *ctx;
  GOptionGroup *debug_group;
  GOptionGroup *service_group;
  GError *error = NULL;
  ctx = g_option_context_new(NULL);
  gint i = 0;
  install_dat_filename = get_reloc_string(PATH_INSTALL_DAT);
  setvbuf(stderr, NULL, _IONBF, 0);

  main_loop_initialized = CreateEvent(NULL, FALSE, FALSE, NULL);


  debug_group = g_option_group_new("debug","Debug options:","debug",NULL,NULL);
  g_option_group_add_entries(debug_group,debug_options);
  g_option_context_add_group(ctx,debug_group);

  service_group = g_option_group_new("service","Service options:","service",NULL,NULL);
  g_option_group_add_entries(service_group,service_options);
  g_option_context_add_group(ctx,service_group);

  g_option_context_add_main_entries(ctx,application_options,NULL);
  g_option_context_set_help_enabled(ctx,FALSE);

  for (i = 1; i < argc; i++)
    {
      if (argv[i][0] == '/')
        {
          if (strlen(argv[i]) == 2)
            {
              argv[i][0] = '-';
            }
          else
            {
              argv[i] = g_strdup_printf("--%s",&argv[i][1]);
            }
        }
    }

  cfgfilename = NULL;
  if (!g_option_context_parse(ctx, &argc, &argv, &error))
    {
      fprintf(stderr, "Error parsing command line arguments: %s\n", error ? error->message : "Invalid arguments");
      g_option_context_free(ctx);
      return 1;
    }
  if (help)
    {
      gchar *help_text = g_option_context_get_help(ctx,FALSE,NULL);
      gchar *p = help_text + 1;
      while(*p)
        {
          if ((*(p - 1) == ' ') && (*p == '-') && (*(p + 1) == '-'))
            {
              *p = ' ';
              p++;
              *p = '/';
            }
          if ((*(p - 1) == ' ') && (*p == '-'))
            {
              *p='/';
            }
          p++;
        }
      fprintf(stderr, help_text);
      g_option_context_free(ctx);
      return 0;
    }
  g_option_context_free(ctx);
  if (show_version)
    {
      version();
      return 0;
    }
  if (remove_service)
    {
      uninstall_service(AGENT_SERVICE_NAME);
      service_manipulation = TRUE;
    }
  if (service_manipulation)
    {
      return 0;
    }
  register_resource_dll("syslog-ng Agent");
  if (debug_flag)
    {
      log_stderr = TRUE;
      if (!quiet)
        {
          verbose_flag = TRUE;
        }
      else
        {
          debug_flag = FALSE;
        }
      return agent_service_main();
    }
  else
    {
      start_service(AGENT_SERVICE_NAME,agent_service_main,main_loop_terminate);
      return 0;
    }
  return 0;
}
