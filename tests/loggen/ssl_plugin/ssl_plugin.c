/*
 * Copyright (c) 2018 Balabit
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

#include "compat/glib.h"
#include "loggen_plugin.h"
#include "loggen_helper.h"
#include "crypto.h"

#include <stddef.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>

#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

static void           start(PluginOption *option);
static void           stop(PluginOption *option);
static gpointer       active_thread_func(gpointer user_data);
static gpointer       idle_thread_func(gpointer user_data);
static gint           get_thread_count(void);
static void           set_generate_message(generate_message_func gen_message);
static GOptionEntry  *get_options(void);
static gboolean       is_plugin_activated(void);
static GPtrArray      *thread_array = NULL;

static gboolean thread_run;
static generate_message_func generate_message;
static GMutex *thread_lock = NULL;
static GCond *thread_start = NULL;
static GCond *thread_connected = NULL;
static gint connect_finished;
static gint active_thread_count;
static gint idle_thread_count;

static int use_ssl = 0;

static GOptionEntry loggen_options[] =
{
  { "use-ssl", 'U', 0, G_OPTION_ARG_NONE, &use_ssl,  "Use ssl layer", NULL },
  { NULL }
};

PluginInfo loggen_plugin_info =
{
  .name = "ssl-plugin",
  .get_options_list = get_options,
  .start_plugin = start,
  .stop_plugin = stop,
  .get_thread_count = get_thread_count,
  .set_generate_message = set_generate_message,
  .is_plugin_activated = is_plugin_activated,
  .require_framing = FALSE
};

static gboolean
is_plugin_activated(void)
{
  if (!use_ssl)
    {
      DEBUG("ssl plugin: none of command line option triggered. no thread will be started\n");
      return FALSE;
    }
  return TRUE;
}

static void
set_generate_message(generate_message_func gen_message)
{
  generate_message = gen_message;
}

static gint
get_thread_count(void)
{
  if (!thread_lock)
    return 0;

  int num;
  g_mutex_lock(thread_lock);
  num = active_thread_count+idle_thread_count;
  g_mutex_unlock(thread_lock);

  return num;
}

static GOptionEntry *
get_options(void)
{
  return loggen_options;
}

static void
start(PluginOption *option)
{
  if (!option)
    {
      ERROR("invalid option refernce\n");
      return;
    }

  if (!option->target || !option->port)
    {
      ERROR("please specify target and port parameters\n");
      return;
    }

  DEBUG("plugin (%d,%d,%d,%d)start\n",
        option->message_length,
        option->interval,
        option->number_of_messages,
        option->permanent
       );

  thread_array = g_ptr_array_new();

  thread_lock = g_mutex_new();
  thread_start = g_cond_new();
  thread_connected = g_cond_new();

  if (!is_plugin_activated())
    {
      active_thread_count  = 0;
      idle_thread_count  = 0;
      return;
    }
  else
    {
      active_thread_count  = option->active_connections;
      idle_thread_count  = option->idle_connections;
      /* call syslog-ng's crypto init */
      crypto_init();
    }

  connect_finished = 0;

  for (int j =0 ; j<active_thread_count; j++)
    {
      ThreadData *data = (ThreadData *)g_malloc0(sizeof(ThreadData));
      data->option = option;
      data->index = j;

      GThread *thread_id = g_thread_new(loggen_plugin_info.name, active_thread_func, (gpointer)data);
      g_ptr_array_add(thread_array, (gpointer) thread_id);
    }

  for (int j=0; j<idle_thread_count; j++)
    {
      ThreadData *data = (ThreadData *)g_malloc0(sizeof(ThreadData));
      data->option = option;
      data->index = j;

      GThread *thread_id = g_thread_new(loggen_plugin_info.name, idle_thread_func, (gpointer)data);
      g_ptr_array_add(thread_array, (gpointer) thread_id);
    }


  DEBUG("wait all thread to be connected to server\n");
  gint64 end_time;
  end_time = g_get_monotonic_time () + CONNECTION_TIMEOUT_SEC * G_TIME_SPAN_SECOND;

  g_mutex_lock(thread_lock);
  while (connect_finished != active_thread_count+idle_thread_count)
    {
      if (! g_cond_wait_until(thread_connected, thread_lock, end_time))
        {
          ERROR("timeout ocured while waiting for connections\n");
          break;
        }
    }

  /* start all threads */
  g_cond_broadcast(thread_start);
  thread_run = TRUE;

  g_mutex_unlock(thread_lock);
}

static void
stop(PluginOption *option)
{
  if (!option)
    {
      ERROR("invalid option refernce\n");
      return;
    }

  DEBUG("plugin stop\n");
  thread_run = FALSE;

  /* wait all threads to finish */
  for (int j =0 ; j<active_thread_count+idle_thread_count; j++)
    {
      GThread *thread_id = g_ptr_array_index(thread_array,j);
      if (!thread_id)
        continue;

      g_thread_join(thread_id);
    }

  /* call syslog-ng's crypto deinit */
  if (active_thread_count+idle_thread_count>0)
    crypto_deinit();

  if (thread_lock)
    g_mutex_free(thread_lock);

  if (thread_start)
    g_cond_free(thread_start);

  if (thread_connected)
    g_cond_free(thread_connected);

  DEBUG("all %d+%d threads have been stoped\n",
        active_thread_count,
        idle_thread_count);
}

gpointer
idle_thread_func(gpointer user_data)
{
  PluginOption *option = ((ThreadData *)user_data)->option;
  int thread_index = ((ThreadData *)user_data)->index;

  int sock_fd = connect_ip_socket(SOCK_STREAM, option->target, option->port, option->use_ipv6);;

  SSL *ssl = open_ssl_connection(sock_fd);
  if (ssl == NULL)
    {
      ERROR("can not connect to %s:%s (%p)\n",option->target, option->port,g_thread_self());
    }
  else
    {
      DEBUG("(%d) connected to server on socket (%p)\n",thread_index,g_thread_self());
    }

  g_mutex_lock(thread_lock);
  connect_finished++;

  if (connect_finished == active_thread_count + idle_thread_count)
    g_cond_broadcast(thread_connected);

  g_mutex_unlock(thread_lock);

  DEBUG("thread (%s,%p) created. wait for start ...\n",loggen_plugin_info.name,g_thread_self());
  g_mutex_lock(thread_lock);
  while (!thread_run)
    {
      g_cond_wait(thread_start,thread_lock);
    }
  g_mutex_unlock(thread_lock);

  DEBUG("thread (%s,%p) started. (r=%d,c=%d)\n",loggen_plugin_info.name,g_thread_self(),option->rate,
        option->number_of_messages);

  while (thread_run && active_thread_count>0)
    {
      g_usleep(10*1000);
    }

  g_mutex_lock(thread_lock);
  idle_thread_count--;
  g_mutex_unlock(thread_lock);

  close_ssl_connection(ssl);
  close(sock_fd);
  g_thread_exit(NULL);
  return NULL;
}

gpointer
active_thread_func(gpointer user_data)
{
  ThreadData *thread_context = (ThreadData *)user_data;
  PluginOption *option = thread_context->option;

  char *message = g_malloc0(MAX_MESSAGE_LENGTH+1);

  int sock_fd = connect_ip_socket(SOCK_STREAM, option->target, option->port, option->use_ipv6);;

  SSL *ssl = open_ssl_connection(sock_fd);

  if (ssl == NULL)
    {
      ERROR("can not connect to %s:%s (%p)\n",option->target, option->port,g_thread_self());
    }
  else
    {
      DEBUG("(%d) connected to server on socket (%p)\n",thread_context->index,g_thread_self());
    }

  g_mutex_lock(thread_lock);
  connect_finished++;

  if (connect_finished == active_thread_count + idle_thread_count)
    g_cond_broadcast(thread_connected);

  g_mutex_unlock(thread_lock);

  DEBUG("thread (%s,%p) created. wait for start ...\n",loggen_plugin_info.name,g_thread_self());
  g_mutex_lock(thread_lock);
  while (!thread_run)
    {
      g_cond_wait(thread_start,thread_lock);
    }
  g_mutex_unlock(thread_lock);

  DEBUG("thread (%s,%p) started. (r=%d,c=%d)\n",loggen_plugin_info.name,g_thread_self(),option->rate,
        option->number_of_messages);

  unsigned long count = 0;
  thread_context->buckets = thread_context->option->rate - (thread_context->option->rate / 10);

  gettimeofday(&thread_context->last_throttle_check, NULL);
  gettimeofday(&thread_context->start_time, NULL);

  gboolean connection_error = FALSE;

  while (ssl && thread_run && !connection_error)
    {
      if (thread_check_exit_criteria(thread_context))
        break;

      if (thread_check_time_bucket(thread_context))
        continue;

      if (!generate_message)
        {
          ERROR("generate_message not yet set up(%p)\n",g_thread_self());
          break;
        }

      int str_len = generate_message(message, MAX_MESSAGE_LENGTH, thread_context->index, count++);

      if (str_len < 0)
        {
          ERROR("can't generate more log lines. end of input file?\n");
          break;
        }

      ssize_t sent = 0;
      while (sent < strlen(message))
        {
          ssize_t rc = SSL_write(ssl, message + sent, strlen(message) - sent);
          if (rc < 0)
            {
              ERROR("error sending buffer on %p (rc=%zd)\n",ssl,rc);
              errno = ECONNABORTED;
              connection_error = TRUE;
              break;
            }
          sent += rc;
        }

      thread_context->sent_messages++;
      thread_context->buckets--;
    }
  DEBUG("thread (%s,%p) finished\n",loggen_plugin_info.name,g_thread_self());

  g_mutex_lock(thread_lock);
  active_thread_count--;
  g_mutex_unlock(thread_lock);

  g_free((gpointer)message);
  close_ssl_connection(ssl);
  close(sock_fd);

  g_thread_exit(NULL);
  return NULL;
}
