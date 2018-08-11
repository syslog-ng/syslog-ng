/*
 * Copyright (c) 2002-2012 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
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

#include "gprocess.h"
#include "userdb.h"
#include "messages.h"
#include "reloc.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <signal.h>

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <pwd.h>
#include <grp.h>

#if SYSLOG_NG_ENABLE_LINUX_CAPS
#  include <sys/capability.h>
#  include <sys/prctl.h>
#endif

/*
 * NOTES:
 *
 * * pidfile is created and removed by the daemon (e.g. the child) itself,
 *   the parent does not touch that
 *
 * * we communicate with the user using stderr (using fprintf) as long as it
 *   is available and using syslog() afterwards
 *
 * * there are 3 processes involved in safe_background mode (e.g. auto-restart)
 *   - startup process which was started by the user (zorpctl)
 *   - supervisor process which automatically restarts the daemon when it exits abnormally
 *   - daemon processes which perform the actual task at hand
 *
 *   The startup process delivers the result of the first startup to its
 *   caller, if we can deliver a failure in this case then restarts will not
 *   be performed (e.g. if the first startup fails, the daemon will not be
 *   restarted even if auto-restart was enabled). After the first successful
 *   start, the startup process exits (delivering that startup was
 *   successful) and the supervisor process wait()s for the daemon processes
 *   to exit. If they exit prematurely (e.g. they crash) they will be
 *   restarted, if the startup is not successful in this case the restart
 *   will be attempted again just as if they crashed.
 *
 *   The processes communicate with two pairs of pipes, startup_result_pipe
 *   is used to indicate success/failure to the startup process,
 *   init_result_pipe (as in "initialization") is used to deliver success
 *   reports from the daemon to the supervisor.
 */


typedef enum
{
  G_PK_STARTUP,
  G_PK_SUPERVISOR,
  G_PK_DAEMON,
} GProcessKind;

#define SAFE_STRING(x) ((x) ? (x) : "NULL")
#define G_PROCESS_FD_LIMIT_RESERVE 64
#define G_PROCESS_FAILURE_NOTIFICATION SYSLOG_NG_PATH_PREFIX "/sbin/syslog-ng-failure"

/* pipe used to deliver the initialization result to the calling process */
static gint startup_result_pipe[2] = { -1, -1 };
/* pipe used to deliver initialization result to the supervisor */
static gint init_result_pipe[2] = { -1, -1 };
static GProcessKind process_kind = G_PK_STARTUP;
static gboolean stderr_present = TRUE;
#if SYSLOG_NG_ENABLE_LINUX_CAPS
static int have_capsyslog = FALSE;
static cap_value_t cap_syslog;
#endif
#ifdef SYSLOG_NG_HAVE_ENVIRON
extern char **environ;
#endif

/* global variables */
static struct
{
  GProcessMode mode;
  const gchar *name;
  const gchar *user;
  gint uid;
  const gchar *group;
  gint gid;
  const gchar *chroot_dir;
  const gchar *pidfile;
  const gchar *pidfile_dir;
  const gchar *cwd;
  const gchar *caps;
  gint  argc;
  gchar **argv;
  gchar *argv_start;
  size_t argv_env_len;
  gchar *argv_orig;
  gboolean core;
  gint fd_limit_min;
  gint check_period;
  gboolean (*check_fn)(void);
} process_opts =
{
  .mode = G_PM_SAFE_BACKGROUND,
  .argc = 0,
  .argv = NULL,
  .argv_start = NULL,
  .argv_env_len = 0,
  .fd_limit_min = 0,
  .check_period = -1,
  .check_fn = NULL,
  .uid = -1,
  .gid = -1
};

#if SYSLOG_NG_ENABLE_SYSTEMD
/**
 * Inherits systemd socket activation from parent process updating the pid
 * in LISTEN_PID to the pid of the child process.
 *
 * @return same as sd_listen_fds
 *   r == 0: no socket activation or this process is not responsible
 *   r >  0: success, number of sockets
 *   r <  0: an error occurred
 */
static int
inherit_systemd_activation(void)
{
  const char *e;
  char buf[24] = { '\0' };
  char *p = NULL;
  unsigned long l;

  /* fetch listen pid */
  if (!(e = getenv("LISTEN_PID")))
    return 0;

  errno = 0;
  l = strtoul(e, &p, 10);
  if (errno != 0 || !p || *p || l == 0)
    return (errno) ? -errno : -EINVAL;

  /* was it for our parent? */
  if (getppid() != (pid_t)l)
    return 0;

  /* verify listen fds */
  if (!(e = getenv("LISTEN_FDS")))
    return 0;

  errno = 0;
  l = strtoul(e, &p, 10);
  if (errno != 0 || !p || *p)
    return (errno) ? -errno : -EINVAL;

  /* update the listen pid to ours */
  snprintf(buf, sizeof(buf), "%d", getpid());
  if (errno != 0 || !*buf)
    return (errno) ? -errno : -EINVAL;

  if (setenv("LISTEN_PID", buf, 1) == 0)
    return (int)l;

  return -1;
}
#else

#define inherit_systemd_activation()

#endif

#if SYSLOG_NG_ENABLE_LINUX_CAPS

typedef enum _cap_result_type
{
  CAP_NOT_SUPPORTED_BY_KERNEL = -2,
  CAP_NOT_SUPPORTED_BY_LIBCAP = -1,
  CAP_SUPPORTED               =  1,
} cap_result_type;

static cap_result_type
_check_and_get_cap_from_text(const gchar *cap_text, cap_value_t *cap)
{
  int ret;

  ret = cap_from_name(cap_text, cap);
  if (ret == -1)
    {
      return CAP_NOT_SUPPORTED_BY_LIBCAP;
    }

  ret = prctl(PR_CAPBSET_READ, *cap);
  if (ret == -1)
    {
      return CAP_NOT_SUPPORTED_BY_KERNEL;
    }

  return CAP_SUPPORTED;
}

/**
 * g_process_enable_cap:
 * @capability: capability to turn on
 *
 * This function modifies the current permitted set of capabilities by
 * enabling the capability specified in @capability.
 *
 * Returns: whether the operation was successful.
 **/
gboolean
g_process_enable_cap(const gchar *cap_name)
{
  if (!process_opts.caps)
    return TRUE;

  cap_value_t capability;

  cap_result_type ret = _check_and_get_cap_from_text(cap_name, &capability);
  if (CAP_SUPPORTED != ret)
    return FALSE;

  /*
   * if libcap or kernel doesn't support cap_syslog, then resort to
   * cap_sys_admin
   */
  if (capability == cap_syslog && !have_capsyslog)
    {
      ret = _check_and_get_cap_from_text("cap_sys_admin", &capability);
      if (ret != CAP_SUPPORTED)
        return FALSE;
    }

  cap_t caps = cap_get_proc();
  if (!caps)
    return FALSE;

  if (cap_set_flag(caps, CAP_EFFECTIVE, 1, &capability, CAP_SET) == -1)
    {
      goto error;
    }

  if (cap_set_proc(caps) == -1)
    {
      goto error;
    }

  cap_free(caps);
  return TRUE;

  char *cap_text = NULL;

error:

  cap_text = cap_to_text(caps, NULL);
  msg_error("Error managing capability set",
            evt_tag_str("caps", cap_text),
            evt_tag_error("error"));

  cap_free(cap_text);
  cap_free(caps);
  return FALSE;
}

/**
 * g_process_cap_save:
 *
 * Save the set of current capabilities and return it. The caller might
 * restore the saved set of capabilities by using cap_restore().
 *
 * Returns: the current set of capabilities
 **/
cap_t
g_process_cap_save(void)
{
  if (!process_opts.caps)
    return NULL;

  return cap_get_proc();
}

/**
 * cap_restore:
 * @r: capability set saved by cap_save()
 *
 * Restore the set of current capabilities specified by @r.
 *
 * Returns: whether the operation was successful.
 **/
void
g_process_cap_restore(cap_t r)
{
  gboolean rc;

  if (!process_opts.caps)
    return;

  rc = cap_set_proc(r) != -1;
  if (!rc)
    {
      gchar *cap_text;

      cap_text = cap_to_text(r, NULL);
      msg_error("Error managing capability set, cap_set_proc returned an error",
                evt_tag_str("caps", cap_text),
                evt_tag_error("error"));
      cap_free(cap_text);
    }
  cap_free(r);
}

#ifndef PR_CAPBSET_READ

/* old glibc versions don't have PR_CAPBSET_READ, we define it to the
 * value as defined in newer versions. */

#define PR_CAPBSET_READ 23
#endif

gboolean
g_process_check_cap_syslog(void)
{

  if (have_capsyslog)
    return TRUE;

  switch (_check_and_get_cap_from_text("cap_syslog", &cap_syslog))
    {
    case CAP_NOT_SUPPORTED_BY_LIBCAP:
      fprintf (stderr, "The CAP_SYSLOG is not supported by libcap;"
               "Falling back to CAP_SYS_ADMIN!\n");
      return FALSE;
      break;

    case CAP_NOT_SUPPORTED_BY_KERNEL:
      fprintf (stderr, "CAP_SYSLOG seems to be supported by libcap, but "
               "the kernel does not appear to recognize it. Falling back "
               "to CAP_SYS_ADMIN!\n");
      return FALSE;
      break;

    case CAP_SUPPORTED:
      have_capsyslog = TRUE;
      return TRUE;
      break;

    default:
      return FALSE;
      break;
    }
}

#endif

/**
 * g_process_set_mode:
 * @mode: an element from ZProcessMode
 *
 * This function should be called by the daemon to set the processing mode
 * as specified by @mode.
 **/
void
g_process_set_mode(GProcessMode mode)
{
  process_opts.mode = mode;
}

/**
 * g_process_get_mode:
 *
 * Return the processing mode applied to the daemon.
 **/
GProcessMode
g_process_get_mode(void)
{
  return process_opts.mode;
}

/**
 * g_process_set_name:
 * @name: the name of the process to be reported as program name
 *
 * This function should be called by the daemon to set the program name
 * which is present in various error message and might influence the PID
 * file if not overridden by g_process_set_pidfile().
 **/
void
g_process_set_name(const gchar *name)
{
  process_opts.name = name;
}

/**
 * g_process_set_user:
 * @user: the name of the user the process should switch to during startup
 *
 * This function should be called by the daemon to set the user name.
 **/
void
g_process_set_user(const gchar *user)
{
  if (!process_opts.user)
    process_opts.user = user;


}

/**
 * g_process_set_group:
 * @group: the name of the group the process should switch to during startup
 *
 * This function should be called by the daemon to set the group name.
 **/
void
g_process_set_group(const gchar *group)
{
  if (!process_opts.group)
    process_opts.group = group;

}

/**
 * g_process_set_chroot:
 * @chroot_dir: the name of the chroot directory the process should switch to during startup
 *
 * This function should be called by the daemon to set the chroot directory
 **/
void
g_process_set_chroot(const gchar *chroot_dir)
{
  if (!process_opts.chroot_dir)
    process_opts.chroot_dir = chroot_dir;
}

/**
 * g_process_set_pidfile:
 * @pidfile: the name of the complete pid file with full path
 *
 * This function should be called by the daemon to set the PID file name to
 * store the pid of the process. This value will be used as the pidfile
 * directly, neither name nor pidfile_dir influences the pidfile location if
 * this is set.
 **/
void
g_process_set_pidfile(const gchar *pidfile)
{
  if (!process_opts.pidfile)
    process_opts.pidfile = pidfile;
}

/**
 * g_process_set_pidfile_dir:
 * @pidfile_dir: name of the pidfile directory
 *
 * This function should be called by the daemon to set the PID file
 * directory. This value is not used if set_pidfile() was called.
 **/
void
g_process_set_pidfile_dir(const gchar *pidfile_dir)
{
  if (!process_opts.pidfile_dir)
    process_opts.pidfile_dir = pidfile_dir;
}

/**
 * g_process_set_working_dir:
 * @working_dir: name of the working directory
 *
 * This function should be called by the daemon to set the working
 * directory. The process will change its current directory to this value or
 * to pidfile_dir if it is unset.
 **/
void
g_process_set_working_dir(const gchar *cwd)
{
  if (!process_opts.cwd)
    process_opts.cwd = cwd;
}


/**
 * g_process_set_caps:
 * @caps: capability specification in text form
 *
 * This function should be called by the daemon to set the initial
 * capability set. The process will change its capabilities to this value
 * during startup, provided it has enough permissions to do so.
 **/
void
g_process_set_caps(const gchar *caps)
{
  if (!process_opts.caps)
    process_opts.caps = caps;
}

/**
 * g_process_set_argv_space:
 * @argc: Original argc, as received by the main function in it's first parameter
 * @argv: Original argv, as received by the main function in it's second parameter
 *
 * This function should be called by the daemon if it wants to enable
 * process title manipulation in the supervisor process.
 **/
void
g_process_set_argv_space(gint argc, gchar **argv)
{
#ifdef SYSLOG_NG_HAVE_ENVIRON
  gchar *lastargv = NULL;
  gchar **envp    = environ;
  gint i;

  if (process_opts.argv)
    return;
  process_opts.argv = argv;
  process_opts.argc = argc;

  for (i = 0; envp[i] != NULL; i++)
    ;

  environ = g_new(char *, i + 1);
  /*
   * Find the last argv string or environment variable within
   * our process memory area.
   */
  for (i = 0; i < process_opts.argc; i++)
    {
      if (lastargv == NULL || lastargv + 1 == process_opts.argv[i])
        lastargv = process_opts.argv[i] + strlen(process_opts.argv[i]);
    }
  for (i = 0; envp[i] != NULL; i++)
    {
      if (lastargv + 1 == envp[i])
        lastargv = envp[i] + strlen(envp[i]);
    }

  process_opts.argv_start = process_opts.argv[0];
  process_opts.argv_env_len = lastargv - process_opts.argv[0] - 1;

  process_opts.argv_orig = malloc(sizeof(gchar) * process_opts.argv_env_len);
  memcpy(process_opts.argv_orig, process_opts.argv_start, process_opts.argv_env_len);

  /*
   * Copy environment
   * XXX - will truncate env on strdup fail
   */
  for (i = 0; envp[i] != NULL; i++)
    environ[i] = g_strdup(envp[i]);
  environ[i] = NULL;
#endif
}

/**
 * g_process_set_check:
 * @check_period: check period in seconds
 * @check_fn: checker function
 *
 * Installs a checker function that is called at the specified rate.
 * The checked process is allowed to run as long as this function
 * returns TRUE.
 */
void
g_process_set_check(gint check_period, gboolean (*check_fn)(void))
{
  process_opts.check_period = check_period;
  process_opts.check_fn = check_fn;
}


/**
 * g_process_message:
 * @fmt: format string
 * @...: arguments to @fmt
 *
 * This function sends a message to the client preferring to use the stderr
 * channel as long as it is available and switching to using syslog() if it
 * isn't. Generally the stderr channell will be available in the startup
 * process and in the beginning of the first startup in the
 * supervisor/daemon processes. Later on the stderr fd will be closed and we
 * have to fall back to using the system log.
 **/
void
g_process_message(const gchar *fmt, ...)
{
  gchar buf[2048];
  va_list ap;

  va_start(ap, fmt);
  g_vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  if (stderr_present)
    fprintf(stderr, "%s: %s\n", process_opts.name, buf);
  else
    {
      gchar name[32];

      g_snprintf(name, sizeof(name), "%s/%s", process_kind == G_PK_SUPERVISOR ? "supervise" : "daemon", process_opts.name);
      openlog(name, LOG_PID, LOG_DAEMON);
      syslog(LOG_CRIT, "%s\n", buf);
      closelog();
    }
}

/**
 * g_process_detach_tty:
 *
 * This function is called from g_process_start() to detach from the
 * controlling tty.
 **/
static void
g_process_detach_tty(void)
{
  if (process_opts.mode != G_PM_FOREGROUND)
    {
      /* detach ourselves from the tty when not staying in the foreground */
      if (isatty(STDIN_FILENO))
        {
#ifdef TIOCNOTTY
          ioctl(STDIN_FILENO, TIOCNOTTY, 0);
#endif
        }
      setsid();
    }
}

/**
 * g_process_change_limits:
 *
 * Set fd limit.
 *
 **/
static void
g_process_change_limits(void)
{
  struct rlimit limit;

  getrlimit(RLIMIT_NOFILE, &limit);
  limit.rlim_cur = limit.rlim_max;
  if (process_opts.fd_limit_min)
    {
      limit.rlim_cur = limit.rlim_max = process_opts.fd_limit_min;
    }

  if (setrlimit(RLIMIT_NOFILE, &limit) < 0)
    g_process_message("Error setting file number limit; limit='%d'; error='%s'", process_opts.fd_limit_min,
                      g_strerror(errno));
}

/**
 * g_process_detach_stdio:
 *
 * Use /dev/null as input/output/error. This function is idempotent, can be
 * called any number of times without harm.
 **/
static void
g_process_detach_stdio(void)
{
  gint devnull_fd;

  if (process_opts.mode != G_PM_FOREGROUND && stderr_present)
    {
      devnull_fd = open("/dev/null", O_RDONLY);
      if (devnull_fd >= 0)
        {
          dup2(devnull_fd, STDIN_FILENO);
          close(devnull_fd);
        }
      devnull_fd = open("/dev/null", O_WRONLY);
      if (devnull_fd >= 0)
        {
          dup2(devnull_fd, STDOUT_FILENO);
          dup2(devnull_fd, STDERR_FILENO);
          close(devnull_fd);
        }
      stderr_present = FALSE;
    }
}

static void
g_process_set_dumpable(void)
{
#if SYSLOG_NG_ENABLE_LINUX_CAPS
  if (!prctl(PR_GET_DUMPABLE, 0, 0, 0, 0))
    {
      gint rc;

      rc = prctl(PR_SET_DUMPABLE, 1, 0, 0, 0);

      if (rc < 0)
        g_process_message("Cannot set process to be dumpable; error='%s'", g_strerror(errno));
    }
#endif
}

/**
 * g_process_enable_core:
 *
 * Enable core file dumping by setting PR_DUMPABLE and changing the core
 * file limit to infinity.
 **/
static void
g_process_enable_core(void)
{
  struct rlimit limit;

  if (process_opts.core)
    {
      g_process_set_dumpable();

      limit.rlim_cur = limit.rlim_max = RLIM_INFINITY;
      if (setrlimit(RLIMIT_CORE, &limit) < 0)
        g_process_message("Error setting core limit to infinity; error='%s'", g_strerror(errno));

    }
}

/**
 * g_process_format_pidfile_name:
 * @buf: buffer to store the pidfile name
 * @buflen: size of @buf
 *
 * Format the pid file name according to the settings specified by the
 * process.
 **/
static const gchar *
g_process_format_pidfile_name(gchar *buf, gsize buflen)
{
  const gchar *pidfile = process_opts.pidfile;

  if (pidfile == NULL)
    {
      g_snprintf(buf, buflen, "%s/%s.pid",
                 process_opts.pidfile_dir ? process_opts.pidfile_dir : get_installation_path_for(SYSLOG_NG_PATH_PIDFILEDIR),
                 process_opts.name);
      pidfile = buf;
    }
  else if (pidfile[0] != '/')
    {
      /* complete path to pidfile not specified, assume it is a relative path to pidfile_dir */
      g_snprintf(buf, buflen, "%s/%s", process_opts.pidfile_dir ? process_opts.pidfile_dir : get_installation_path_for(
                   SYSLOG_NG_PATH_PIDFILEDIR), pidfile);
      pidfile = buf;

    }
  return pidfile;
}

/**
 * g_process_write_pidfile:
 * @pid: pid to write into the pidfile
 *
 * Write the pid to the pidfile.
 **/
static void
g_process_write_pidfile(pid_t pid)
{
  gchar buf[256];
  const gchar *pidfile;
  FILE *fd;

  pidfile = g_process_format_pidfile_name(buf, sizeof(buf));
  fd = fopen(pidfile, "w");
  if (fd != NULL)
    {
      fprintf(fd, "%d\n", (int) pid);
      fclose(fd);
    }
  else
    {
      g_process_message("Error creating pid file; file='%s', error='%s'", pidfile, g_strerror(errno));
    }

}

/**
 * g_process_remove_pidfile:
 *
 * Remove the pidfile.
 **/
static void
g_process_remove_pidfile(void)
{
  gchar buf[256];
  const gchar *pidfile;

  pidfile = g_process_format_pidfile_name(buf, sizeof(buf));

  if (unlink(pidfile) < 0)
    {
      g_process_message("Error removing pid file; file='%s', error='%s'", pidfile, g_strerror(errno));
    }
}

/**
 * g_process_change_root:
 *
 * Change the current root to the value specified by the user, causes the
 * startup process to fail if this function returns FALSE. (e.g. the user
 * specified a chroot but we could not change to that directory)
 *
 * Returns: TRUE to indicate success
 **/
static gboolean
g_process_change_root(void)
{
  if (process_opts.chroot_dir)
    {
      if (chroot(process_opts.chroot_dir) < 0)
        {
          g_process_message("Error in chroot(); chroot='%s', error='%s'\n", process_opts.chroot_dir, g_strerror(errno));
          return FALSE;
        }
      if (chdir("/") < 0)
        {
          g_process_message("Error in chdir() after chroot; chroot='%s', error='%s'\n", process_opts.chroot_dir,
                            g_strerror(errno));
          return FALSE;
        }
    }
  return TRUE;
}

static void
g_process_keep_caps(void)
{
#if SYSLOG_NG_ENABLE_LINUX_CAPS
  if (process_opts.caps)
    prctl(PR_SET_KEEPCAPS, 1, 0, 0, 0);
#endif
}

/**
 * g_process_change_user:
 *
 * Change the current user/group/groups to the value specified by the user.
 * causes the startup process to fail if this function returns FALSE. (e.g.
 * the user requested the uid/gid to change we could not change to that uid)
 *
 * Returns: TRUE to indicate success
 **/
static gboolean
g_process_change_user(void)
{
  g_process_keep_caps();

  if (process_opts.gid >= 0)
    {
      if (setgid((gid_t) process_opts.gid) < 0)
        {
          g_process_message("Error in setgid(); group='%s', gid='%d', error='%s'", process_opts.group, process_opts.gid,
                            g_strerror(errno));
          if (getuid() == 0)
            return FALSE;
        }
      if (process_opts.user && initgroups(process_opts.user, (gid_t) process_opts.gid) < 0)
        {
          g_process_message("Error in initgroups(); user='%s', error='%s'", process_opts.user, g_strerror(errno));
          if (getuid() == 0)
            return FALSE;
        }
    }

  if (process_opts.uid >= 0)
    {
      if (setuid((uid_t) process_opts.uid) < 0)
        {
          g_process_message("Error in setuid(); user='%s', uid='%d', error='%s'", process_opts.user, process_opts.uid,
                            g_strerror(errno));
          if (getuid() == 0)
            return FALSE;
        }
    }

  return TRUE;
}

#if SYSLOG_NG_ENABLE_LINUX_CAPS
/**
 * g_process_change_caps:
 *
 * Change the current capset to the value specified by the user.  causes the
 * startup process to fail if this function returns FALSE, but we only do
 * this if the capset cannot be parsed, otherwise a failure changing the
 * capabilities will not result in failure
 *
 * Returns: TRUE to indicate success
 **/
static gboolean
g_process_change_caps(void)
{
  if (process_opts.caps)
    {
      cap_t cap = cap_from_text(process_opts.caps);

      if (cap == NULL)
        {
          g_process_message("Error parsing capabilities: %s", process_opts.caps);
          process_opts.caps = NULL;
          return FALSE;
        }
      else
        {
          if (cap_set_proc(cap) == -1)
            {
              g_process_message("Error setting capabilities, capability management disabled; error='%s'", g_strerror(errno));
              process_opts.caps = NULL;

            }
          cap_free(cap);
        }
    }
  return TRUE;
}

#else

static gboolean
g_process_change_caps(void)
{
  return TRUE;
}

#endif

static void
g_process_resolve_names(void)
{
  if (process_opts.user && !resolve_user(process_opts.user, &process_opts.uid))
    {
      g_process_message("Error resolving user; user='%s'", process_opts.user);
      process_opts.uid = -1;
    }
  if (process_opts.group && !resolve_group(process_opts.group, &process_opts.gid))
    {
      g_process_message("Error resolving group; group='%s'", process_opts.group);
      process_opts.gid = -1;
    }
}

/**
 * g_process_change_dir:
 *
 * Change the current working directory to the value specified by the user
 * and verify that the daemon would be able to dump core to that directory
 * if that is requested.
 **/
static void
g_process_change_dir(void)
{
  const gchar *cwd = NULL;

  if (process_opts.mode != G_PM_FOREGROUND)
    {
      if (process_opts.cwd)
        cwd = process_opts.cwd;
      else if (process_opts.pidfile_dir)
        cwd = process_opts.pidfile_dir;
      if (!cwd)
        cwd = get_installation_path_for(SYSLOG_NG_PATH_PIDFILEDIR);

      if (cwd)
        if (chdir(cwd))
          g_process_message("Error changing to directory=%s, errcode=%d", cwd, errno);
    }

  /* this check is here to avoid having to change directory early in the startup process */
  if ((process_opts.core) && access(".", W_OK) < 0)
    {
      gchar buf[256];

      if (!getcwd(buf, sizeof(buf)))
        strncpy(buf, "unable-to-query", sizeof(buf));
      g_process_message("Unable to write to current directory, core dumps will not be generated; dir='%s', error='%s'", buf,
                        g_strerror(errno));
    }

}

/**
 * g_process_send_result:
 * @ret_num: exit code of the process
 *
 * This function is called to notify our parent process (which is the same
 * executable process but separated with a fork()) about the result of the
 * process startup phase. Specifying ret_num == 0 means that everything was
 * dandy, all other values mean that the initialization failed and the
 * parent should exit using ret_num as the exit code. The function behaves
 * differently depending on which process it was called from, determined by
 * the value of the process_kind global variable. In the daemon process it
 * writes to init_result_pipe, in the startup process it writes to the
 * startup_result_pipe.
 *
 * This function can only be called once, further invocations will do nothing.
 **/
static void
g_process_send_result(guint ret_num)
{
  gchar buf[10];
  guint buf_len;
  gint *fd;

  if (process_kind == G_PK_SUPERVISOR)
    fd = &startup_result_pipe[1];
  else if (process_kind == G_PK_DAEMON)
    fd = &init_result_pipe[1];
  else
    g_assert_not_reached();

  if (*fd != -1)
    {
      buf_len = g_snprintf(buf, sizeof(buf), "%d\n", ret_num);
      if (write(*fd, buf, buf_len) < buf_len)
        g_assert_not_reached();
      close(*fd);
      *fd = -1;
    }
}

/**
 * g_process_recv_result:
 *
 * Retrieves an exit code value from one of the result pipes depending on
 * which process the function was called from. This function can be called
 * only once, further invocations will return non-zero result code.
 **/
static gint
g_process_recv_result(void)
{
  gchar ret_buf[6];
  gint ret_num = 1;
  gint *fd;

  /* FIXME: use a timer */
  if (process_kind == G_PK_SUPERVISOR)
    fd = &init_result_pipe[0];
  else if (process_kind == G_PK_STARTUP)
    fd = &startup_result_pipe[0];
  else
    g_assert_not_reached();

  if (*fd != -1)
    {
      memset(ret_buf, 0, sizeof(ret_buf));
      if (read(*fd, ret_buf, sizeof(ret_buf)) > 0)
        {
          ret_num = atoi(ret_buf);
        }
      else
        {
          /* the process probably crashed without telling a proper exit code */
          ret_num = 1;
        }
      close(*fd);
      *fd = -1;
    }
  return ret_num;
}

/**
 * g_process_perform_startup:
 *
 * This function is the startup process, never returns, the startup process exits here.
 **/
static void
g_process_perform_startup(void)
{
  /* startup process */
  exit(g_process_recv_result());
}


#define SPT_PADCHAR   '\0'

static void
g_process_setproctitle(const gchar *proc_title)
{
#ifdef SYSLOG_NG_HAVE_ENVIRON
  size_t len;

  g_assert(process_opts.argv_start != NULL);

  len = g_strlcpy(process_opts.argv_start, proc_title, process_opts.argv_env_len);
  for (; len < process_opts.argv_env_len; ++len)
    process_opts.argv_start[len] = SPT_PADCHAR;
#endif
}


#define PROC_TITLE_SPACE 1024

/**
 * g_process_perform_supervise:
 *
 * Supervise process, returns only in the context of the daemon process, the
 * supervisor process exits here.
 **/
static void
g_process_perform_supervise(void)
{
  pid_t pid;
  gboolean first = TRUE, exited = FALSE;
  gchar proc_title[PROC_TITLE_SPACE];
  struct sigaction sa;

  g_snprintf(proc_title, PROC_TITLE_SPACE, "supervising %s", process_opts.name);
  g_process_setproctitle(proc_title);

  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = SIG_IGN;
  sigaction(SIGHUP, &sa, NULL);

  while (1)
    {
      if (pipe(init_result_pipe) != 0)
        {
          g_process_message("Error daemonizing process, cannot open pipe; error='%s'", g_strerror(errno));
          g_process_startup_failed(1, TRUE);
        }

      /* fork off a child process */
      if ((pid = fork()) < 0)
        {
          g_process_message("Error forking child process; error='%s'", g_strerror(errno));
          g_process_startup_failed(1, TRUE);
        }
      else if (pid != 0)
        {
          gint rc;
          gboolean deadlock = FALSE;

          /* this is the supervisor process */

          /* shut down init_result_pipe write side */
          close(init_result_pipe[1]);
          init_result_pipe[1] = -1;

          rc = g_process_recv_result();
          if (first)
            {
              /* first time encounter, we have a chance to report back, do it */
              g_process_send_result(rc);
              if (rc != 0)
                break;
              g_process_detach_stdio();
            }
          first = FALSE;
          if (rc != 0)
            {
              gint i = 0;
              /* initialization failed in daemon, it will probably exit soon, wait and restart */

              while (i < 6 && waitpid(pid, &rc, WNOHANG) == 0)
                {
                  if (i > 3)
                    kill(pid, i > 4 ? SIGKILL : SIGTERM);
                  sleep(1);
                  i++;
                }
              if (i == 6)
                g_process_message("Initialization failed but the daemon did not exit, even when forced to, trying to recover; pid='%d'",
                                  pid);
              continue;
            }

          if (process_opts.check_fn && (process_opts.check_period >= 0))
            {
              gint i = 1;
              while (!(exited = waitpid(pid, &rc, WNOHANG)))
                {
                  if (i >= process_opts.check_period)
                    {
                      if (!process_opts.check_fn())
                        break;
                      i = 0;
                    }
                  sleep(1);
                  i++;
                }

              if (!exited)
                {
                  gint j = 0;
                  g_process_message("Daemon deadlock detected, killing process;");
                  deadlock = TRUE;

                  while (j < 6 && waitpid(pid, &rc, WNOHANG) == 0)
                    {
                      if (j > 3)
                        kill(pid, j > 4 ? SIGKILL : SIGABRT);
                      sleep(1);
                      j++;
                    }
                  if (j == 6)
                    g_process_message("The daemon did not exit after deadlock, even when forced to, trying to recover; pid='%d'", pid);
                }
            }
          else
            {
              waitpid(pid, &rc, 0);
            }

          if (deadlock || WIFSIGNALED(rc) || (WIFEXITED(rc) && WEXITSTATUS(rc) != 0))
            {
              gchar argbuf[64];

              if (!access(G_PROCESS_FAILURE_NOTIFICATION, R_OK | X_OK))
                {
                  const gchar *notify_reason;
                  pid_t npid = fork();
                  gint nrc;
                  switch (npid)
                    {
                    case -1:
                      g_process_message("Could not fork for external notification; reason='%s'", strerror(errno));
                      break;

                    case 0:
                      switch(fork())
                        {
                        case -1:
                          g_process_message("Could not fork for external notification; reason='%s'", strerror(errno));
                          exit(1);
                          break;
                        case 0:
                          if (deadlock)
                            {
                              notify_reason = "deadlock detected";
                              argbuf[0] = 0;
                            }
                          else
                            {
                              snprintf(argbuf, sizeof(argbuf), "%d", WIFSIGNALED(rc) ? WTERMSIG(rc) : WEXITSTATUS(rc));
                              if (WIFSIGNALED(rc))
                                notify_reason = "signalled";
                              else
                                notify_reason = "non-zero exit code";
                            }
                          execlp(G_PROCESS_FAILURE_NOTIFICATION, G_PROCESS_FAILURE_NOTIFICATION,
                                 SAFE_STRING(process_opts.name),
                                 SAFE_STRING(process_opts.chroot_dir),
                                 SAFE_STRING(process_opts.pidfile_dir),
                                 SAFE_STRING(process_opts.pidfile),
                                 SAFE_STRING(process_opts.cwd),
                                 SAFE_STRING(process_opts.caps),
                                 notify_reason,
                                 argbuf,
                                 (deadlock || !WIFSIGNALED(rc) || WTERMSIG(rc) != SIGKILL) ? "restarting" : "not-restarting",
                                 (gchar *) NULL);
                          g_process_message("Could not execute external notification; reason='%s'", strerror(errno));
                          break;

                        default:
                          exit(0);
                          break;
                        } /* child process */
                    default:
                      waitpid(npid, &nrc, 0);
                      break;
                    }
                }
              if (deadlock || !WIFSIGNALED(rc) || WTERMSIG(rc) != SIGKILL)
                {
                  g_process_message("Daemon exited due to a deadlock/signal/failure, restarting; exitcode='%d'", rc);
                  sleep(1);
                }
              else
                {
                  g_process_message("Daemon was killed, not restarting; exitcode='%d'", rc);
                  break;
                }
            }
          else
            {
              g_process_message("Daemon exited gracefully, not restarting; exitcode='%d'", rc);
              break;
            }
        }
      else
        {
          /* this is the daemon process, thus we should return to the caller of g_process_start() */
          /* shut down init_result_pipe read side */
          process_kind = G_PK_DAEMON;
          close(init_result_pipe[0]);
          init_result_pipe[0] = -1;

          /* update systemd socket activation pid */
          inherit_systemd_activation();

          memcpy(process_opts.argv_start, process_opts.argv_orig, process_opts.argv_env_len);
          return;
        }
    }
  exit(0);
}

/**
 * g_process_start:
 *
 * Start the process as directed by the options set by various
 * g_process_set_*() functions.
 **/
void
g_process_start(void)
{
  pid_t pid;

  g_process_detach_tty();
  g_process_change_limits();
  g_process_resolve_names();

  if (process_opts.mode == G_PM_BACKGROUND)
    {
      /* no supervisor, sends result to startup process directly */
      if (pipe(init_result_pipe) != 0)
        {
          g_process_message("Error daemonizing process, cannot open pipe; error='%s'", g_strerror(errno));
          exit(1);
        }

      if ((pid = fork()) < 0)
        {
          g_process_message("Error forking child process; error='%s'", g_strerror(errno));
          exit(1);
        }
      else if (pid != 0)
        {
          /* shut down init_result_pipe write side */

          close(init_result_pipe[1]);

          /* connect startup_result_pipe with init_result_pipe */
          startup_result_pipe[0] = init_result_pipe[0];
          init_result_pipe[0] = -1;

          g_process_perform_startup();
          /* NOTE: never returns */
          g_assert_not_reached();
        }
      process_kind = G_PK_DAEMON;

      /* shut down init_result_pipe read side */
      close(init_result_pipe[0]);
      init_result_pipe[0] = -1;

      /* update systemd socket activation pid */
      inherit_systemd_activation();
    }
  else if (process_opts.mode == G_PM_SAFE_BACKGROUND)
    {
      /* full blown startup/supervisor/daemon */
      if (pipe(startup_result_pipe) != 0)
        {
          g_process_message("Error daemonizing process, cannot open pipe; error='%s'", g_strerror(errno));
          exit(1);
        }
      /* first fork off supervisor process */
      if ((pid = fork()) < 0)
        {
          g_process_message("Error forking child process; error='%s'", g_strerror(errno));
          exit(1);
        }
      else if (pid != 0)
        {
          /* this is the startup process */

          /* shut down startup_result_pipe write side */
          close(startup_result_pipe[1]);
          startup_result_pipe[1] = -1;

          /* NOTE: never returns */
          g_process_perform_startup();
          g_assert_not_reached();
        }
      /* this is the supervisor process */

      /* shut down startup_result_pipe read side */
      close(startup_result_pipe[0]);
      startup_result_pipe[0] = -1;

      /* update systemd socket activation pid */
      inherit_systemd_activation();

      process_kind = G_PK_SUPERVISOR;
      g_process_perform_supervise();
      /* we only return in the daamon process here */
    }
  else if (process_opts.mode == G_PM_FOREGROUND)
    {
      process_kind = G_PK_DAEMON;
    }
  else
    {
      g_assert_not_reached();
    }

  /* daemon process, we should return to the caller to perform work */
  /* Only call setsid() for backgrounded processes. */
  if (process_opts.mode != G_PM_FOREGROUND)
    {
      setsid();
    }

  /* NOTE: we need to signal the parent in case of errors from this point.
   * This is accomplished by writing the appropriate exit code to
   * init_result_pipe, the easiest way doing so is calling g_process_startup_failed.
   * */

  if (!g_process_change_root() ||
      !g_process_change_user() ||
      !g_process_change_caps())
    {
      g_process_startup_failed(1, TRUE);
    }
  g_process_enable_core();
  g_process_change_dir();
}


/**
 * g_process_startup_failed:
 * @ret_num: exit code
 * @may_exit: whether to exit the process
 *
 * This is a public API function to be called by the user code when
 * initialization failed.
 **/
void
g_process_startup_failed(guint ret_num, gboolean may_exit)
{
  if (process_kind != G_PK_STARTUP)
    g_process_send_result(ret_num);

  if (may_exit)
    {
      exit(ret_num);
    }
  else
    {
      g_process_detach_stdio();
    }
}

/**
 * g_process_startup_ok:
 *
 * This is a public API function to be called by the user code when
 * initialization was successful, we can report back to the user.
 **/
void
g_process_startup_ok(void)
{
  g_process_write_pidfile(getpid());

  g_process_send_result(0);
  g_process_detach_stdio();
}

/**
 * g_process_finish:
 *
 * This is a public API function to be called by the user code when the
 * daemon exits after properly initialized (e.g. when it terminates because
 * of SIGTERM). This function currently only removes the PID file.
 **/
void
g_process_finish(void)
{
#ifdef SYSLOG_NG_HAVE_ENVIRON
  /**
   * There is a memory leak for **environ and elements that should be
   * freed here theoretically.
   *
   * The reason why environ is copied during g_process_set_argv_space
   * so to be able to overwrite process title in ps/top commands to
   * supervisor. In bsd there is setproctitle call which solve this,
   * but currently it is not available for linux.
   *
   * The problem is that modules can add their own env variables to the
   * list, which must not be freed here, otherwise it would result
   * double free (e.g some version of Oracle Java). One might also would
   * try to track which environment variables are added by syslog-ng,
   * and free only those. There is still a problem with this, **environ
   * itself cannot be freed in some cases. For example libcurl registers
   * an atexit function which needs an environment variable, that would
   * be freed here before at_exit is called, resulting in invalid read.
   *
   * As this leak does not cause any real problem like accumulating over
   * time, it is safe to leave it as it is.
  */
#endif

  g_process_remove_pidfile();
}

static gboolean
g_process_process_mode_arg(const gchar *option_name G_GNUC_UNUSED, const gchar *value, gpointer data G_GNUC_UNUSED,
                           GError **error)
{
  if (strcmp(value, "foreground") == 0)
    {
      process_opts.mode = G_PM_FOREGROUND;
    }
  else if (strcmp(value, "background") == 0)
    {
      process_opts.mode = G_PM_BACKGROUND;
    }
  else if (strcmp(value, "safe-background") == 0)
    {
      process_opts.mode = G_PM_SAFE_BACKGROUND;
    }
  else
    {
      g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE, "Error parsing process-mode argument");
      return FALSE;
    }
  return TRUE;
}

static gboolean
g_process_process_no_caps(const gchar *option_name G_GNUC_UNUSED, const gchar *value G_GNUC_UNUSED,
                          gpointer data G_GNUC_UNUSED, GError *error)
{
  process_opts.caps = NULL;
  return TRUE;
}

static GOptionEntry g_process_option_entries[] =
{
  { "foreground",   'F', G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE,     &process_opts.mode,              "Do not go into the background after initialization", NULL },
  { "process-mode",   0,                     0, G_OPTION_ARG_CALLBACK, g_process_process_mode_arg ,     "Set process running mode", "<foreground|background|safe-background>" },
  { "user",         'u',                     0, G_OPTION_ARG_STRING,   &process_opts.user,              "Set the user to run as", "<user>" },
  { "uid",            0,  G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_STRING,   &process_opts.user,              NULL, NULL },
  { "group",        'g',                     0, G_OPTION_ARG_STRING,   &process_opts.group,             "Set the group to run as", "<group>" },
  { "gid",            0,  G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_STRING,   &process_opts.group,             NULL, NULL },
  { "chroot",       'C',                     0, G_OPTION_ARG_STRING,   &process_opts.chroot_dir,        "Chroot to this directory", "<dir>" },
  { "caps",           0,                     0, G_OPTION_ARG_STRING,   &process_opts.caps,              "Set default capability set", "<capspec>" },
  { "no-caps",        0,  G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, g_process_process_no_caps,       "Disable managing Linux capabilities", NULL },
  { "pidfile",      'p',                     0, G_OPTION_ARG_STRING,   &process_opts.pidfile,           "Set path to pid file", "<pidfile>" },
  { "enable-core",    0,                     0, G_OPTION_ARG_NONE,     &process_opts.core,              "Enable dumping core files", NULL },
  { "fd-limit",       0,                  0, G_OPTION_ARG_INT,      &process_opts.fd_limit_min,         "The minimum required number of fds", NULL },
  { NULL, 0, 0, 0, NULL, NULL, NULL },
};

void
g_process_add_option_group(GOptionContext *ctx)
{
  GOptionGroup *group;

  group = g_option_group_new("process", "Process options", "Process options", NULL, NULL);
  g_option_group_add_entries(group, g_process_option_entries);
  g_option_context_add_group(ctx, group);
}
