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
#include "mainloop.h"
#include "mainloop-call.h"
#include "mainloop-io-worker.h"
#include "mainloop-worker.h"
#include "apphook.h"
#include "cfg.h"
#include "stats.h"
#include "messages.h"
#include "children.h"
#include "misc.h"
#include "hostname.h"
#include "control.h"
#include "logqueue.h"
#include "dnscache.h"
#include "tls-support.h"
#include "scratch-buffers.h"
#include "reloc.h"
#include "service-management.h"
#include "compat.h"
#include "sgroup.h"
#include "driver.h"
#include <openssl/rand.h>

#include <sys/types.h>

#ifndef _WIN32
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <iv_signal.h>
#else
#include <signal.h>
#endif

#include <string.h>
#include <iv.h>
#include <iv_work.h>
#include <iv_event.h>

/**
 * Processing model
 * ================
 *
 * This comment documents how the work performed by syslog-ng is
 * partitioned between threads. First of all it is useful to know that
 * the configuration is translated to a tree of LogPipe instances, as
 * described in a comment in logpipe.h.
 *
 * The basic assumptions for threading:
 *   - configuration file is parsed in the main thread
 *   - the log pipe tree is built in the main thread
 *   - processing of messages is stalled while the
 *     configuration is reloaded
 *   - the _queue() operation for LogPipe instances can happen in
 *     multiple threads
 *   - notifications accross LogPipe instances happen in the main thread
 *
 * This boils down to:
 * ===================
 *   - If not otherwise specified LogPipe derived classes can only be
 *     instantiated (e.g. new()) or initialized/deinitialized (_init/deinit)
 *     in the main thread. Exceptions to this rule are documented below.
 *   - All queue operations finish before either deinit is called.
 *   - Locking is only needed between multiple invocations of _queue()
 *     in parallel threads, and any other main thread activity.
 *
 * Threading model
 * ===============
 *   - the main thread manages the configuration and polls for I/O
 *     using epoll
 *   - whenever an I/O event happens, work may be delegated to worker
 *     threads. Currently only the LogReader/LogWriter classes make use
 *     of worker threads, everything else remains in the main thread
 *     (internal messages, incoming connections, etc).
 *   - _all_ I/O polling must be registered in the main thread (update_watches
 *     and friends)
 *
 */

/* parsed command line arguments */
gchar *preprocess_into = NULL;
gboolean syntax_only = FALSE;
gboolean generate_persist_file = FALSE;
guint32 g_run_id;
guint32 g_hostid;

/* USED ONLY IN PREMIUM EDITION */
gboolean server_mode = TRUE;

gboolean __main_loop_is_terminating = FALSE;

/* signal handling */
static struct iv_signal sighup_poll;
static struct iv_signal sigterm_poll;
static struct iv_signal sigint_poll;
static struct iv_signal sigchild_poll;

static struct iv_event stop_signal;
static struct iv_event reload_signal;


/* Currently running configuration, should not be used outside the mainloop
 * logic. If anything needs access to the GlobalConfig instance at runtime,
 * it needs to save that during initialization.  If anything needs the
 * config being parsed (e.g.  in the bison generated code), it should
 * consult the value of "configuration", which is NULL after the parsing is
 * finished.
 */
static GlobalConfig *current_configuration;

/************************************************************************************
 * Cross-thread function calls into the main loop
 ************************************************************************************/

typedef void (*Quit_Func)(gpointer user_data);
typedef struct _Quit_Callback_Func Quit_Callback_Func;
struct _Quit_Callback_Func
{
  Quit_Func func;
  gpointer user_data;
};

GList *quit_callback_list = NULL;

ThreadId main_thread_handle;

static void sig_term_handler(void *s);
static void sig_hup_handler(void *s);

void
main_loop_stop_signal_init(void)
{
  IV_EVENT_INIT(&stop_signal);
  stop_signal.handler = sig_term_handler;
  iv_event_register(&stop_signal);
}

void
main_loop_reload_signal_init(void)
{
  IV_EVENT_INIT(&reload_signal);
  reload_signal.handler = sig_hup_handler;
  iv_event_register(&reload_signal);
}


/************************************************************************************
 * stats timer
 ************************************************************************************/

/* stats timer */
static struct iv_timer stats_timer;

static void
stats_timer_rearm(gint stats_freq)
{
  stats_timer.cookie = GINT_TO_POINTER(stats_freq);
  if (stats_freq > 0)
    {
      /* arm the timer */
      iv_validate_now();
      stats_timer.expires = iv_now;
      timespec_add_msec(&stats_timer.expires, stats_freq * 1000);
      iv_timer_register(&stats_timer);
    }
}

static void
stats_timer_elapsed(gpointer st)
{
  gint stats_freq = GPOINTER_TO_INT(st);

  stats_generate_log();
  stats_timer_rearm(stats_freq);
}

static void
stats_timer_kickoff(GlobalConfig *cfg)
{
  if (iv_timer_registered(&stats_timer))
    iv_timer_unregister(&stats_timer);

  stats_timer_rearm(cfg->stats_freq);
}

void
stats_timer_init(GlobalConfig *cfg)
{
  IV_TIMER_INIT(&stats_timer);
  stats_timer.handler = stats_timer_elapsed;

  stats_timer_kickoff(cfg);
};

/************************************************************************************
 * config load/reload
 ************************************************************************************/

/* the old configuration that is being reloaded */
GlobalConfig *main_loop_old_config;
/* the pending configuration we wish to switch to */
GlobalConfig *main_loop_new_config;

static guint32
main_loop_inc_and_set_run_id(guint32 run_id)
{
  g_run_id = run_id;
  return ++g_run_id;
}

static guint32
create_hostid()
{
  union {
    unsigned char _raw[sizeof(guint32)];
    guint32 id;
  } hostid;

  RAND_bytes(hostid._raw, sizeof(hostid._raw));

  return hostid.id;
}

static void
main_loop_init_run_id(PersistState *persist_state)
{
  gsize size;
  guint8 version;
  PersistEntryHandle handle;
  guint32 *run_id;

  handle = persist_state_lookup_entry(persist_state, "run_id", &size, &version);
  if (handle)
  {
    run_id = persist_state_map_entry(persist_state,handle);
    *run_id = main_loop_inc_and_set_run_id(*run_id);
    persist_state_unmap_entry(persist_state,handle);
  }
  else
  {
    handle = persist_state_alloc_entry(persist_state, "run_id", sizeof(guint));
    run_id = persist_state_map_entry(persist_state,handle);
    *run_id = main_loop_inc_and_set_run_id(0);
    persist_state_unmap_entry(persist_state,handle);
  }
}

static void
main_loop_init_hostid(PersistState *persist_state)
{
  gsize size;
  guint8 version;
  PersistEntryHandle handle;
  guint32 *hostid;

  handle = persist_state_lookup_entry(persist_state, "hostid", &size, &version);
  if (handle)
  {
    hostid = persist_state_map_entry(persist_state,handle);
    g_hostid = *hostid;
    persist_state_unmap_entry(persist_state,handle);
  }
  else
  {
    g_hostid = create_hostid();
    handle = persist_state_alloc_entry(persist_state, "hostid", sizeof(guint32));
    hostid = persist_state_map_entry(persist_state,handle);
    *hostid = g_hostid;
    persist_state_unmap_entry(persist_state,handle);
  }
}

/* called when syslog-ng first starts up */

gboolean
main_loop_initialize_state(GlobalConfig *cfg, const gchar *persist_filename)
{
  gboolean success;

  cfg->state = persist_state_new(persist_filename);

  if (!persist_state_start(cfg->state))
    return FALSE;

  cfg_generate_persist_file(cfg);

  main_loop_init_run_id(cfg->state);
  main_loop_init_hostid(cfg->state);
  if (cfg->use_uniqid)
    {
      log_msg_init_rcptid(cfg->state);
    }

  success = cfg_init(cfg);
  if (success)
    persist_state_commit(cfg->state);
  else
    persist_state_cancel(cfg->state);
  return success;
}

/* called to apply the new configuration once all I/O worker threads have finished */
void
main_loop_reload_config_apply(void)
{
  if (__main_loop_is_terminating)
    {
      if (main_loop_new_config)
        {
          cfg_free(main_loop_new_config);
          main_loop_new_config = NULL;
        }
      return;
    }

  main_loop_old_config->persist = persist_config_new();
  cfg_deinit(main_loop_old_config);
  cfg_persist_config_move(main_loop_old_config, main_loop_new_config);
  cfg_generate_persist_file(main_loop_new_config);

  if (cfg_init(main_loop_new_config))
    {
      msg_verbose("New configuration initialized", NULL);
      if (strcmp(main_loop_new_config->cfg_fingerprint, main_loop_old_config->cfg_fingerprint)!=0)
        {
          main_loop_new_config->stats_reset = TRUE;
        }
      cfg_free(main_loop_old_config);
      persist_config_free(main_loop_new_config->persist);
      main_loop_new_config->persist = NULL;
      current_configuration = main_loop_new_config;
      service_management_clear_status();

      if (main_loop_old_config->use_uniqid && !current_configuration->use_uniqid)
        {
          log_msg_deinit_rcptid();
        }
      else if (!main_loop_old_config->use_uniqid && current_configuration->use_uniqid)
        {
          log_msg_init_rcptid(current_configuration->state);
        }
    }
  else
    {
      msg_error("Error initializing new configuration, reverting to old config", NULL);
      service_management_publish_status("Error initializing new configuration, using the old config");
      cfg_deinit(main_loop_new_config);
      cfg_persist_config_move(main_loop_new_config, main_loop_old_config);
      if (!cfg_reinit(main_loop_old_config))
        {
          /* hmm. hmmm, error reinitializing old configuration, we're hosed.
           * Best is to kill ourselves in the hope that the supervisor
           * restarts us.
           */
          kill(getpid(), SIGQUIT);
          g_assert_not_reached();
        }
      persist_config_free(main_loop_old_config->persist);
      main_loop_old_config->persist = NULL;
      cfg_free(main_loop_new_config);
      current_configuration = main_loop_old_config;
      goto finish;
    }
  /* this is already running with the new config in place */
  res_init();
  app_post_config_loaded();

  show_config_reload_message(current_configuration);
 finish:
  main_loop_new_config = NULL;
  main_loop_old_config = NULL;

  stats_timer_kickoff(current_configuration);
  if (current_configuration->stats_reset)
    {
      stats_cleanup_orphans();
      current_configuration->stats_reset = FALSE;
    }
  return;
}

/* initiate configuration reload */
static void
main_loop_reload_config_initiate(void)
{
  service_management_publish_status("Reloading configuration");
  if (main_loop_new_config)
    {
      /* This block is entered only if this function is reentered before
       * main_loop_reload_config_apply() would be called.  In that case we
       * drop the previously parsed configuration and start over again to
       * ensure that the contents of the running configuration matches the
       * contents of the file at the time the SIGHUP signal was received.
       */
      cfg_free(main_loop_new_config);
      main_loop_new_config = NULL;
    }
  main_loop_old_config = current_configuration;
  app_pre_config_loaded();
  main_loop_new_config = cfg_new(0);
  if (!cfg_read_config(main_loop_new_config, cfgfilename, FALSE, NULL))
    {
      cfg_free(main_loop_new_config);
      main_loop_new_config = NULL;
      main_loop_old_config = NULL;
      msg_error("Error parsing configuration",
                evt_tag_str(EVT_TAG_FILENAME, cfgfilename),
                NULL);
      service_management_publish_status("Error parsing new configuration, using the old config");
      return;
    }
  main_loop_worker_sync_call(main_loop_reload_config_apply);
}

/************************************************************************************
 * syncronized exit
 ************************************************************************************/

static struct iv_timer main_loop_exit_timer;

static void
main_loop_exit_finish(void)
{
  /* deinit the current configuration, as at this point we _know_ that no
   * threads are running.  This will unregister ivykis tasks and timers
   * that could fire while the configuration is being destructed */
  cfg_deinit(current_configuration);
  iv_quit();
}

static void
main_loop_exit_timer_elapsed(void *arg)
{
  main_loop_worker_sync_call(main_loop_exit_finish);
}

void
main_loop_add_quit_callback_list_element(gpointer func, gpointer user_data)
{
  Quit_Callback_Func *cfunc = g_new(Quit_Callback_Func,1);

  cfunc->func = func;
  cfunc->user_data = user_data;

  quit_callback_list = g_list_append(quit_callback_list, cfunc);

  return;
}


static void
main_loop_call_quit_callback_list_element(Quit_Callback_Func *func)
{

  func->func(func->user_data);

  return;
}

static void
main_loop_call_thread_quit_callback()
{

  g_list_foreach(quit_callback_list,(GFunc) main_loop_call_quit_callback_list_element,NULL);
  g_list_foreach(quit_callback_list,(GFunc) g_free ,NULL);
  g_list_free(quit_callback_list);
  quit_callback_list = NULL;

  return;
}

/************************************************************************************
 * signal handlers
 ************************************************************************************/

static void
sig_hup_handler(void *s)
{
  if (!__main_loop_is_terminating)
    {
      main_loop_call_thread_quit_callback();
      main_loop_reload_config_initiate();
    }
}

static void
sig_term_handler(void *s)
{
  if (__main_loop_is_terminating)
    return;

  show_config_shutdown_message(current_configuration);

  main_loop_call_thread_quit_callback();

  IV_TIMER_INIT(&main_loop_exit_timer);
  iv_validate_now();
  main_loop_exit_timer.expires = iv_now;
  main_loop_exit_timer.handler = main_loop_exit_timer_elapsed;
  timespec_add_msec(&main_loop_exit_timer.expires, 100);
  iv_timer_register(&main_loop_exit_timer);
  __main_loop_is_terminating = TRUE;
}

static void
sig_child_handler(void *s)
{
  pid_t pid;
  int status;

  /* this may handle multiple SIGCHLD signals, however it doesn't
   * matter if one or multiple SIGCHLD was received assuming that
   * all exited child process are waited for */

  do
    {
      pid = waitpid(-1, &status, WNOHANG);
      child_manager_sigchild(pid, status);
    }
  while (pid > 0);
}

/************************************************************************************
 * syslog-ng main loop
 ************************************************************************************/
#ifdef _WIN32
void
term_signal(int signal)
{
  main_loop_terminate();
}
#endif

static void
_ignore_signal(gint signum)
{
  struct sigaction sa;

  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = SIG_IGN;
  sigaction(signum, &sa, NULL);
}

static void
_register_signal_handler(struct iv_signal *signal_poll, gint signum, void (*handler)(void *))
{
  IV_SIGNAL_INIT(signal_poll);
  signal_poll->signum = signum;
  signal_poll->flags = IV_SIGNAL_FLAG_EXCLUSIVE;
  signal_poll->cookie = NULL;
  signal_poll->handler = handler;
  iv_signal_register(signal_poll);
}

static void
setup_signals(void)
{
  _ignore_signal(SIGPIPE);
  _register_signal_handler(&sighup_poll, SIGHUP, sig_hup_handler);
  _register_signal_handler(&sigchild_poll, SIGCHLD, sig_child_handler);
  _register_signal_handler(&sigterm_poll, SIGTERM, sig_term_handler);
  _register_signal_handler(&sigint_poll, SIGINT, sig_term_handler);
}

/*
 * Returns: exit code to be returned to the calling process.
 */
int
main_loop_init(gchar *config_string)
{
  app_startup();
  service_management_publish_status("Starting up...");

  init_signals();
#ifdef _WIN32
  signal(SIGTERM,term_signal);
  signal(SIGINT,term_signal);
#endif
  main_loop_worker_init();
  main_loop_io_worker_init();
  main_loop_stop_signal_init();
  main_loop_reload_signal_init();
  main_loop_call_init();

  current_configuration = cfg_new(0);
  if (cfgfilename)
    {
      if (!cfg_read_config(current_configuration, cfgfilename, syntax_only, preprocess_into))
        {
          return 1;
        }
    }
  else if (config_string)
    {
      if (!cfg_load_config(current_configuration, config_string, syntax_only, preprocess_into))
        {
          return 1;
        }
    }
  else
    {
      return 0;
    }

  if (syntax_only || preprocess_into)
    {
      return 0;
    }
  if (!main_loop_initialize_state(current_configuration, persist_file))
    {
      return 2;
    }
  return 0;
}

void
main_loop_deinit(void)
{
  control_destroy();
  cfg_free(current_configuration);

  current_configuration = NULL;

  main_loop_call_deinit();
  iv_event_unregister(&stop_signal);
  iv_event_unregister(&reload_signal);
}

int
main_loop_run(void)
{
  show_config_startup_message(current_configuration);

  setup_signals();
  control_init(ctlfilename);

  stats_timer_init(current_configuration);

  /* main loop */
  service_management_indicate_readiness();
  service_management_clear_status();
  iv_main();
  service_management_publish_status("Shutting down...");


  main_loop_deinit();
  return 0;
}

static GOptionEntry main_loop_options[] =
{
  { "cfgfile",           'f',         0, G_OPTION_ARG_STRING, &cfgfilename, "Set config file name, default=" PATH_SYSLOG_NG_CONF, "<config>" },
  { "persist-file",      'R',         0, G_OPTION_ARG_STRING, &persist_file, "Set the name of the persistent configuration file, default=" PATH_PERSIST_CONFIG, "<fname>" },
  { "preprocess-into",     0,         0, G_OPTION_ARG_STRING, &preprocess_into, "Write the preprocessed configuration file to the file specified", "output" },
  { "syntax-only",       's',         0, G_OPTION_ARG_NONE, &syntax_only, "Only read and parse config file", NULL},
  { "control",           'c',         0, G_OPTION_ARG_STRING, &ctlfilename, "Set syslog-ng control socket, default=" PATH_CONTROL_SOCKET, "<ctlpath>" },
  { NULL },
};

void
main_loop_add_options(GOptionContext *ctx)
{
  g_option_context_add_main_entries(ctx, main_loop_options, NULL);
  main_loop_io_worker_add_options(ctx);
}


void
main_loop_terminate()
{
  iv_event_post(&stop_signal);
  return;
}

void
main_loop_reload()
{
  iv_event_post(&reload_signal);
  return;
}
