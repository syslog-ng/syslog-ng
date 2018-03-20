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

#ifndef __HELPER_H__
#define __HELPER_H__

#include "compat/compat.h"
#include "compat/glib.h"

#include <glib.h>
#include <sys/un.h>
#include <netdb.h>
#include <openssl/ssl.h>

#include <libgen.h>

int get_debug_level(void);
void set_debug_level(int new_debug);
unsigned long time_val_diff_in_usec(struct timeval *t1, struct timeval *t2);
double time_val_diff_in_sec(struct timeval *t1, struct timeval *t2);
void time_val_diff_in_timeval(struct timeval *res, const struct timeval *t1, const struct timeval *t2);
size_t get_now_timestamp(char *stamp, gsize stamp_size);
void format_timezone_offset_with_colon(char *timestamp, int timestamp_size, struct tm *tm);
int connect_ip_socket(int sock_type, const char *target, const char *port, int use_ipv6);
int connect_unix_domain_socket(int sock_type, const char *path);
SSL *open_ssl_connection(int sock_fd);
void close_ssl_connection(SSL *ssl);

#define MAX_MESSAGE_LENGTH 8192
#define USEC_PER_SEC      1000000
#define CONNECTION_TIMEOUT_SEC 5

#define ERROR(format,...) do{\
  fprintf(stderr,"error [%s:%s:%d] ",basename(__FILE__),__func__,__LINE__);\
  fprintf(stderr,format,##__VA_ARGS__);\
}while(0)

/* debug messages can be turned on by "--debug" command line option */
#define DEBUG(format,...) do{\
  if (!get_debug_level()) \
    break; \
  fprintf(stdout,"debug [%s:%s:%d] ",basename(__FILE__),__func__,__LINE__); \
  fprintf(stdout,format,##__VA_ARGS__);\
}while(0)

#endif
