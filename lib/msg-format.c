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

#include "msg-format.h"
#include "cfg.h"
#include "plugin.h"

void
msg_format_options_defaults(MsgFormatOptions *options)
{
  options->flags = LP_EXPECT_HOSTNAME | LP_STORE_LEGACY_MSGHDR;
  options->recv_time_zone = NULL;
  options->recv_time_zone_info = NULL;
  options->bad_hostname = NULL;
  options->default_pri = 0xFFFF;
  options->sdata_param_value_max = 65535;
}

/*
 * NOTE: options_init and options_destroy are a bit weird, because their
 * invocation is not completely symmetric:
 *
 *   - init is called from driver init (e.g. affile_sd_init),
 *   - destroy is called from driver free method (e.g. affile_sd_free, NOT affile_sd_deinit)
 *
 * The reason:
 *   - when initializing the reloaded configuration fails for some reason,
 *     we have to fall back to the old configuration, thus we cannot dump
 *     the information stored in the Options structure.
 *
 * For the reasons above, init and destroy behave the following way:
 *
 *   - init is idempotent, it can be called multiple times without leaking
 *     memory, and without loss of information
 *   - destroy is only called once, when the options are indeed to be destroyed
 *
 * As init allocates memory, it has to take care about freeing memory
 * allocated by the previous init call (or it has to reuse those).
 *
 */
void
msg_format_options_init(MsgFormatOptions *options, GlobalConfig *cfg)
{
  gchar *recv_time_zone, *format;
  TimeZoneInfo *recv_time_zone_info;
  MsgFormatHandler *format_handler;
  Plugin *p;

  recv_time_zone = options->recv_time_zone;
  options->recv_time_zone = NULL;
  recv_time_zone_info = options->recv_time_zone_info;
  options->recv_time_zone_info = NULL;
  format = options->format;
  options->format = NULL;
  format_handler = options->format_handler;
  options->format_handler = NULL;

  msg_format_options_destroy(options);

  options->format = format;
  options->format_handler = format_handler;
  options->recv_time_zone = recv_time_zone;
  options->recv_time_zone_info = recv_time_zone_info;

  if (cfg->bad_hostname_compiled)
    options->bad_hostname = &cfg->bad_hostname;
  if (options->recv_time_zone == NULL)
    options->recv_time_zone = g_strdup(cfg->recv_time_zone);
  if (options->recv_time_zone_info == NULL)
    options->recv_time_zone_info = time_zone_info_new(options->recv_time_zone);

  if (!options->format)
    options->format = g_strdup("syslog");

  p = plugin_find(cfg, LL_CONTEXT_FORMAT, options->format);
  if (p)
    options->format_handler = plugin_construct(p, cfg, LL_CONTEXT_FORMAT, options->format);
}

void
msg_format_options_destroy(MsgFormatOptions *options)
{
  if (options->format)
    {
      g_free(options->format);
      options->format = NULL;
    }
  if (options->recv_time_zone)
    {
      g_free(options->recv_time_zone);
      options->recv_time_zone = NULL;
    }
  if (options->recv_time_zone_info)
    {
      time_zone_info_free(options->recv_time_zone_info);
      options->recv_time_zone_info = NULL;
    }
}
