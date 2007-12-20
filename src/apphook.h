/*
 * Copyright (C) 2002-2007 BalaBit IT Ltd.
 * Copyright (C) 2002-2007 Balazs Scheidler
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Note that this permission is granted for only version 2 of the GPL.
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  
 */
  
#ifndef APPHOOK_H_INCLUDED
#define APPHOOK_H_INCLUDED

#include "syslog-ng.h"

/* this enum must be in the order the given events actually happen in time */
enum
{
  AH_STARTUP,
  AH_POST_DAEMONIZED,
  AH_SHUTDOWN
};

typedef void (*ApplicationHookFunc)(gint type, gpointer user_data);

void register_application_hook(gint type, ApplicationHookFunc func, gpointer user_data);
void app_startup();
void app_post_daemonized();
void app_shutdown();

#endif
