/*
 * Copyright (c) 2009-2014 Balabit
 * Copyright (c) 2009-2014 Balázs Scheidler <balazs.scheidler@balabit.com>
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

#include "syslog-ng.h"
#include "logmsg/logmsg.h"
#include "template/templates.h"
#include "apphook.h"
#include "cfg.h"
#include "timeutils.h"
#include "plugin.h"
#include "libtest/template_lib.h"
#include "libtest/stopwatch.h"

#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

gboolean success = TRUE;

int
main(int argc G_GNUC_UNUSED, char *argv[] G_GNUC_UNUSED)
{

  app_startup();

  init_template_tests();
  putenv("TZ=MET-1METDST");
  tzset();

  cfg_load_module(configuration, "syslogformat");
  cfg_load_module(configuration, "basicfuncs");

  perftest_template("$DATE\n");
  perftest_template("<$PRI>$DATE $HOST $MSGHDR$MSG\n");
  perftest_template("$DATE\n");
  perftest_template("$DATE $HOST\n");
  perftest_template("$DATE $HOST $MSGHDR\n");
  perftest_template("$DATE $HOST $MSGHDR$MSG\n");
  perftest_template("$DATE $HOST $MSGHDR$MSG value\n");
  perftest_template("$DATE $HOST $MSGHDR$MSG ${APP.VALUE}\n");
  perftest_template("$MSG\n");
  perftest_template("$TAGS\n");
  perftest_template("$(echo $MSG)\n");
  perftest_template("$(+ $FACILITY_NUM $FACILITY_NUM)\n");
  perftest_template("$DATE $FACILITY.$PRIORITY $HOST $MSGHDR$MSG $SEQNO\n");
  perftest_template("${APP.VALUE} ${APP.VALUE2}\n");
  perftest_template("$DATE ${HOST:--} ${PROGRAM:--} ${PID:--} ${MSGID:--} ${SDATA:--} $MSG\n");

  app_shutdown();

  if (success)
    return 0;
  return 1;
}
