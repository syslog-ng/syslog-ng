/*
 * Copyright (c) 2002, 2003, 2004 BalaBit IT Ltd, Budapest, Hungary
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

#ifndef MESSAGES_H_INCLUDED
#define MESSAGES_H_INCLUDED

#include <syslog-ng.h>
#include <evtlog.h>

extern int debug_flag;
extern int verbose_flag;

#define msg_fatal(desc, tag1, tags...) msg_event_send(msg_event_create(EVT_PRI_CRIT, desc, tag1, ##tags ))
#define msg_error(desc, tag1, tags...) msg_event_send(msg_event_create(EVT_PRI_ERR, desc, tag1, ##tags ))
#define msg_notice(desc, tag1, tags...) msg_event_send(msg_event_create(EVT_PRI_NOTICE, desc, tag1, ##tags ))

#define msg_verbose(desc, tag1, tags...) \
	do { \
	  if (verbose_flag) \
	    msg_notice(desc, tag1, ##tags ); \
	} while (0)
	
#define msg_debug(desc, tag1, tags...) \
	do { \
	  if (debug_flag) \
	    msg_event_send(msg_event_create(EVT_PRI_DEBUG, desc, tag1, ##tags )); \
	} while (0)

EVTREC *msg_event_create(gint prio, const char *desc, EVTTAG *tag1, ...);
void msg_event_send(EVTREC *e);

void msg_syslog_started(void);

gboolean msg_init(int use_stderr);
void msg_deinit(void);

#endif
