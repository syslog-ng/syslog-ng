/*
 * Copyright (c) 2002-2013 Balabit
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
#include "mainloop-call.h"
#include "apphook.h"
#include "cfg.h"
#include "stats/stats-registry.h"
#include "messages.h"
#include "children.h"
#include "control/control-main.h"
#include "reloc.h"
#include "service-management.h"
#include "persist-state.h"
#include "run-id.h"
#include "host-id.h"
#include "debugger/debugger-main.h"
#include "plugin.h"
#include "resolved-configurable-paths.h"
#include "scratch-buffers.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <iv.h>
#include <iv_signal.h>
#include <iv_event.h>

volatile gint main_loop_workers_running;

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
 *   - notifications across LogPipe instances happen in the main thread
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

ThreadId main_thread_handle;
GCond *thread_halt_cond;
GStaticMutex workers_running_lock = G_STATIC_MUTEX_INIT;

struct _MainLoop
{
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
  gboolean _is_terminating;

  /* signal handling */
  struct iv_signal sighup_poll;
  struct iv_signal sigterm_poll;
  struct iv_signal sigint_poll;
  struct iv_signal sigchild_poll;
  struct iv_signal sigusr1_poll;

  struct iv_event exit_requested;
  struct iv_event reload_config_requested;

  struct iv_timer exit_timer;

  /* Currently running configuration, should not be used outside the mainloop
   * logic. If anything needs access to the GlobalConfig instance at runtime,
   * it needs to save that during initialization.  If anything needs the
   * config being parsed (e.g.  in the bison generated code), it should
   * consult the value of "configuration", which is NULL after the parsing is
   * finished.
   */
  GlobalConfig *current_configuration;

  /* the old configuration that is being reloaded */
  GlobalConfig *old_config;
  /* the pending configuration we wish to switch to */
  GlobalConfig *new_config;

  MainLoopOptions *options;
  ControlServer *control_server;
};

static MainLoop main_loop;


MainLoop *
main_loop_get_instance(void)
{
  return &main_loop;
}

void
main_loop_set_server_mode(MainLoop *self, gboolean server_mode)
{
  self->options->server_mode = server_mode;
}

gboolean
main_loop_is_server_mode(MainLoop *self)
{
  return self->options->server_mode;
}

/* called when syslog-ng first starts up */
gboolean
main_loop_initialize_state(GlobalConfig *cfg, const gchar *persist_filename)
{
  gboolean success;

  cfg->state = persist_state_new(persist_filename);
  persist_state_set_global_error_handler(cfg->state, (gpointer)main_loop_exit, (gpointer)main_loop_get_instance());

  if (!persist_state_start(cfg->state))
    return FALSE;
  if (!run_id_init(cfg->state))
    return FALSE;
  if (!host_id_init(cfg->state))
    return FALSE;

  success = cfg_init(cfg);

  if (success)
    persist_state_commit(cfg->state);
  else
    persist_state_cancel(cfg->state);
  return success;
}

static inline gboolean
main_loop_is_terminating(MainLoop *self)
{
  return self->_is_terminating;
}

/* called to apply the new configuration once all I/O worker threads have finished */
static void
main_loop_reload_config_apply(gpointer user_data)
{
  MainLoop *self = (MainLoop *) user_data;

  if (main_loop_is_terminating(self))
    {
      if (self->new_config)
        {
          cfg_free(self->new_config);
          self->new_config = NULL;
        }
      is_reloading_scheduled = FALSE;
      return;
    }
  self->old_config->persist = persist_config_new();
  cfg_deinit(self->old_config);
  cfg_persist_config_move(self->old_config, self->new_config);

  if (cfg_init(self->new_config))
    {
      msg_verbose("New configuration initialized");
      persist_config_free(self->new_config->persist);
      self->new_config->persist = NULL;
      cfg_free(self->old_config);
      self->current_configuration = self->new_config;
      service_management_clear_status();
    }
  else
    {
      msg_error("Error initializing new configuration, reverting to old config");
      service_management_publish_status("Error initializing new configuration, using the old config");
      cfg_persist_config_move(self->new_config, self->old_config);
      cfg_deinit(self->new_config);
      if (!cfg_init(self->old_config))
        {
          /* hmm. hmmm, error reinitializing old configuration, we're hosed.
           * Best is to kill ourselves in the hope that the supervisor
           * restarts us.
           */
          kill(getpid(), SIGQUIT);
          g_assert_not_reached();
        }
      persist_config_free(self->old_config->persist);
      self->old_config->persist = NULL;
      cfg_free(self->new_config);
      self->current_configuration = self->old_config;
      goto finish;
    }

  /* this is already running with the new config in place */
  app_config_changed();
  msg_notice("Configuration reload request received, reloading configuration");

finish:
  self->new_config = NULL;
  self->old_config = NULL;

  return;
}


/* initiate configuration reload */
void
main_loop_reload_config_initiate(gpointer user_data)
{
  MainLoop *self = (MainLoop *) user_data;

  if (main_loop_is_terminating(self))
    return;
  if (is_reloading_scheduled)
    {
      msg_notice("Error initiating reload, reload is already ongoing");
      return;
    }

  service_management_publish_status("Reloading configuration");

  self->old_config = self->current_configuration;
  self->new_config = cfg_new(0);
  if (!cfg_read_config(self->new_config, resolvedConfigurablePaths.cfgfilename, FALSE, NULL))
    {
      cfg_free(self->new_config);
      self->new_config = NULL;
      self->old_config = NULL;
      msg_error("Error parsing configuration",
                evt_tag_str(EVT_TAG_FILENAME, resolvedConfigurablePaths.cfgfilename));
      service_management_publish_status("Error parsing new configuration, using the old config");
      return;
    }

  is_reloading_scheduled = TRUE;
  main_loop_worker_sync_call(main_loop_reload_config_apply, self);
}

static void
block_till_workers_exit(void)
{
  GTimeVal end_time;

  g_get_current_time(&end_time);
  g_time_val_add(&end_time, 15 * G_USEC_PER_SEC);

  g_static_mutex_lock(&workers_running_lock);
  while (main_loop_workers_running)
    {
      if (!g_cond_timed_wait(thread_halt_cond, g_static_mutex_get_mutex(&workers_running_lock), &end_time))
        {
          /* timeout has passed. */
          fprintf(stderr, "Main thread timed out (15s) while waiting workers threads to exit. "
                  "workers_running: %d. Continuing ...\n", main_loop_workers_running);
          break;
        }
    }

  g_static_mutex_unlock(&workers_running_lock);
}

GlobalConfig *
main_loop_get_current_config(MainLoop *self)
{
  return self->current_configuration;
}

/* main_loop_verify_config
 * compares active configuration versus config file */

void
main_loop_verify_config(GString *result, MainLoop *self)
{
  const gchar *file_path = resolvedConfigurablePaths.cfgfilename;
  gchar *config_mem = self -> current_configuration -> original_config -> str;
  GError *err = NULL;
  gchar *file_contents;

  if (!g_file_get_contents(file_path, &file_contents, NULL, &err))
    {
      g_string_assign(result, "Cannot read configuration file: ");
      g_string_append(result, err -> message);
      g_error_free(err);
      err = NULL;
      return;
    }

  if (strcmp(file_contents, config_mem) == 0)
    g_string_assign(result, "Configuration file matches active configuration");
  else
    g_string_assign(result, "Configuration file does not match active configuration");

  g_free(file_contents);
}

/************************************************************************************
 * syncronized exit
 ************************************************************************************/

static void
main_loop_exit_finish(gpointer user_data)
{
  MainLoop *self = (MainLoop *) user_data;

  /* deinit the current configuration, as at this point we _know_ that no
   * threads are running.  This will unregister ivykis tasks and timers
   * that could fire while the configuration is being destructed */
  cfg_deinit(self->current_configuration);
  iv_quit();
}

static void
main_loop_exit_timer_elapsed(gpointer user_data)
{
  MainLoop *self = (MainLoop *) user_data;

  main_loop_worker_sync_call(main_loop_exit_finish, self);
}

static void
main_loop_exit_initiate(gpointer user_data)
{
  MainLoop *self = (MainLoop *) user_data;

  if (main_loop_is_terminating(self))
    return;

  app_pre_shutdown();

  msg_notice("syslog-ng shutting down",
             evt_tag_str("version", SYSLOG_NG_VERSION));

  IV_TIMER_INIT(&self->exit_timer);
  iv_validate_now();
  self->exit_timer.expires = iv_now;
  self->exit_timer.handler = main_loop_exit_timer_elapsed;
  self->exit_timer.cookie = self;
  timespec_add_msec(&self->exit_timer.expires, 100);
  iv_timer_register(&self->exit_timer);
  self->_is_terminating = TRUE;
}


/************************************************************************************
 * signal handlers
 ************************************************************************************/

static void
sig_hup_handler(gpointer user_data)
{
  MainLoop *self = (MainLoop *) user_data;

  main_loop_reload_config_initiate(self);
}

static void
sig_term_handler(gpointer user_data)
{
  MainLoop *self = (MainLoop *) user_data;

  main_loop_exit_initiate(self);
}

static void
sig_child_handler(gpointer user_data)
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
sig_usr1_handler(gpointer user_data)
{
  app_reopen_files();
}

static void
_ignore_signal(gint signum)
{
  struct sigaction sa;

  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = SIG_IGN;
  sigaction(signum, &sa, NULL);
}

static void
_register_signal_handler(struct iv_signal *signal_poll, gint signum, void (*handler)(void *), gpointer user_data)
{
  IV_SIGNAL_INIT(signal_poll);
  signal_poll->signum = signum;
  signal_poll->flags = IV_SIGNAL_FLAG_EXCLUSIVE;
  signal_poll->cookie = user_data;
  signal_poll->handler = handler;
  iv_signal_register(signal_poll);
}

static void
setup_signals(MainLoop *self)
{
  _ignore_signal(SIGPIPE);
  _register_signal_handler(&self->sighup_poll, SIGHUP, sig_hup_handler, self);
  _register_signal_handler(&self->sigchild_poll, SIGCHLD, sig_child_handler, self);
  _register_signal_handler(&self->sigterm_poll, SIGTERM, sig_term_handler, self);
  _register_signal_handler(&self->sigint_poll, SIGINT, sig_term_handler, self);
  _register_signal_handler(&self->sigusr1_poll, SIGUSR1, sig_usr1_handler, self);
}

/************************************************************************************
 * syslog-ng main loop
 ************************************************************************************/

static void
_register_event(struct iv_event *event, void (*handler)(void *), gpointer user_data)
{
  IV_EVENT_INIT(event);
  event->handler = handler;
  event->cookie = user_data;
  iv_event_register(event);
}

static void
main_loop_init_events(MainLoop *self)
{
  _register_event(&self->exit_requested, main_loop_exit_initiate, self);
  _register_event(&self->reload_config_requested, main_loop_reload_config_initiate, self);
}

void
main_loop_exit(MainLoop *self)
{
  iv_event_post(&self->exit_requested);
  return;
}

void
main_loop_reload_config(MainLoop *self)
{
  iv_event_post(&self->reload_config_requested);
  return;
}


void
main_loop_init(MainLoop *self, MainLoopOptions *options)
{
  service_management_publish_status("Starting up...");

  self->options = options;
  scratch_buffers_automatic_gc_init();
  main_loop_worker_init();
  main_loop_io_worker_init();
  main_loop_call_init();

  main_loop_init_events(self);
  setup_signals(self);

  self->current_configuration = cfg_new(0);
}

/*
 * Returns: exit code to be returned to the calling process, 0 on success.
 */
int
main_loop_read_and_init_config(MainLoop *self)
{
  MainLoopOptions *options = self->options;

  if (!cfg_read_config(self->current_configuration, resolvedConfigurablePaths.cfgfilename, options->syntax_only,
                       options->preprocess_into))
    {
      return 1;
    }

  if (options->syntax_only || options->preprocess_into)
    {
      return 0;
    }

  if (!main_loop_initialize_state(self->current_configuration, resolvedConfigurablePaths.persist_file))
    {
      return 2;
    }
  self->control_server = control_init(self, resolvedConfigurablePaths.ctlfilename);
  return 0;
}

static void
main_loop_free_config(MainLoop *self)
{
  cfg_free(self->current_configuration);
  self->current_configuration = NULL;
}

void
main_loop_deinit(MainLoop *self)
{
  main_loop_free_config(self);

  if (self->control_server)
    control_server_free(self->control_server);

  iv_event_unregister(&self->exit_requested);
  iv_event_unregister(&self->reload_config_requested);
  main_loop_call_deinit();
  main_loop_io_worker_deinit();
  main_loop_worker_deinit();
  block_till_workers_exit();
  scratch_buffers_automatic_gc_deinit();
  g_static_mutex_free(&workers_running_lock);
}

void
main_loop_run(MainLoop *self)
{
  msg_notice("syslog-ng starting up",
             evt_tag_str("version", SYSLOG_NG_VERSION));

  /* main loop */
  service_management_indicate_readiness();
  service_management_clear_status();
  if (self->options->interactive_mode)
    {
      cfg_load_module(self->current_configuration, "mod-python");
      debugger_start(self, self->current_configuration);
    }
  app_running();
  iv_main();
  service_management_publish_status("Shutting down...");
}

void
main_loop_add_options(GOptionContext *ctx)
{
  main_loop_io_worker_add_options(ctx);
}

void
main_loop_thread_resource_init(void)
{
  thread_halt_cond = g_cond_new();
  main_thread_handle = get_thread_id();
}

void main_loop_thread_resource_deinit(void)
{
  g_cond_free(thread_halt_cond);
}
