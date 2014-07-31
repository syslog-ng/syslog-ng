/*
 * Copyright (c) 2002-2013 BalaBit IT Ltd, Budapest, Hungary
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
#include "service-management.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#if ENABLE_SYSTEMD
#include <systemd/sd-daemon.h>
#include "messages.h"

void
service_management_publish_status(const gchar *status)
{
  gchar *status_buffer;
  time_t now = time(NULL);

  status_buffer = g_strdup_printf("STATUS=%s (%s)", status, ctime(&now));
  sd_notify(0, status_buffer);
  g_free(status_buffer);
}

void
service_management_clear_status(void)
{
  sd_notify(0, "STATUS=");
}

void
service_management_indicate_readiness(void)
{
  sd_notify(0, "READY=1");
}

static gboolean
service_management_systemd_is_used(void)
{
  int fd;

  fd = open("/run/systemd/system/", O_RDONLY);

  if (fd < 0)
  {
    msg_debug("Systemd is not detected as the running init system", NULL);
    return FALSE;
  }
  else
  {
    msg_debug("Systemd is detected as the running init system", NULL);
    close(fd);
    return TRUE;
  }
}

ServiceManagementType
service_management_get_type(void)
{
  if (service_management_systemd_is_used())
    return SMT_SYSTEMD;
  else
    return SMT_NONE;
}


#endif
