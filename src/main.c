/***************************************************************************
 *
 * Copyright (c) 2002 BalaBit IT Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id: main.c,v 1.16 2003/07/04 15:56:26 bazsi Exp $
 *
 ***************************************************************************/

#include "syslog-ng.h"
#include "cfg.h"
#include "messages.h"
#include "memtrace.h"
#include "children.h"
#include "memtrace.h"
#include "misc.h"
#include "stats.h"
#include "dnscache.h"

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <time.h>

#include <grp.h>

#if HAVE_GETOPT_H
#include <getopt.h>
#endif

static char cfgfilename[128] = PATH_SYSLOG_NG_CONF;
static char pidfilename[128] = PATH_PIDFILE;

static gboolean do_fork = 1;
static gboolean sig_hup_received = FALSE;
static gboolean sig_term_received = FALSE;
static gboolean sig_child_received = FALSE;
static gchar *chroot_dir = NULL;
static gchar *run_as_user = NULL;
static uid_t uid = 0;
static gid_t gid = 0;


void usage(void)
{
  printf("Usage: syslog-ng [options]\n"
	 "Accept and manage system log messages\n\n"
	 "Options:\n"
	 "  -s, --syntax-only                Only read and parse config file\n"
	 "  -d, --debug                      Turn on debugging messages\n"
	 "  -v, --verbose                    Be a bit more verbose\n"
	 "  -e, --stderr                     Log internal messages to stderr\n"
	 "  -F, --foreground                 Don't fork into background\n"
	 "  -f <fname>, --cfgfile=<fname>    Set config file name, default=" PATH_SYSLOG_NG_CONF "\n"
	 "  -V, --version                    Display version number (" PACKAGE " " VERSION ")\n"
	 "  -p <fname>, --pidfile=<fname>    Set pid file name, default=" PATH_PIDFILE "\n"
	 "  -C <dir>, --chroot=<dir>         Chroot to directory\n"
	 "  -u <user>, --user=<user>         Switch to user\n"
	 "  -g <group>, --group=<group>      Switch to group\n"
#ifdef YYDEBUG
	 "  -y, --yydebug                    Turn on yacc debug messages\n"
#endif
	 );

  exit(0);
}

static void 
sig_hup_handler(int signo)
{
  sig_hup_received = TRUE;
}

static void
sig_term_handler(int signo)
{
  sig_term_received = TRUE;
}

static void
sig_segv_handler(int signo)
{
  kill(getpid(), SIGSEGV);
}

static void
sig_child_handler(int signo)
{
  sig_child_received = TRUE;
}

static void
setup_signals(void)
{
  struct sigaction sa;
  
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &sa, NULL);
  sa.sa_handler = sig_hup_handler;
  sigaction(SIGHUP, &sa, NULL);
  sa.sa_handler = sig_term_handler;
  sigaction(SIGTERM, &sa, NULL);
  sa.sa_handler = sig_term_handler;
  sigaction(SIGINT, &sa, NULL);
  sa.sa_handler = sig_child_handler;
  sigaction(SIGCHLD, &sa, NULL);
  sa.sa_handler = sig_segv_handler;
  sa.sa_flags = SA_RESETHAND;
  sigaction(SIGSEGV, &sa, NULL);
}

gboolean
stats_timer(gpointer st)
{
  stats_generate_log();
  return TRUE;
}

int 
main_loop_run(GlobalConfig *cfg)
{
  GMainLoop *main_loop;
  gint iters;
  guint stats_timer_id = 0;
  
  msg_notice("syslog-ng starting up", 
             evt_tag_str("version", VERSION),
             NULL);
  main_loop = g_main_loop_new(NULL, TRUE);
  if (cfg->stats_freq > 0)
    stats_timer_id = g_timeout_add(cfg->stats_freq * 1000, stats_timer, NULL);
  while (g_main_loop_is_running(main_loop))
    {
      if (cfg->time_sleep > 0)
        {
          struct timespec ts;
          
          ts.tv_sec = cfg->time_sleep / 1000;
          ts.tv_nsec = (cfg->time_sleep % 1000) * 1E6;
          
          nanosleep(&ts, NULL);
        }
      g_main_context_iteration(g_main_loop_get_context(main_loop), TRUE);
      if (sig_hup_received)
        {
          msg_notice("SIGHUP received, reloading configuration", NULL);
          cfg = cfg_reload_config(cfgfilename, cfg);
          sig_hup_received = FALSE;
          if (cfg->stats_freq > 0)
            {
              if (stats_timer_id != 0)
                g_source_remove(stats_timer_id);
              stats_timer_id = g_timeout_add(cfg->stats_freq * 1000, stats_timer, NULL);
            }
          stats_cleanup_orphans();
        }
      if (sig_term_received)
        {
          msg_notice("SIGTERM received, terminating", NULL);
          sig_term_received = FALSE;
          break;
        }
      if (sig_child_received)
	{
	  pid_t pid;
	  int status;

          do
	    {
	      pid = waitpid(-1, &status, WNOHANG);
	      child_manager_sigchild(pid, status);
	    }
          while (pid > 0);
	  sig_child_received = FALSE;
	}
    }
    
  msg_notice("syslog-ng shutting down", 
             evt_tag_str("version", VERSION),
             NULL);
  iters = 0;
  while (g_main_context_iteration(NULL, FALSE) && iters < 3)
    {
      iters++;
    }
  cfg_deinit(cfg, NULL);
  cfg_free(cfg);
  g_main_loop_unref(main_loop);
  return 0;
}

gboolean
daemonize(void)
{
  pid_t pid;

  if (!do_fork)
    return 1;

  pid = fork();
  if (pid == 0) 
    {
      int fd;
      
      fd = open(pidfilename, O_CREAT | O_WRONLY | O_NOCTTY | O_TRUNC, 0600);
      if (fd != -1) 
	{
	  gchar buf[32];

	  g_snprintf(buf, sizeof(buf), "%d", (int) getpid());
	  write(fd, buf, strlen(buf));
	  close(fd);
	}
      return TRUE;
    }
  else if (pid == -1) 
    {
      msg_error("Error during fork",
                evt_tag_errno(EVT_TAG_OSERROR, errno),
                NULL);
      return FALSE;
    }
  exit(0);
}

void
setup_std_fds(gboolean log_to_stderr)
{
  int fd;

  if (do_fork)
    {
      fd = open("/dev/null", O_RDONLY);
      if (fd != -1)
	{
	  dup2(fd, STDIN_FILENO);
	  if (fd != STDIN_FILENO)
	    close(fd);
	}
      fd = open("/dev/null", O_WRONLY);
      if (fd != -1)
	{
	  dup2(fd, STDOUT_FILENO);
	  if (!log_to_stderr)
	    dup2(fd, STDERR_FILENO);
	  if (fd != STDOUT_FILENO && fd != STDERR_FILENO)
	    close(fd);
	}
      setsid();
    }
}

int
setup_creds(void)
{
  if (chroot_dir) 
    {
      if (chroot(chroot_dir) < 0) 
	{
	  msg_error("Error during chroot()",
	            evt_tag_errno(EVT_TAG_OSERROR, errno),
	            NULL);
	  return 0;
	}
    }

  if (uid || gid || run_as_user) 
    {
      setgid(gid);
      initgroups(run_as_user, gid);
      setuid(uid);
    }
  return 1;
}

#ifdef YYDEBUG
extern int yydebug;
#endif

int 
main(int argc, char *argv[])
{
  GlobalConfig *cfg;

#if HAVE_GETOPT_LONG
  struct option syslog_ng_options[] = 
    {
      { "cfgfile", required_argument, NULL, 'f' },
      { "pidfile", required_argument, NULL, 'p' },
      { "debug", no_argument, NULL, 'd' },
      { "syntax-only", no_argument, NULL, 's' },
      { "verbose", no_argument, NULL, 'v' },
      { "foreground", no_argument, NULL, 'F' },
      { "help", no_argument, NULL, 'h' },
      { "version", no_argument, NULL, 'V' },
      { "chroot", required_argument, NULL, 'C' },
      { "user", required_argument, NULL, 'u' },
      { "group", required_argument, NULL, 'g' },
      { "stderr", no_argument, NULL, 'e' },
#ifdef YYDEBUG
      { "yydebug", no_argument, NULL, 'y' },
#endif
      { NULL, 0, NULL, 0 }
    };
#endif
  int syntax_only = 0;
  int log_to_stderr = 0;
  int opt, rc;

#if HAVE_GETOPT_LONG
  while ((opt = getopt_long(argc, argv, "sFf:p:dvhyVC:u:g:e", syslog_ng_options, NULL)) != -1)
#else
  while ((opt = getopt(argc, argv, "sFf:p:dvhyVC:u:g:e")) != -1)
#endif
    {
      switch (opt) 
	{
	case 'f':
	  strncpy(cfgfilename, optarg, sizeof(cfgfilename));
	  break;
	case 'p':
	  strncpy(pidfilename, optarg, sizeof(pidfilename));
	  break;
	case 's':
	  syntax_only = 1;
	  break;
	case 'd':
	  debug_flag++;
	  break;
	case 'v':
	  verbose_flag++;
	  break;
	case 'e':
	  log_to_stderr = 1;
	  break;
	case 'F':
	  do_fork = 0;
	  break;
	case 'V':
	  printf(PACKAGE " " VERSION "\n");
	  return 0;
	case 'C':
	  chroot_dir = optarg;
	  break;
	case 'u':
	  if (!resolve_user(optarg, &uid))
	    usage();
	  run_as_user = optarg;
	  break;
	case 'g':
	  if (!resolve_group(optarg, &gid))
	    usage();
	  break;
#ifdef YYDEBUG
	case 'y':
	  yydebug = 1;
	  break;
#endif
	case 'h':
	default:
	  usage();
	  break;
	}
      
    }
  
  
  z_mem_trace_init("syslog-ng.trace");
  tzset();
  setup_signals();
  msg_init(log_to_stderr);
  child_manager_init();
  dns_cache_init();

  cfg = cfg_new(cfgfilename);
  if (!cfg)
    {
      return 1;
    }

  if (syntax_only)
    {
      return 0;
    }


  if (!cfg_init(cfg, NULL))
    {
      return 2;
    }

  if (!daemonize())
    {
      return 2;
    }
  /* from now on internal messages are written to the system log as well */
  msg_syslog_started();
  
  setup_creds();
  setup_std_fds(log_to_stderr);

  rc = main_loop_run(cfg);
  child_manager_deinit();
  msg_deinit();
  dns_cache_destroy();
  z_mem_trace_dump();
  return rc;
}

