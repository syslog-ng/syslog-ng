/*
 * Copyright (c) 2002-2013 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2013 Balázs Scheidler
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
#include "mainloop-worker.h"
#include "mainloop-io-worker.h"
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

#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <time.h>
#include <iv.h>
#include <iv_signal.h>
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

/* NOTE: these should not be here, rather they should be either processed by
 * the main module and then passed as parameters or propagated to a place
 * closer to their usage.  The simple reason they are here, is that mainloop
 * needed them and it implements the command line options to parse them.
 * This is far from perfect. (Bazsi) */
static const gchar *cfgfilename;
static const gchar *persist_file;
static const gchar *ctlfilename;
const gchar *module_path;
static gchar *preprocess_into = NULL;
gboolean syntax_only = FALSE;

/*
 * This variable is used to detect that syslog-ng is being terminated, in which
 * case ongoing reload operations are aborted.
 *
 * The variable is deeply embedded in various mainloop callbacks to get out
 * of an ongoing reload and start doing the termination instead.  A better
 * solution would be to use a queue for intrusive, worker-stopping
 * operations and serialize such tasks so they won't interfere which each other.
 *
 * This interference is now implemented by conditionals scattered around the code.
 *
 * Example:
 *   * reload is now taking two steps (marked R in the figure below)
 *     1) parse the configuration and request worker threads to be stopped
 *     2) apply the configuration once all threads exited
 *   * termination is also taking two steps
 *     1) send out the shutting down message and start waiting 100msec
 *     2) terminate the mainloop
 *
 * The problem happens when reload and termination happen at around the same
 * time and these steps are interleaved.
 *
 *   Normal operation: RRTT (e.g. reload finishes, then termination)
 *   Problematic case: RTRT (e.g. reload starts, termination starts, config apply, terminate)
 *
 * In the problematic case, two independent operations do similar things to
 * the mainloop, and to prevent misfortune we need to handle this case explicitly.
 *
 * Were the two operations serialized by some kind of queue, the problems
 * would be gone.
 */
gboolean __main_loop_is_terminating = FALSE;

/* signal handling */
static struct iv_signal sighup_poll;
static struct iv_signal sigterm_poll;
static struct iv_signal sigint_poll;
static struct iv_signal sigchild_poll;

static struct iv_event exit_requested;
static struct iv_event reload_config_requested;


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
typedef struct _MainLoopTaskCallSite MainLoopTaskCallSite;
struct _MainLoopTaskCallSite
{
  struct iv_list_head list;
  MainLoopTaskFunc func;
  gpointer user_data;
  gpointer result;
  gboolean pending;
  gboolean wait;
  GCond *cond;
  GStaticMutex lock;
};

TLS_BLOCK_START
{
  MainLoopTaskCallSite call_info;
}
TLS_BLOCK_END;

static GStaticMutex main_task_lock = G_STATIC_MUTEX_INIT;
static struct iv_list_head main_task_queue = IV_LIST_HEAD_INIT(main_task_queue);
static struct iv_event main_task_posted;
ThreadId main_thread_handle;

#define call_info  __tls_deref(call_info)

gpointer
main_loop_call(MainLoopTaskFunc func, gpointer user_data, gboolean wait)
{
  if (main_loop_is_main_thread())
    return func(user_data);

  g_static_mutex_lock(&main_task_lock);

  /* check if a previous call is being executed */
  g_static_mutex_lock(&call_info.lock);
  if (call_info.pending)
    {
      /* yes, it is still running, indicate that we need to be woken up */
      call_info.wait = TRUE;
      g_static_mutex_unlock(&call_info.lock);

      while (call_info.pending)
        g_cond_wait(call_info.cond, g_static_mutex_get_mutex(&main_task_lock));
    }
  else
    {
      g_static_mutex_unlock(&call_info.lock);
    }

  /* call_info.lock is no longer needed, since we're the only ones using call_info now */
  INIT_IV_LIST_HEAD(&call_info.list);
  call_info.pending = TRUE;
  call_info.func = func;
  call_info.user_data = user_data;
  call_info.wait = wait;
  iv_list_add(&call_info.list, &main_task_queue);
  iv_event_post(&main_task_posted);
  if (wait)
    {
      while (call_info.pending)
        g_cond_wait(call_info.cond, g_static_mutex_get_mutex(&main_task_lock));
    }
  g_static_mutex_unlock(&main_task_lock);
  return call_info.result;
}

static void
main_loop_call_handler(gpointer user_data)
{
  g_static_mutex_lock(&main_task_lock);
  while (!iv_list_empty(&main_task_queue))
    {
      MainLoopTaskCallSite *site;
      gpointer result;

      site = iv_list_entry(main_task_queue.next, MainLoopTaskCallSite, list);
      iv_list_del_init(&site->list);
      g_static_mutex_unlock(&main_task_lock);

      result = site->func(site->user_data);

      g_static_mutex_lock(&site->lock);
      site->result = result;
      site->pending = FALSE;
      g_static_mutex_unlock(&site->lock);

      g_static_mutex_lock(&main_task_lock);
      if (site->wait)
        g_cond_signal(site->cond);
    }
  g_static_mutex_unlock(&main_task_lock);
}

void
main_loop_call_init(void)
{
  IV_EVENT_INIT(&main_task_posted);
  main_task_posted.cookie = NULL;
  main_task_posted.handler = main_loop_call_handler;
  iv_event_register(&main_task_posted);
}

void
main_loop_call_thread_init(void)
{
  call_info.cond = g_cond_new();
}

void
main_loop_call_thread_deinit(void)
{
  if (call_info.cond)
    g_cond_free(call_info.cond);
}

/************************************************************************************
 * config load/reload
 ************************************************************************************/

/* the old configuration that is being reloaded */
static GlobalConfig *main_loop_old_config;
/* the pending configuration we wish to switch to */
static GlobalConfig *main_loop_new_config;


/* called when syslog-ng first starts up */
static gboolean
main_loop_initialize_state(GlobalConfig *cfg, const gchar *persist_filename)
{
  gboolean success;

  cfg->state = persist_state_new(persist_filename);
  if (!persist_state_start(cfg->state))
    return FALSE;

  success = cfg_init(cfg);
  if (success)
    persist_state_commit(cfg->state);
  else
    persist_state_cancel(cfg->state);
  return success;
}

/* called to apply the new configuration once all I/O worker threads have finished */
static void
main_loop_reload_config_apply(void)
{
  if (main_loop_is_terminating())
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

  if (cfg_init(main_loop_new_config))
    {
      msg_verbose("New configuration initialized", NULL);
      persist_config_free(main_loop_new_config->persist);
      main_loop_new_config->persist = NULL;
      cfg_free(main_loop_old_config);
      current_configuration = main_loop_new_config;
      service_management_clear_status();
    }
  else
    {
      msg_error("Error initializing new configuration, reverting to old config", NULL);
      service_management_publish_status("Error initializing new configuration, using the old config");
      cfg_persist_config_move(main_loop_new_config, main_loop_old_config);
      if (!cfg_init(main_loop_old_config))
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
  msg_notice("Configuration reload request received, reloading configuration",
               NULL);

 finish:
  main_loop_new_config = NULL;
  main_loop_old_config = NULL;

  stats_cleanup_orphans();
  return;
}

/* initiate configuration reload */
void
main_loop_reload_config_initiate(void)
{
  if (!main_loop_is_terminating())
    return;

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

static void
main_loop_exit_initiate(void)
{
  if (main_loop_is_terminating())
    return;

  msg_notice("syslog-ng shutting down",
             evt_tag_str("version", VERSION),
             NULL);

  IV_TIMER_INIT(&main_loop_exit_timer);
  iv_validate_now();
  main_loop_exit_timer.expires = iv_now;
  main_loop_exit_timer.handler = main_loop_exit_timer_elapsed;
  timespec_add_msec(&main_loop_exit_timer.expires, 100);
  iv_timer_register(&main_loop_exit_timer);
  __main_loop_is_terminating = TRUE;
}


/************************************************************************************
 * signal handlers
 ************************************************************************************/

static void
sig_hup_handler(void *s)
{
  main_loop_reload_config_initiate();
}

static void
sig_term_handler(void *s)
{
  main_loop_exit_initiate();
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

static void
setup_signals(void)
{
  struct sigaction sa;

  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &sa, NULL);
  
  IV_SIGNAL_INIT(&sighup_poll);
  sighup_poll.signum = SIGHUP;
  sighup_poll.flags = IV_SIGNAL_FLAG_EXCLUSIVE;
  sighup_poll.cookie = NULL;
  sighup_poll.handler = sig_hup_handler;
  iv_signal_register(&sighup_poll);

  IV_SIGNAL_INIT(&sigchild_poll);
  sigchild_poll.signum = SIGCHLD;
  sigchild_poll.flags = IV_SIGNAL_FLAG_EXCLUSIVE;
  sigchild_poll.handler = sig_child_handler;
  iv_signal_register(&sigchild_poll);

  IV_SIGNAL_INIT(&sigterm_poll);
  sigterm_poll.signum = SIGTERM;
  sigterm_poll.flags = IV_SIGNAL_FLAG_EXCLUSIVE;
  sigterm_poll.handler = sig_term_handler;
  iv_signal_register(&sigterm_poll);

  IV_SIGNAL_INIT(&sigint_poll);
  sigint_poll.signum = SIGINT;
  sigint_poll.flags = IV_SIGNAL_FLAG_EXCLUSIVE;
  sigint_poll.handler = sig_term_handler;
  iv_signal_register(&sigint_poll);
}

/************************************************************************************
 * syslog-ng main loop
 ************************************************************************************/

static void
main_loop_init_events(void)
{
  IV_EVENT_INIT(&exit_requested);
  exit_requested.handler = (void (*)(void *)) main_loop_exit_initiate;
  iv_event_register(&exit_requested);

  IV_EVENT_INIT(&reload_config_requested);
  reload_config_requested.handler = (void (*)(void *)) main_loop_reload_config_initiate;
  iv_event_register(&reload_config_requested);
}

void
main_loop_exit(void)
{
  iv_event_post(&exit_requested);
  return;
}

void
main_loop_reload_config(void)
{
  iv_event_post(&reload_config_requested);
  return;
}

void
main_loop_init(void)
{
  service_management_publish_status("Starting up...");

  main_thread_handle = get_thread_id();
  main_loop_worker_init();
  main_loop_io_worker_init();
  main_loop_call_init();

  main_loop_init_events();
  control_init(ctlfilename);
  setup_signals();
}

/*
 * Returns: exit code to be returned to the calling process, 0 on success.
 */
int
main_loop_read_and_init_config(void)
{
  current_configuration = cfg_new(0);
  if (!cfg_read_config(current_configuration, cfgfilename, syntax_only, preprocess_into))
    {
      return 1;
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

static void
main_loop_free_config(void)
{
  cfg_free(current_configuration);
  current_configuration = NULL;
}

void
main_loop_deinit(void)
{
  main_loop_free_config();
  control_destroy();

  iv_event_unregister(&main_task_posted);
  iv_event_unregister(&exit_requested);
  iv_event_unregister(&reload_config_requested);
  main_loop_io_worker_deinit();
  main_loop_worker_deinit();
}

void
main_loop_run(void)
{
  msg_notice("syslog-ng starting up",
             evt_tag_str("version", VERSION),
             NULL);

  /* main loop */
  service_management_indicate_readiness();
  service_management_clear_status();
  iv_main();
  service_management_publish_status("Shutting down...");
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
main_loop_global_init(void)
{
  cfgfilename = get_installation_path_for(PATH_SYSLOG_NG_CONF);
  persist_file = get_installation_path_for(PATH_PERSIST_CONFIG);
  ctlfilename = get_installation_path_for(PATH_CONTROL_SOCKET);
  module_path = get_installation_path_for(MODULE_PATH);
}

