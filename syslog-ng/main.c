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

#if ENABLE_SSL
#include <openssl/ssl.h>
#include <openssl/rand.h>
#endif


#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

#include <grp.h>

#if HAVE_GETOPT_H
#include <getopt.h>
#endif

#include <iv.h>
#include <iv_signal.h>

static gchar *install_dat_filename = PATH_INSTALL_DAT;
static gchar *installer_version = NULL;
static gboolean display_version = FALSE;
static gboolean display_module_registry = FALSE;

#ifdef YYDEBUG
extern int cfg_parser_debug;
#endif

static GOptionEntry syslogng_options[] = 
{
  { "version",           'V',         0, G_OPTION_ARG_NONE, &display_version, "Display version number (" PACKAGE " " VERSION ")", NULL },
  { "module-path",         0,         0, G_OPTION_ARG_STRING, &module_path, "Set the list of colon separated directories to search for modules, default=" MODULE_PATH, "<path>" },
  { "module-registry",     0,         0, G_OPTION_ARG_NONE, &display_module_registry, "Display module information", NULL },
  { "default-modules",     0,         0, G_OPTION_ARG_STRING, &default_modules, "Set the set of auto-loaded modules, default=" DEFAULT_MODULES, "<module-list>" },
  { "seed",              'S',         0, G_OPTION_ARG_NONE, &seed_rng, "Seed the RNG using ~/.rnd or $RANDFILE", NULL},
#ifdef YYDEBUG
  { "yydebug",           'y',         0, G_OPTION_ARG_NONE, &cfg_parser_debug, "Enable configuration parser debugging", NULL },
#endif
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
  printf(PACKAGE " " VERSION "\n"
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
         ON_OFF_STR(ENABLE_GPROF),
         ON_OFF_STR(ENABLE_MEMTRACE),
         ON_OFF_STR(ENABLE_IPV6),
         ON_OFF_STR(ENABLE_SPOOF_SOURCE),
         ON_OFF_STR(ENABLE_TCP_WRAPPER),
         ON_OFF_STR(ENABLE_LINUX_CAPS),
         ON_OFF_STR(ENABLE_PCRE));

}

#if ENABLE_LINUX_CAPS
#define BASE_CAPS "cap_net_bind_service,cap_net_broadcast,cap_net_raw," \
  "cap_dac_read_search,cap_dac_override,cap_chown,cap_fowner=p "

static void
setup_caps (void)
{
  static gchar *capsstr_syslog = BASE_CAPS "cap_syslog=ep";
  static gchar *capsstr_sys_admin = BASE_CAPS "cap_sys_admin=ep";

  /* Set up the minimal privilege we'll need
   *
   * NOTE: polling /proc/kmsg requires cap_sys_admin, otherwise it'll always
   * indicate readability. Enabling/disabling cap_sys_admin on every poll
   * invocation seems to be too expensive. So I enable it for now.
   */
  if (g_process_check_cap_syslog())
    g_process_set_caps(capsstr_syslog);
  else
    g_process_set_caps(capsstr_sys_admin);
}
#else

#define setup_caps()

#endif

int 
main(int argc, char *argv[])
{
  gint rc;
  GOptionContext *ctx;
  GError *error = NULL;

  z_mem_trace_init("syslog-ng.trace");

  g_process_set_argv_space(argc, (gchar **) argv);

  setup_caps();

  ctx = g_option_context_new("syslog-ng");
  g_process_add_option_group(ctx);
  msg_add_option_group(ctx);
  g_option_context_add_main_entries(ctx, syslogng_options, NULL);
  main_loop_add_options(ctx);
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

  if (syntax_only || debug_flag)
    {
      g_process_set_mode(G_PM_FOREGROUND);
    }
  g_process_set_name("syslog-ng");
  
  /* in this case we switch users early while retaining a limited set of
   * credentials in order to initialize/reinitialize the configuration.
   */
  g_process_start();
  rc = main_loop_init();
  
  if (rc)
    {
      g_process_startup_failed(rc, TRUE);
      return rc;
    }
  else
    {
      if (syntax_only)
        g_process_startup_failed(0, TRUE);
      else
        g_process_startup_ok();
    }

  /* we are running as a non-root user from this point */
  
  app_post_daemonized();
  app_post_config_loaded();
  /* from now on internal messages are written to the system log as well */
  
  rc = main_loop_run();

  app_shutdown();
  z_mem_trace_dump();
  g_process_finish();
  return rc;
}

