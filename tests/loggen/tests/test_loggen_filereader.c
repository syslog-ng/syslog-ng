/*
 * Copyright (c) 2007-2018 Balabit
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

#include "tests/loggen/file_reader.c"

#include <criterion/criterion.h>
#include <criterion/parameterized.h>

typedef struct _filereader_test_param
{
  int   skip_tokens;
  const gchar *line;
  const gchar *expected_pri;
  const gchar *expected_ver;
  const gchar *expected_time_stamp;
  const gchar *expected_host;
  const gchar *expected_app;
  const gchar *expected_pid;
  const gchar *expected_msgid;
  const gchar *expected_sdata;
  const gchar *expected_message;
} FileReaderTestParam;

ParameterizedTestParameters(loggen, test_filereader)
{
  static FileReaderTestParam parser_params[] =
  {
    /* RFC5424 log format */
    {
      .skip_tokens = 0,
      .line = "<134>1 2011-02-01T15:25:25+01:00 zts-win001 syslog-ng_Agent 3268 - [meta sequenceId=\"5\" sysUpTime=\"6\"][origin ip=\"zts-win001\" software=\"syslog-ng Agent\"][win@18372.4 EVENT_ID=\"17\" EVENT_NAME=\"Application\" EVENT_REC_NUM=\"2515\" EVENT_SID=\"S-1-5-21-1044605109-3424919905-550079122-1000\" EVENT_SID_TYPE=\"User\" EVENT_SOURCE=\"syslog-ng Agent\" EVENT_TYPE=\"Information\" EVENT_USERNAME=\"ZTS-WIN001\\balabit\"] ﻿ZTS-WIN001\balabit: Application syslog-ng Agent: [Information] Service Removed (EventID 17)",
      .expected_pri = "134",
      .expected_ver = "1",
      .expected_time_stamp = "2011-02-01T15:25:25+01:00",
      .expected_host = "zts-win001",
      .expected_app = "syslog-ng_Agent",
      .expected_pid = "3268",
      .expected_msgid = "-",
      .expected_sdata = "[meta sequenceId=\"5\" sysUpTime=\"6\"][origin ip=\"zts-win001\" software=\"syslog-ng Agent\"][win@18372.4 EVENT_ID=\"17\" EVENT_NAME=\"Application\" EVENT_REC_NUM=\"2515\" EVENT_SID=\"S-1-5-21-1044605109-3424919905-550079122-1000\" EVENT_SID_TYPE=\"User\" EVENT_SOURCE=\"syslog-ng Agent\" EVENT_TYPE=\"Information\" EVENT_USERNAME=\"ZTS-WIN001\\balabit\"]",
      .expected_message = "ZTS-WIN001\balabit: Application syslog-ng Agent: [Information] Service Removed (EventID 17)"
    },
    {
      .skip_tokens = 0,
      .line = "<134>1 2016-04-15T11:00:06+02:00 zts-win101 Microsoft-Windows-Service_Control_Manager 3748 1234 [win@18372.4 EVENT_FACILITY=\"16\" EVENT_ID=\"7036\" EVENT_LEVEL=\"4\" EVENT_NAME=\"System\" EVENT_REC_NUM=\"471252\" EVENT_SID=\"N/A\" EVENT_SOURCE=\"Microsoft-Windows-Service Control Manager\" EVENT_TYPE=\"Information\" EVENT_CATEGORY=\"None\"][meta sequenceId=\"397\" sysUpTime=\"85\"] ﻿N/A: System Microsoft-Windows-Service Control Manager: [Information] The syslog-ng Agent service entered the running state. (EventID 7036)",
      .expected_pri = "134",
      .expected_ver = "1",
      .expected_time_stamp = "2016-04-15T11:00:06+02:00",
      .expected_host = "zts-win101",
      .expected_app = "Microsoft-Windows-Service_Control_Manager",
      .expected_pid = "3748",
      .expected_msgid = "1234",
      .expected_sdata = "[win@18372.4 EVENT_FACILITY=\"16\" EVENT_ID=\"7036\" EVENT_LEVEL=\"4\" EVENT_NAME=\"System\" EVENT_REC_NUM=\"471252\" EVENT_SID=\"N/A\" EVENT_SOURCE=\"Microsoft-Windows-Service Control Manager\" EVENT_TYPE=\"Information\" EVENT_CATEGORY=\"None\"][meta sequenceId=\"397\" sysUpTime=\"85\"]",
      .expected_message = "N/A: System Microsoft-Windows-Service Control Manager: [Information] The syslog-ng Agent service entered the running state. (EventID 7036)"
    },
    {
      .skip_tokens = 0,
      .line = "<01>32 2016-04-15T11:00:06+03:00 zts-win101 loggen 12 34 - test message for loggen",
      .expected_pri = "01",
      .expected_ver = "32",
      .expected_time_stamp = "2016-04-15T11:00:06+03:00",
      .expected_host = "zts-win101",
      .expected_app = "loggen",
      .expected_pid = "12",
      .expected_msgid = "34",
      .expected_sdata = "-",
      .expected_message = "test message for loggen"
    },
    {
      .skip_tokens = 0,
      .line = "<01>32 2016-04-15T11:00:06+04:00 - - - - - test message for loggen",
      .expected_pri = "01",
      .expected_ver = "32",
      .expected_time_stamp = "2016-04-15T11:00:06+04:00",
      .expected_host = "-",
      .expected_app = "-",
      .expected_pid = "-",
      .expected_msgid = "-",
      .expected_sdata = "-",
      .expected_message = "test message for loggen"
    },
    {
      .skip_tokens = 0,
      .line = "<01>32 2016-04-15T11:00:06+05:00 - - - - [sdata_test=\"1\"] ﻿test message for loggen",
      .expected_pri = "01",
      .expected_ver = "32",
      .expected_time_stamp = "2016-04-15T11:00:06+05:00",
      .expected_host = "-",
      .expected_app = "-",
      .expected_pid = "-",
      .expected_msgid = "-",
      .expected_sdata = "[sdata_test=\"1\"]",
      .expected_message = "test message for loggen"
    },
    {
      .skip_tokens = 0,
      .line = "<01>32 2016-04-15T11:00:07+02:00 - - - - [sdata_test=\"1\"] ﻿",
      .expected_pri = "01",
      .expected_ver = "32",
      .expected_time_stamp = "2016-04-15T11:00:07+02:00",
      .expected_host = "-",
      .expected_app = "-",
      .expected_pid = "-",
      .expected_msgid = "-",
      .expected_sdata = "[sdata_test=\"1\"]",
      .expected_message = ""
    },
    {
      .skip_tokens = 0,
      .line = "<01>32 2016-04-15T11:00:08+02:00 - - - - -",
      .expected_pri = "01",
      .expected_ver = "32",
      .expected_time_stamp = "2016-04-15T11:00:08+02:00",
      .expected_host = "-",
      .expected_app = "-",
      .expected_pid = "-",
      .expected_msgid = "-",
      .expected_sdata = "-",
      .expected_message = ""
    },
    {
      .skip_tokens = 0,
      .line = "<123>3210 2016-04-15T11:00:09+02:00 - - - - -",
      .expected_pri = "123",
      .expected_ver = "32", /* ver shall be truncated to 2 char long */
      .expected_time_stamp = "2016-04-15T11:00:09+02:00",
      .expected_host = "-",
      .expected_app = "-",
      .expected_pid = "-",
      .expected_msgid = "-",
      .expected_sdata = "-",
      .expected_message = ""
    },
    {
      /* invalid sdata */
      .skip_tokens = 0,
      .line = "<134>1 2016-04-15T14:00:06+02:00 zts-win101 Microsoft-Windows-Service_Control_Manager 3748 - [win@18372.4 EVENT_FACILITY=16 EVENT_ID=7036 EVENT_LEVEL=4 EVENT_NAME=System EVENT_REC_NUM=471252 EVENT_SID=N/Agent service entered the running state. (EventID 7036)\n",
      .expected_pri = "134",
      .expected_ver = "1", /* ver shall be truncated to 2 char long */
      .expected_time_stamp = "2016-04-15T14:00:06+02:00",
      .expected_host = "zts-win101",
      .expected_app = "Microsoft-Windows-Service_Control_Manager",
      .expected_pid = "3748",
      .expected_msgid = "-",
      .expected_sdata = "-",
      .expected_message = ""
    },
    /* RFC3164 log format */
    {
      .skip_tokens = 1,
      .line = "499 <134>Feb 04 16:22:31 zts-win001 Microsoft-Windows-Winlogon[3720]: : Application Microsoft-Windows-Winlogon: [Information] Windows license validated. (EventID 4101)",
      .expected_pri = "134",
      .expected_ver = "",
      .expected_time_stamp = "Feb 04 16:22:31",
      .expected_host = "zts-win001",
      .expected_app = "Microsoft-Windows-Winlogon",
      .expected_pid = "3720",
      .expected_msgid = "",
      .expected_sdata = "",
      .expected_message = ": Application Microsoft-Windows-Winlogon: [Information] Windows license validated. (EventID 4101)"
    },
    {
      .skip_tokens = 0,
      .line = "<134>Jul 27 14:55:12 syslogng-win7 Microsoft-Windows-Service_Control_Manager[3720]: : System Microsoft-Windows-Service Control Manager: [Information] The Multimedia Class Scheduler service entered the running state. (EventID 7036)",
      .expected_pri = "134",
      .expected_ver = "",
      .expected_time_stamp = "Jul 27 14:55:12",
      .expected_host = "syslogng-win7",
      .expected_app = "Microsoft-Windows-Service_Control_Manager",
      .expected_pid = "3720",
      .expected_msgid = "",
      .expected_sdata = "",
      .expected_message = ": System Microsoft-Windows-Service Control Manager: [Information] The Multimedia Class Scheduler service entered the running state. (EventID 7036)"
    },
    {
      .skip_tokens = 0,
      .line = "<34>Oct 11 22:14:15 mymachine su[123]: : 'su root' failed for lonvick on /dev/pts/8",
      .expected_pri = "34",
      .expected_ver = "",
      .expected_time_stamp = "Oct 11 22:14:15",
      .expected_host = "mymachine",
      .expected_app = "su",
      .expected_pid = "123",
      .expected_msgid = "",
      .expected_sdata = "",
      .expected_message = ": 'su root' failed for lonvick on /dev/pts/8"
    },
    {
      .skip_tokens = 0,
      .line = "<13>Feb  5 17:32:18 10.0.0.99 myapp[222]: : Use the BFG!",
      .expected_pri = "13",
      .expected_ver = "",
      .expected_time_stamp = "Feb  5 17:32:18",
      .expected_host = "10.0.0.99",
      .expected_app = "myapp",
      .expected_pid = "222",
      .expected_msgid = "",
      .expected_sdata = "",
      .expected_message = ": Use the BFG!"
    },
    {
      .skip_tokens = 0,
      .line = "<13>Feb  5 17:32:18 10.0.0.99 testapp: : Use the BFG!",
      .expected_pri = "13",
      .expected_ver = "",
      .expected_time_stamp = "Feb  5 17:32:18",
      .expected_host = "10.0.0.99",
      .expected_app = "testapp",
      .expected_pid = "",
      .expected_msgid = "",
      .expected_sdata = "",
      .expected_message = ": Use the BFG!"
    },
    {
      .skip_tokens = 0,
      .line = "<165>Aug 24 05:34:00 mymachine myproc[10]: : %% It's time to make the do-nuts. []:#!@?<> %%  Ingredients: Mix=OK, Jelly=OK # Devices: Mixer=OK, Jelly_Injector=OK, Frier=OK # Transport: Conveyer1=OK, Conveyer2=OK # %%",
      .expected_pri = "165",
      .expected_ver = "",
      .expected_time_stamp = "Aug 24 05:34:00",
      .expected_host = "mymachine",
      .expected_app = "myproc",
      .expected_pid = "10",
      .expected_msgid = "",
      .expected_sdata = "",
      .expected_message = ": %% It's time to make the do-nuts. []:#!@?<> %%  Ingredients: Mix=OK, Jelly=OK # Devices: Mixer=OK, Jelly_Injector=OK, Frier=OK # Transport: Conveyer1=OK, Conveyer2=OK # %%"
    },
    {
      .skip_tokens = 0,
      .line = "<13>Feb  5 17:32:18 10.0.0.99 Use the BFG 1!",
      .expected_pri = "13",
      .expected_ver = "",
      .expected_time_stamp = "Feb  5 17:32:18",
      .expected_host = "10.0.0.99",
      .expected_app = "",
      .expected_pid = "",
      .expected_msgid = "",
      .expected_sdata = "",
      .expected_message = "Use the BFG 1!"
    },
    {
      .skip_tokens = 0,
      .line = "<13>Feb  5 17:32:18 10.0.0.99 testapp: Use the BFG 2!",
      .expected_pri = "13",
      .expected_ver = "",
      .expected_time_stamp = "Feb  5 17:32:18",
      .expected_host = "10.0.0.99",
      .expected_app = "testapp",
      .expected_pid = "",
      .expected_msgid = "",
      .expected_sdata = "",
      .expected_message = "Use the BFG 2!"
    },
    {
      .skip_tokens = 0,
      .line = "<34>Oct 11 22:14:15 mymachine su[123]: 'su root' failed for lonvick on /dev/pts/8",
      .expected_pri = "34",
      .expected_ver = "",
      .expected_time_stamp = "Oct 11 22:14:15",
      .expected_host = "mymachine",
      .expected_app = "su",
      .expected_pid = "123",
      .expected_msgid = "",
      .expected_sdata = "",
      .expected_message = "'su root' failed for lonvick on /dev/pts/8"
    },
    {
      .skip_tokens = 0,
      .line = "<34>Oct 11 22:14:15 mymachine",
      .expected_pri = "34",
      .expected_ver = "",
      .expected_time_stamp = "Oct 11 22:14:15",
      .expected_host = "mymachine",
      .expected_app = "",
      .expected_pid = "",
      .expected_msgid = "",
      .expected_sdata = "",
      .expected_message = ""
    }
  };

  return cr_make_param_array(FileReaderTestParam, parser_params, G_N_ELEMENTS(parser_params));
}

ParameterizedTest(FileReaderTestParam *param, loggen, test_filereader)
{
  SyslogMsgElements elements;

  parse_line(str_skip_tokens(param->line, param->skip_tokens), &elements);

  cr_expect_str_eq(elements.pri, param->expected_pri, "Error: pri doesn't match (val=\"%s\", expected=\"%s\")\n",
                   elements.pri, param->expected_pri);
  cr_expect_str_eq(elements.ver, param->expected_ver, "Error: ver doesn't match (val=\"%s\", expected=\"%s\")\n",
                   elements.ver, param->expected_ver);
  cr_expect_str_eq(elements.time_stamp, param->expected_time_stamp,
                   "Error: time_stamp doesn't match (val=\"%s\", expected=\"%s\")\n",elements.time_stamp, param->expected_time_stamp);
  cr_expect_str_eq(elements.host, param->expected_host, "Error: host doesn't match (val=\"%s\", expected=\"%s\")\n",
                   elements.host, param->expected_host);
  cr_expect_str_eq(elements.app, param->expected_app, "Error: app doesn't match (val=\"%s\", expected=\"%s\")\n",
                   elements.app, param->expected_app);
  cr_expect_str_eq(elements.pid, param->expected_pid, "Error: pid doesn't match (val=\"%s\", expected=\"%s\")\n",
                   elements.pid, param->expected_pid);
  cr_expect_str_eq(elements.msgid, param->expected_msgid, "Error: msgid doesn't match (val=\"%s\", expected=\"%s\")\n",
                   elements.msgid, param->expected_msgid);
  cr_expect_str_eq(elements.sdata, param->expected_sdata, "Error: sdata doesn't match (val=\"%s\", expected=\"%s\")\n",
                   elements.sdata, param->expected_sdata);
  cr_expect_str_eq(elements.message, param->expected_message,
                   "Error: message doesn't match (val=\"%s\", expected=\"%s\")\n",elements.message, param->expected_message);
}

