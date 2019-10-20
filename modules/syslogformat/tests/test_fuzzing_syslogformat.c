/*
 * Copyright (c) 2021 Balabit
 * Copyright (c) 2021 Kokan
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

#include <stdint.h>
#include <stddef.h>

#include "apphook.h"
#include "msg-format.h"
#include "logmsg/logmsg.h"
#include "cfg.h"
#include "syslog-format.h"
#include <iv.h>


int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  if (size <= 1) return 0;

  app_startup();
  syslog_format_init();
  MsgFormatOptions parse_options;
  msg_format_options_defaults(&parse_options);

  LogMessage *msg = log_msg_new_empty();
  gsize problem_position = 0;

  syslog_format_handler(&parse_options, msg, data, size, &problem_position);

  log_msg_unref(msg);
  app_shutdown();
  iv_deinit();
  return 0;
}



