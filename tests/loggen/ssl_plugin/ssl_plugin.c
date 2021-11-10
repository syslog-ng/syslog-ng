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

static gboolean       start(PluginOption *option);
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
static GMutex thread_lock;
static GCond thread_start;
static GCond thread_connected;
static gint connect_finished;
static gint active_thread_count;
static gint idle_thread_count;

static int use_ssl = 0;
static int proxied_tls_passthrough = 0;

static GOptionEntry loggen_options[] =
{
  { "use-ssl", 'U', 0, G_OPTION_ARG_NONE, &use_ssl,  "Use ssl layer", NULL },
  { "proxied-tls-passthrough", 0, 0, G_OPTION_ARG_NONE, &proxied_tls_passthrough, "Send the PROXY protocol v1 header before the encrypted payload", NULL },
  { NULL }
};

PluginInfo ssl_loggen_plugin_info =
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
  g_mutex_lock(&thread_lock);
  int num = active_thread_count + idle_thread_count;
  g_mutex_unlock(&thread_lock);

  return num;
}

static GOptionEntry *
get_options(void)
{
  return loggen_options;
}

static gboolean
start(PluginOption *option)
{
  if (!option)
    {
      ERROR("invalid option reference\n");
      return FALSE;
    }

  if (!is_plugin_activated())
    return TRUE;

  if (!option->target || !option->port)
    {
      ERROR("please specify target and port parameters\n");
      return FALSE;
    }

  DEBUG("plugin (%d,%d,%d,%d)start\n",
        option->message_length,
        option->interval,
        option->number_of_messages,
        option->permanent
       );

  thread_array = g_ptr_array_new();

  g_mutex_init(&thread_lock);
  g_cond_init(&thread_start);
  g_cond_init(&thread_connected);

  active_thread_count = option->active_connections;
  idle_thread_count = option->idle_connections;
  crypto_init();

  connect_finished = 0;

  for (int j =0; j < option->active_connections; j++)
    {
      ThreadData *data = (ThreadData *)g_malloc0(sizeof(ThreadData));
      data->option = option;
      data->index = j;

      GThread *thread_id = g_thread_new(ssl_loggen_plugin_info.name, active_thread_func, (gpointer)data);
      g_ptr_array_add(thread_array, (gpointer) thread_id);
    }

  for (int j=0; j < option->idle_connections; j++)
    {
      ThreadData *data = (ThreadData *)g_malloc0(sizeof(ThreadData));
      data->option = option;
      data->index = j;

      GThread *thread_id = g_thread_new(ssl_loggen_plugin_info.name, idle_thread_func, (gpointer)data);
      g_ptr_array_add(thread_array, (gpointer) thread_id);
    }


  DEBUG("wait all thread to be connected to server\n");
  gint64 end_time;
  end_time = g_get_monotonic_time () + CONNECTION_TIMEOUT_SEC * G_TIME_SPAN_SECOND;

  g_mutex_lock(&thread_lock);
  while (connect_finished != option->active_connections + option->idle_connections)
    {
      if (! g_cond_wait_until(&thread_connected, &thread_lock, end_time))
        {
          ERROR("timeout occurred while waiting for connections\n");
          break;
        }
    }

  /* start all threads */
  g_cond_broadcast(&thread_start);
  thread_run = TRUE;

  g_mutex_unlock(&thread_lock);

  return TRUE;
}

static void
stop(PluginOption *option)
{
  if (!option)
    {
      ERROR("invalid option reference\n");
      return;
    }

  if (!is_plugin_activated())
    return;

  DEBUG("plugin stop\n");
  thread_run = FALSE;

  /* wait all threads to finish */
  for (int j = 0; j < option->active_connections + option->idle_connections; j++)
    {
      GThread *thread_id = g_ptr_array_index(thread_array, j);
      if (!thread_id)
        continue;

      g_thread_join(thread_id);
    }

  crypto_deinit();

  g_mutex_clear(&thread_lock);
  g_cond_clear(&thread_start);
  g_cond_clear(&thread_connected);

  DEBUG("all %d+%d threads have been stopped\n",
        option->active_connections,
        option->idle_connections);
}

gpointer
idle_thread_func(gpointer user_data)
{
  ThreadData *thread_context = (ThreadData *)user_data;
  PluginOption *option = thread_context->option;
  int thread_index = thread_context->index;

  int sock_fd = connect_ip_socket(SOCK_STREAM, option->target, option->port, option->use_ipv6);

  SSL *ssl = open_ssl_connection(sock_fd);
  if (ssl == NULL)
    {
      ERROR("can not connect to %s:%s (%p)\n", option->target, option->port, g_thread_self());
    }
  else
    {
      DEBUG("(%d) connected to server on socket (%p)\n", thread_index, g_thread_self());
    }

  g_mutex_lock(&thread_lock);
  connect_finished++;

  if (connect_finished == option->active_connections + option->idle_connections)
    g_cond_broadcast(&thread_connected);

  g_mutex_unlock(&thread_lock);

  DEBUG("thread (%s,%p) created. wait for start ...\n", ssl_loggen_plugin_info.name, g_thread_self());
  g_mutex_lock(&thread_lock);
  while (!thread_run)
    {
      g_cond_wait(&thread_start, &thread_lock);
    }
  g_mutex_unlock(&thread_lock);

  DEBUG("thread (%s,%p) started. (r=%d,c=%d)\n", ssl_loggen_plugin_info.name, g_thread_self(), option->rate,
        option->number_of_messages);

  while (thread_run && active_thread_count>0)
    {
      g_usleep(10*1000);
    }

  g_mutex_lock(&thread_lock);
  idle_thread_count--;
  g_mutex_unlock(&thread_lock);

  close_ssl_connection(ssl);
  shutdown(sock_fd, SHUT_RDWR);
  close(sock_fd);

  g_free(thread_context);
  g_thread_exit(NULL);
  return NULL;
}

static inline void
send_plaintext_proxy_header(ThreadData *thread_context, int sock_fd, char *buf, size_t buf_size)
{
  PluginOption *option = thread_context->option;

  int proxy_header_len = generate_proxy_header(buf, buf_size, thread_context->index, option->proxy_src_ip,
                                               option->proxy_dst_ip, option->proxy_src_port, option->proxy_dst_port);

  DEBUG("Generated PROXY protocol v1 header; len=%d\n", proxy_header_len);

  size_t sent = 0;
  while (sent < proxy_header_len)
    {
      int rc = send(sock_fd, buf + sent, proxy_header_len - sent, 0);
      if (rc < 0)
        {
          ERROR("Error sending buffer on %d (rc=%d)\n", sock_fd, rc);
          return;
        }

      sent += rc;
    }

  DEBUG("Sent PROXY protocol v1 header; len=%d\n", proxy_header_len);
}

gpointer
active_thread_func(gpointer user_data)
{
  ThreadData *thread_context = (ThreadData *)user_data;
  PluginOption *option = thread_context->option;

  char *message = g_malloc0(MAX_MESSAGE_LENGTH+1);

  int sock_fd = connect_ip_socket(SOCK_STREAM, option->target, option->port, option->use_ipv6);

  if(proxied_tls_passthrough)
    send_plaintext_proxy_header(thread_context, sock_fd, message, MAX_MESSAGE_LENGTH);

  SSL *ssl = open_ssl_connection(sock_fd);

  if (ssl == NULL)
    {
      ERROR("can not connect to %s:%s (%p)\n", option->target, option->port, g_thread_self());
    }
  else
    {
      DEBUG("(%d) connected to server on socket (%p)\n", thread_context->index, g_thread_self());
    }

  g_mutex_lock(&thread_lock);
  connect_finished++;

  if (connect_finished == option->active_connections + option->idle_connections)
    g_cond_broadcast(&thread_connected);

  g_mutex_unlock(&thread_lock);

  DEBUG("thread (%s,%p) created. wait for start ...\n", ssl_loggen_plugin_info.name, g_thread_self());
  g_mutex_lock(&thread_lock);
  while (!thread_run)
    {
      g_cond_wait(&thread_start, &thread_lock);
    }
  g_mutex_unlock(&thread_lock);

  DEBUG("thread (%s,%p) started. (r=%d,c=%d)\n", ssl_loggen_plugin_info.name, g_thread_self(), option->rate,
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
          ERROR("generate_message not yet set up(%p)\n", g_thread_self());
          break;
        }

      int str_len = generate_message(message, MAX_MESSAGE_LENGTH, thread_context, count++);

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
              ERROR("error sending buffer on %p (rc=%zd)\n", ssl, rc);
              errno = ECONNABORTED;
              connection_error = TRUE;
              break;
            }
          sent += rc;
        }

      if(!connection_error)
        {
          thread_context->sent_messages++;
          thread_context->buckets--;
        }

      if(connection_error && option->reconnect)
        {
          close_ssl_connection(ssl);
          shutdown(sock_fd, SHUT_RDWR);
          close(sock_fd);

          ERROR("destination connection %s:%s (%p) is lost, try to reconnect\n", option->target, option->port, g_thread_self());
          sock_fd = connect_ip_socket(SOCK_STREAM, option->target, option->port, option->use_ipv6);
          if(proxied_tls_passthrough)
            send_plaintext_proxy_header(thread_context, sock_fd, message, MAX_MESSAGE_LENGTH);
          ssl = open_ssl_connection(sock_fd);

          while(ssl == NULL && !thread_check_exit_criteria(thread_context))
            {
              ERROR("can not reconnect to %s:%s (%p), try again after %d sec\n", option->target, option->port, g_thread_self(), 1);
              g_usleep(1e6);

              sock_fd = connect_ip_socket(SOCK_STREAM, option->target, option->port, option->use_ipv6);
              if(proxied_tls_passthrough)
                send_plaintext_proxy_header(thread_context, sock_fd, message, MAX_MESSAGE_LENGTH);
              ssl = open_ssl_connection(sock_fd);
            }

          if(ssl != NULL)
            {
              DEBUG("(%d) reconnected to server on socket (%p)\n", thread_context->index, g_thread_self());
              connection_error = FALSE;
            }
        }
    }
  DEBUG("thread (%s,%p) finished\n", ssl_loggen_plugin_info.name, g_thread_self());

  g_mutex_lock(&thread_lock);
  active_thread_count--;
  g_mutex_unlock(&thread_lock);

  g_free((gpointer)message);
  close_ssl_connection(ssl);
  shutdown(sock_fd, SHUT_RDWR);
  close(sock_fd);

  g_free(thread_context);
  g_thread_exit(NULL);
  return NULL;
}
