/*
 * Copyright (c) 2002-2013 Balabit
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
#include "messages.h"

#if SYSLOG_NG_ENABLE_SYSTEMD

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <systemd/sd-daemon.h>

#endif

typedef struct _ServiceManagement ServiceManagement;

struct _ServiceManagement
{
  ServiceManagementType type;
  void (*publish_status)(const gchar *status);
  void (*clear_status)(void);
  void (*indicate_readiness)(void);
  gboolean (*is_active)(void);
};

ServiceManagement *current_service_mgmt = NULL;

#if SYSLOG_NG_ENABLE_SYSTEMD

static inline void
service_management_systemd_publish_status(const gchar *status)
{
  gchar *status_buffer;
  time_t now = time(NULL);

  status_buffer = g_strdup_printf("STATUS=%s (%s)", status, ctime(&now));
  sd_notify(0, status_buffer);
  g_free(status_buffer);
}

static inline void
service_management_systemd_clear_status(void)
{
  sd_notify(0, "STATUS=");
}

static inline void
service_management_systemd_indicate_readiness(void)
{
  sd_notify(0, "READY=1");
}

static gboolean
service_management_systemd_is_active(void)
{
  struct stat st;

  if (lstat("/run/systemd/system/", &st) < 0 || !S_ISDIR(st.st_mode))
    {
      msg_debug("Systemd is not detected as the running init system");
      return FALSE;
    }
  else
    {
      msg_debug("Systemd is detected as the running init system");
      return TRUE;
    }
}

#endif

void
service_management_publish_status(const gchar *status)
{
  current_service_mgmt->publish_status(status);
}

void
service_management_clear_status(void)
{
  current_service_mgmt->clear_status();
}

void
service_management_indicate_readiness(void)
{
  current_service_mgmt->indicate_readiness();
}

ServiceManagementType
service_management_get_type(void)
{
  return current_service_mgmt->type;
}

static inline void
service_management_dummy_publish_status(const gchar *status)
{
}

static inline void
service_management_dummy_clear_status(void)
{
}

static inline void
service_management_dummy_indicate_readiness(void)
{
}

static gboolean
service_management_dummy_is_active(void)
{
  return TRUE;
}

ServiceManagement service_managements[] =
{
  {
    .type = SMT_NONE,
    .publish_status = service_management_dummy_publish_status,
    .clear_status = service_management_dummy_clear_status,
    .indicate_readiness = service_management_dummy_indicate_readiness,
    .is_active = service_management_dummy_is_active
  },
#if SYSLOG_NG_ENABLE_SYSTEMD
  {
    .type = SMT_SYSTEMD,
    .publish_status = service_management_systemd_publish_status,
    .clear_status = service_management_systemd_clear_status,
    .indicate_readiness = service_management_systemd_indicate_readiness,
    .is_active = service_management_systemd_is_active
  }
#endif
};

void
service_management_init(void)
{
  gint i = 0;
  while (i < sizeof(service_managements) / sizeof(ServiceManagement))
    {
      if (service_managements[i].is_active())
        current_service_mgmt = &service_managements[i];
      i++;
    }
}
