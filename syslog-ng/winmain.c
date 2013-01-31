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
#include "winservice.h"
#include "reloc.h"

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


static gchar *install_dat_filename;
static gchar *installer_version = NULL;
static gboolean display_version = FALSE;
static gboolean display_module_registry = FALSE;
static gboolean dummy = FALSE;
static gboolean foreground = FALSE;
static gboolean install = FALSE;
static gboolean uninstall = FALSE;
static gboolean start = FALSE;
static gboolean stop = FALSE;
extern gchar *preprocess_into;

#define SERVER_SERVICE_NAME "syslog-ng"
#define SERVER_SERVICE_DESCRIPTION "Collects log messages from eventlog containers and log files, and forwards them to a central syslog-ng server."
#define SERVER_SERVICE_DISPLAY_NAME "syslog-ng"

#ifdef YYDEBUG
extern int cfg_parser_debug;
#endif

static GOptionEntry syslogng_options[] =
{
  { "version",           'V',         0, G_OPTION_ARG_NONE, &display_version, "Display version number (" PACKAGE " " COMBINED_VERSION ")", NULL },
  { "module-path",         0,         0, G_OPTION_ARG_STRING, &module_path, "Set the list of colon separated directories to search for modules, default=" MODULE_PATH, "<path>" },
  { "module-registry",     0,         0, G_OPTION_ARG_NONE, &display_module_registry, "Display module information", NULL },
  { "default-modules",     0,         0, G_OPTION_ARG_STRING, &default_modules, "Set the set of auto-loaded modules, default=" DEFAULT_MODULES, "<module-list>" },
  { "seed",              'S',         0, G_OPTION_ARG_NONE, &dummy, "Does nothing, the need to seed the random generator is autodetected", NULL},
#ifdef YYDEBUG
  { "yydebug",           'y',         0, G_OPTION_ARG_NONE, &cfg_parser_debug, "Enable configuration parser debugging", NULL },
#endif
  { "qdisk-dir",         'Q',         0, G_OPTION_ARG_STRING, &qdisk_dir, "Set the name of the disk buffer directory", "<dir>" },
  { "foreground",        'F',         0, G_OPTION_ARG_NONE, &foreground, "Run syslog-ng in foreground (console mode)", NULL},
  { "install",           'I',         0, G_OPTION_ARG_NONE, &install, "Install service", NULL},
  { "uninstall",         'U',         0, G_OPTION_ARG_NONE, &uninstall, "Uninstall service", NULL},
  { "start",              0,         0, G_OPTION_ARG_NONE, &start, "Start service", NULL},
  { "stop",              0,         0, G_OPTION_ARG_NONE, &stop, "Stop service", NULL},
  { NULL },
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
  printf(PACKAGE " " COMBINED_VERSION "\n"
         "Installer-Version: %s\n"
         "Revision: " SOURCE_REVISION "\n"
#if WITH_COMPILE_DATE
         "Compile-Date: " __DATE__ " " __TIME__ "\n"
#endif
         "Default-Modules: %s\n"
         "Available-Modules: ",
         installer_version,
         default_modules);

  plugin_list_modules(stdout, FALSE);

  printf("Enable-Debug: %s\n"
         "Enable-GProf: %s\n"
         "Enable-Memtrace: %s\n"
         "Enable-IPv6: %s\n"
         "Enable-Spoof-Source: %s\n"
         "Enable-TCP-Wrapper: %s\n"
         "Enable-Linux-Caps: %s\n"
         "Enable-Pcre: %s\n",
         ON_OFF_STR(ENABLE_DEBUG),
         ON_OFF_STR(0),
         ON_OFF_STR(0),
         ON_OFF_STR(ENABLE_IPV6),
         ON_OFF_STR(0),
         ON_OFF_STR(0),
         ON_OFF_STR(0),
         ON_OFF_STR(ENABLE_PCRE));
}

int server_service_main()
{
  gint rc = 0;
  rc = main_loop_init(NULL);
  SetEvent(main_loop_initialized);
  if (rc)
    {
      fprintf(stderr,"FAILED TO INITIALIZE MAINLOOP!\n");
      goto exit;
    }
  if (preprocess_into)
    {
      goto exit;
    }

  /* we are running as a non-root user from this point */

  app_post_daemonized();
  app_post_config_loaded();
  /* from now on internal messages are written to the system log as well */

  rc = main_loop_run();

  app_shutdown();
  z_mem_trace_dump();
exit:
  return rc;
}

int
main(int argc, char *argv[])
{
  GOptionContext *ctx;
  GError *error = NULL;

  z_mem_trace_init("syslog-ng.trace");

  install_dat_filename = get_reloc_string(PATH_INSTALL_DAT);

  ctx = g_option_context_new(NULL);
  msg_add_option_group(ctx);
  g_option_context_add_main_entries(ctx, syslogng_options, NULL);
  main_loop_add_options(ctx);
  setvbuf(stderr, NULL, _IONBF, 0);

  main_loop_initialized = CreateEvent(NULL, FALSE, FALSE, NULL);
  if (!g_option_context_parse(ctx, &argc, &argv, &error))
    {
      fprintf(stderr, "Error parsing command line arguments: %s\n", error ? error->message : "Invalid arguments");
      g_option_context_free(ctx);
      return 1;
    }
  g_option_context_free(ctx);
  if (argc > 1)
    {
      fprintf(stderr, "Excess number of arguments\n");
      return 1;
    }

  if (install)
    {
      return install_service(SERVER_SERVICE_NAME ,SERVER_SERVICE_DISPLAY_NAME, SERVER_SERVICE_DESCRIPTION, SERVICE_DEMAND_START, NULL, NULL, NULL, NULL, FALSE);
    }

  if (uninstall)
    {
      return uninstall_service(SERVER_SERVICE_NAME);
    }

  if (start)
    {
      return start_service_by_name(SERVER_SERVICE_NAME);
    }

  if (stop)
    {
      return stop_service_by_name(SERVER_SERVICE_NAME);
    }

  if (display_version)
    {
      version();
      return 0;
    }

  if (display_module_registry)
    {
      plugin_list_modules(stdout, TRUE);
      return 0;
    }

  if (debug_flag)
    {
      log_stderr = TRUE;
    }

  if (!(foreground))
    {
      /*Run syslog-ng as service*/
      start_service(SERVER_SERVICE_NAME,server_service_main,main_loop_terminate);
      return 0;
    }
  return server_service_main();
}
