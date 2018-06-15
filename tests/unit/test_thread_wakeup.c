/*
 * Copyright (c) 2010-2011 Balabit
 * Copyright (c) 2010-2011 Balázs Scheidler
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include "syslog-ng.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

gboolean thread_exit = FALSE;
gboolean thread_started;
GCond *thread_startup;
GMutex *thread_lock;
pthread_t thread_handle;

void
sigusr1_handler(int signo)
{
}

static void
setup_sigusr1(void)
{
  struct sigaction sa;

  memset(&sa, 0, sizeof(sa));
  /* NOTE: this has to be a real signal handler as SIG_IGN will not interrupt the accept call below */
  sa.sa_handler = sigusr1_handler;
  sa.sa_flags = 0;
  sigaction(SIGUSR1, &sa, NULL);
}

static void
signal_startup(void)
{
  thread_handle = pthread_self();
  g_mutex_lock(thread_lock);
  thread_started = TRUE;
  g_cond_signal(thread_startup);
  g_mutex_unlock(thread_lock);
}

static gboolean
create_test_thread(GThreadFunc thread_func, gpointer data)
{
  GThread *t;
  struct timespec nsleep;

  thread_exit = FALSE;
  thread_started = FALSE;
  t = g_thread_create(thread_func, data, TRUE, NULL);
  g_mutex_lock(thread_lock);
  while (!thread_started)
    g_cond_wait(thread_startup, thread_lock);
  g_mutex_unlock(thread_lock);
  nsleep.tv_sec = 0;
  nsleep.tv_nsec = 1e6;
  nanosleep(&nsleep, NULL);
  thread_exit = TRUE;
  pthread_kill(thread_handle, SIGUSR1);
  return (g_thread_join(t) != NULL);
}

int sock;

gpointer
accept_thread_func(gpointer args)
{
  struct sockaddr_un peer;
  gint new_sock;
  gpointer result = NULL;

  setup_sigusr1();
  signal_startup();
  while (1)
    {
      gint err;
      socklen_t peer_size = sizeof(peer);

      new_sock = accept(sock, (struct sockaddr *) &peer, &peer_size);
      err = errno;

      if (new_sock >= 0)
        close(new_sock);
      if (thread_exit)
        {
          fprintf(stderr, "accept woken up, errno=%s\n", g_strerror(err));
          result = (gpointer) 0x1;
          break;
        }
    }
  close(sock);
  return result;
}

int
test_accept_wakeup(void)
{
  struct sockaddr_un s;

  unlink("almafa");
  sock = socket(PF_UNIX, SOCK_STREAM, 0);
  s.sun_family = AF_UNIX;
  strcpy(s.sun_path, "almafa");
  if (bind(sock, (struct sockaddr *) &s, sizeof(s)) < 0)
    {
      perror("error binding socket");
      return 1;
    }
  if (listen(sock, 255) < 0)
    {
      perror("error in listen()");
      return 1;
    }
  return create_test_thread(accept_thread_func, NULL);
}

gpointer
read_thread_func(gpointer args)
{
  gint *pair = (gint *) args;
  gpointer result = NULL;

  setup_sigusr1();
  signal_startup();
  while (1)
    {
      gint err;
      gchar buf[1024];
      gint count G_GNUC_UNUSED = read(pair[1], buf, sizeof(buf));
      err = errno;

      if (thread_exit)
        {
          fprintf(stderr, "read woken up, errno=%s\n", g_strerror(err));
          result = (gpointer) 0x1;
          break;
        }
    }
  close(pair[0]);
  close(pair[1]);
  return result;
}

int
test_read_wakeup(void)
{
  gint pair[2];

  socketpair(PF_UNIX, SOCK_STREAM, 0, pair);
  return create_test_thread(read_thread_func, pair);
}

int
main(int argc, char *argv[])
{
  g_thread_init(NULL);

  thread_lock = g_mutex_new();
  thread_startup = g_cond_new();

  if (!test_accept_wakeup() ||
      !test_read_wakeup())
    return 1;
  return 0;
}
