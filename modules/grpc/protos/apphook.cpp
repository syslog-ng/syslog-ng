/*
 * Copyright (c) 2024 Axoflow
 * Copyright (c) 2024 László Várady
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

#include "apphook.h"

#include "syslog-ng.h"

#include "compat/cpp-start.h"
#include "lib/apphook.h"
#include "compat/cpp-end.h"

#include <google/protobuf/message_lite.h>

namespace {

void
grpc_global_init(gint type, gpointer user_data)
{

}

void
grpc_global_deinit(gint type, gpointer user_data)
{
  google::protobuf::ShutdownProtobufLibrary();
}

}

void
grpc_register_global_initializers(void)
{
  static gboolean initialized = FALSE;

  if (!initialized)
    {
      register_application_hook(AH_STARTUP, grpc_global_init, NULL, AHM_RUN_ONCE);
      register_application_hook(AH_SHUTDOWN, grpc_global_deinit, NULL, AHM_RUN_ONCE);
      initialized = TRUE;
    }
}

static void __attribute__((destructor))
_protobuf_cleanup(void)
{
  google::protobuf::ShutdownProtobufLibrary();
}
