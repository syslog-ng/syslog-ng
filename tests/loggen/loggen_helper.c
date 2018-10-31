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

#include "loggen_helper.h"

#include <syslog-ng-config.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <openssl/err.h>

static int debug = 0;

int
get_debug_level(void)
{
  return debug;
}

void
set_debug_level(int new_debug)
{
  debug = new_debug;
}

static int
connect_to_server(struct sockaddr *dest_addr, int dest_addr_len, int sock_type)
{
  int sock = socket(dest_addr->sa_family, sock_type, 0);
  if (sock < 0)
    {
      ERROR("error creating socket: %s\n", g_strerror(errno));
      return -1;
    }
  if (sock_type == SOCK_STREAM)
    {
#ifdef TCP_SYNCNT
      int synRetries = 2; /* to avoid long waiting for server connection */
      setsockopt(sock, IPPROTO_TCP, TCP_SYNCNT, &synRetries, sizeof(synRetries));
#endif
    }

  DEBUG("try to connect to server ...\n");
  if (connect(sock, dest_addr, dest_addr_len) < 0)
    {
      ERROR("error connecting socket: %s\n", g_strerror(errno));
      close(sock);
      return -1;
    }
  DEBUG("server connection established (%d)\n",sock);
  return sock;
}
int
connect_ip_socket(int sock_type, const char *target, const char *port, int use_ipv6)
{
  struct sockaddr *dest_addr;
  socklen_t dest_addr_len;

  if (!target || !port)
    {
      ERROR("Invalid server address/port\n");
      return -1;
    }

  DEBUG("server IP = %s:%s\n",target,port);
#if SYSLOG_NG_HAVE_GETADDRINFO
  struct addrinfo hints;
  struct addrinfo *res;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = use_ipv6 ? AF_INET6 : AF_INET;
  hints.ai_socktype = sock_type;
#ifdef AI_ADDRCONFIG
  hints.ai_flags = AI_ADDRCONFIG;
#endif
  hints.ai_protocol = 0;
  if (getaddrinfo(target, port, &hints, &res) != 0)
    {
      ERROR("name lookup error (%s:%s)\n",target, port);
      return -1;
    }

  dest_addr = res->ai_addr;
  dest_addr_len = res->ai_addrlen;
#else
  struct hostent *he;
  struct servent *se;
  static struct sockaddr_in s_in;

  he = gethostbyname(target);
  if (!he)
    {
      ERROR("name lookup error (%s)\n",target);
      return -1;
    }
  s_in.sin_family = AF_INET;
  s_in.sin_addr = *(struct in_addr *) he->h_addr;

  se = getservbyname(port, sock_type == SOCK_STREAM ? "tcp" : "udp");
  if (se)
    s_in.sin_port = se->s_port;
  else
    s_in.sin_port = htons(atoi(port));
  dest_addr = (struct sockaddr *) &s_in;
  dest_addr_len = sizeof(s_in);
#endif

  return connect_to_server(dest_addr, dest_addr_len, sock_type);
}

int connect_unix_domain_socket(int sock_type, const char *path)
{
  struct sockaddr_un saun;
  struct sockaddr *dest_addr;
  socklen_t dest_addr_len;

  if (!path)
    {
      ERROR("No target path specified\n");
      return -1;
    }

  DEBUG("unix domain socket: %s\n",path);
  saun.sun_family = AF_UNIX;

  gsize max_target_path_size = sizeof(saun.sun_path);
  if (strlen(path) >= max_target_path_size)
    {
      ERROR("Target path is too long; max_target_length=%" G_GSIZE_FORMAT "\n", max_target_path_size - 1);
      return -1;
    }

  strcpy(saun.sun_path, path);

  dest_addr = (struct sockaddr *) &saun;
  dest_addr_len = sizeof(saun);

  return connect_to_server(dest_addr, dest_addr_len, sock_type);
}

unsigned long
time_val_diff_in_usec(struct timeval *t1, struct timeval *t2)
{
  return (t1->tv_sec - t2->tv_sec) * USEC_PER_SEC + (t1->tv_usec - t2->tv_usec);
}

void
time_val_diff_in_timeval(struct timeval *res, const struct timeval *t1, const struct timeval *t2)
{
  res->tv_sec = (t1->tv_sec - t2->tv_sec);
  res->tv_usec = (t1->tv_usec - t2->tv_usec);
  if (res->tv_usec < 0)
    {
      res->tv_sec--;
      res->tv_usec += USEC_PER_SEC;
    }
}

double
time_val_diff_in_sec(struct timeval *t1, struct timeval *t2)
{
  struct timeval res;
  time_val_diff_in_timeval(&res,t1,t2);
  return (double)res.tv_sec + (double)res.tv_usec/USEC_PER_SEC;
}

size_t
get_now_timestamp(char *stamp, gsize stamp_size)
{
  struct timeval now;
  struct tm;

  gettimeofday(&now, NULL);
  localtime_r(&now.tv_sec, &tm);
  return strftime(stamp, stamp_size, "%Y-%m-%dT%H:%M:%S", &tm);
}

size_t
get_now_timestamp_bsd(char *stamp, gsize stamp_size)
{
  struct timeval now;
  struct tm tm;

  gettimeofday(&now, NULL);
  localtime_r(&now.tv_sec, &tm);
  return strftime(stamp, stamp_size, "%b %d %T", &tm);
}

void
format_timezone_offset_with_colon(char *timestamp, int timestamp_size, struct tm *tm)
{
  char offset[7];
  int len = strftime(offset, sizeof(offset), "%z", tm);

  memmove(&offset[len - 1], &offset[len - 2], 3);
  offset[len - 2] = ':';

  strncat(timestamp, offset, timestamp_size - strlen(timestamp) -1);
}

SSL *
open_ssl_connection(int sock_fd)
{
  SSL_CTX *ctx = NULL;
  if (NULL == (ctx = SSL_CTX_new(SSLv23_client_method())))
    {
      ERROR("error creating SSL_CTX\n");
      return NULL;
    }

  SSL *ssl = NULL;
  if (NULL == (ssl = SSL_new(ctx)))
    {
      ERROR("error creating SSL\n");
      return NULL;
    }

  SSL_set_fd (ssl, sock_fd);
  if (-1 == SSL_connect(ssl))
    {
      ERROR("SSL connect failed\n");
      ERR_print_errors_fp(stderr);
      return NULL;
    }

  DEBUG("SSL connection established\n");
  return ssl;
}
void
close_ssl_connection(SSL *ssl)
{
  if (!ssl)
    {
      DEBUG("SSL connection was not initialized\n");
      return;
    }

  SSL_shutdown(ssl);
  SSL_CTX_free(SSL_get_SSL_CTX(ssl));
  SSL_free(ssl);

  DEBUG("SSL connection closed\n");
}
