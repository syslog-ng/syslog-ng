/*
 * Copyright (c) 2008-2015 Balabit
 * Copyright (c) 2008-2015 Balázs Scheidler <balazs.scheidler@balabit.com>
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

#include "csvparser.h"

#include "syslog-ng.h"
#include "logmsg/logmsg.h"
#include "apphook.h"
#include "string-list.h"
#include "cfg.h"
#include "plugin.h"

#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>


#define TEST_ASSERT(x, desc)    \
  do        \
    {         \
        if (!(x))   \
          {     \
            fprintf(stderr, "Testcase failed: %s; msg='%s', cond='%s', value='%s', expected_value='%s'\n", desc, msg, #x, value, expected_value); \
            exit(1);    \
          }     \
    }       \
  while (0)

MsgFormatOptions parse_options;

int
testcase(gchar *msg, guint parse_flags, gint max_columns, gint dialect, guint32 flags, gchar *delimiters, gchar *quotes,
         gchar *null_value, const gchar *string_delims[], gchar *first_value, ...)
{
  LogMessage *logmsg;
  LogParser *p, *pclone;
  gchar *expected_value;
  gint i;
  va_list va;
  NVTable *nvtable;

  const gchar *column_array[] =
  {
    "C1",
    "C2",
    "C3",
    "C4",
    "C5",
    "C6",
    "C7",
    "C8",
    "C9",
    "C10",
    "C11",
    "C12",
    "C13",
    "C14",
    "C15",
    "C16",
    "C17",
    "C18",
    "C19",
    "C20",
    "C21",
    "C22",
    "C23",
    "C24",
    "C25",
    "C26",
    "C27",
    "C28",
    "C29",
    "C30",
    NULL
  };
  gboolean success;

  if (max_columns != -1)
    {
      g_assert(max_columns < (sizeof(column_array) / sizeof(column_array[0])));

      column_array[max_columns] = NULL;
    }

  parse_options.flags = parse_flags;
  logmsg = log_msg_new(msg, strlen(msg), NULL, &parse_options);

  p = csv_parser_new(NULL);
  csv_scanner_options_set_flags(csv_parser_get_scanner_options(p), flags);
  csv_scanner_options_set_dialect(csv_parser_get_scanner_options(p), dialect);
  csv_scanner_options_set_columns(csv_parser_get_scanner_options(p), string_array_to_list(column_array));
  if (delimiters)
    csv_scanner_options_set_delimiters(csv_parser_get_scanner_options(p), delimiters);
  if (quotes)
    csv_scanner_options_set_quote_pairs(csv_parser_get_scanner_options(p), quotes);
  if (null_value)
    csv_scanner_options_set_null_value(csv_parser_get_scanner_options(p), null_value);

  if (string_delims)
    csv_scanner_options_set_string_delimiters(csv_parser_get_scanner_options(p), string_array_to_list(string_delims));

  pclone = (LogParser *) log_pipe_clone(&p->super);
  log_pipe_unref(&p->super);

  nvtable = nv_table_ref(logmsg->payload);
  success = log_parser_process(pclone, &logmsg, NULL, log_msg_get_value(logmsg, LM_V_MESSAGE, NULL), -1);
  nv_table_unref(nvtable);

  if (success && !first_value)
    {
      fprintf(stderr, "unexpected match; msg=%s\n", msg);
      exit(1);
    }
  if (!success && first_value)
    {
      fprintf(stderr, "unexpected non-match; msg=%s\n", msg);
      exit(1);
    }
  log_pipe_unref(&pclone->super);

  va_start(va, first_value);
  expected_value = first_value;
  i = 0;
  while (expected_value && column_array[i])
    {
      const gchar *value;
      gssize value_len;

      value = log_msg_get_value_by_name(logmsg, column_array[i], &value_len);

      if (expected_value && expected_value[0])
        {
          TEST_ASSERT(value && value[0], "expected value set, but no actual value");
          TEST_ASSERT(strlen(expected_value) == value_len, "value length doesn't match actual length");
          TEST_ASSERT(strncmp(value, expected_value, value_len) == 0, "value does not match expected value");
        }
      else
        {
          TEST_ASSERT(!(value && value[0]), "expected unset, but actual value present");
        }

      expected_value = va_arg(va, char *);
      i++;
    }

  log_msg_unref(logmsg);
  return 1;
}

int
main(int argc G_GNUC_UNUSED, char *argv[] G_GNUC_UNUSED)
{
  app_startup();

  putenv("TZ=MET-1METDST");
  tzset();

  configuration = cfg_new_snippet();
  cfg_load_module(configuration, "syslogformat");
  msg_format_options_defaults(&parse_options);
  msg_format_options_init(&parse_options, configuration);

  // string delim & single char & a char benne van a stringben is
  const char *string_delims[] = {" :", NULL};
  testcase("<15> openvpn[2499]: PTHREAD support :initialized", 0, -1, CSV_SCANNER_ESCAPE_NONE, 0, " ", NULL, NULL,
           string_delims,
           "PTHREAD", "support", "initialized", NULL);

  // string delim & single char & a char nincs benne a stringben
  testcase("<15> openvpn[2499]: PTHREAD,support :initialized", 0, -1, CSV_SCANNER_ESCAPE_NONE, 0, ",", NULL, NULL,
           string_delims,
           "PTHREAD", "support", "initialized", NULL);

  // string delim & multi char & a char benne van a stringben is
  testcase("<15> openvpn[2499]: PTHREAD support :initialized", 0, -1, CSV_SCANNER_ESCAPE_NONE, 0, " :", NULL, NULL,
           string_delims,
           "PTHREAD", "support", "initialized", NULL);

  // string delim & multi char & a char nincs benne a stringben
  testcase("<15> openvpn[2499]: PTHREAD,support :initialized", 0, -1, CSV_SCANNER_ESCAPE_NONE, 0, ";,", NULL, NULL,
           string_delims,
           "PTHREAD", "support", "initialized", NULL);

  // quote + string delim & multi char & a char benne van a stringben is
  testcase("<15> openvpn[2499]: 'PTHREAD' 'support' :'initialized'", 0, -1, CSV_SCANNER_ESCAPE_NONE, 0, " :", "''", NULL,
           string_delims,
           "PTHREAD", "support", "initialized", NULL);

  // quote + string delim & multi char & a char nincs benne a stringben
  testcase("<15> openvpn[2499]: 'PTHREAD','support' :'initialized'", 0, -1, CSV_SCANNER_ESCAPE_NONE, 0, ";,", "''", NULL,
           string_delims,
           "PTHREAD", "support", "initialized", NULL);

  // BE + quote + string delim & multi char & a char benne van a stringben is
  testcase("<15> openvpn[2499]: 'PTHRE\\\'AD' 'support' :'initialized'", 0, -1, CSV_SCANNER_ESCAPE_BACKSLASH, 0, " :",
           "''", NULL, string_delims,
           "PTHRE'AD", "support", "initialized", NULL);

  // DCE + quote + string delim & multi char & a char nincs benne a stringben
  testcase("<15> openvpn[2499]: 'PTHREAD','sup''port' :'initialized'", 0, -1, CSV_SCANNER_ESCAPE_DOUBLE_CHAR, 0, ";,",
           "''", NULL, string_delims,
           "PTHREAD", "sup'port", "initialized", NULL);

  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, 3, CSV_SCANNER_ESCAPE_NONE, 0, " ", NULL, NULL, NULL,
           "PTHREAD", "support", "initialized", NULL);

  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, 2, CSV_SCANNER_ESCAPE_NONE, CSV_SCANNER_DROP_INVALID,
           " ", NULL, NULL, NULL,
           NULL);

  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, 2, CSV_SCANNER_ESCAPE_NONE, CSV_SCANNER_GREEDY, " ",
           NULL, NULL, NULL,
           "PTHREAD", "support initialized", NULL);

  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, -1, CSV_SCANNER_ESCAPE_NONE, 0, " ,;", NULL, NULL, NULL,
           "PTHREAD", "support", "initialized", NULL);

  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, -1, CSV_SCANNER_ESCAPE_NONE, 0, " ,;", NULL, "support",
           NULL,
           "PTHREAD", "", "initialized", NULL);

  testcase("<15> openvpn[2499]: \"PTHREAD\" \"support\" \"initialized\"", 0, -1, CSV_SCANNER_ESCAPE_NONE, 0, " ", NULL,
           NULL, NULL,
           "PTHREAD", "support", "initialized", NULL);

  //  testcase("<15> openvpn[2499]: \"PTHREAD\"+\"support\" \"initialized\"", 0, -1, CSV_SCANNER_ESCAPE_NONE, " ", NULL, NULL, NULL,
  //           "PTHREAD\"+\"support", "initialized", NULL);

  testcase("<15> openvpn[2499]: \"  PTHREAD  \" \" support\" \"initialized \"", 0, -1, CSV_SCANNER_ESCAPE_NONE,
           CSV_SCANNER_STRIP_WHITESPACE, " ", NULL, NULL, NULL,
           "PTHREAD", "support", "initialized", NULL);

  testcase("<15> openvpn[2499]: \"PTHREAD support\" \"initialized\"", 0, -1, CSV_SCANNER_ESCAPE_NONE, 0, " ", NULL, NULL,
           NULL,
           "PTHREAD support", "initialized", NULL);

  testcase("<15> openvpn[2499]: \"PTHREAD support initialized\"", 0, -1, CSV_SCANNER_ESCAPE_NONE, 0, " ", NULL, NULL,
           NULL,
           "PTHREAD support initialized", NULL);

  testcase("<15> openvpn[2499]: \"PTHREAD support initialized", 0, -1, CSV_SCANNER_ESCAPE_NONE, 0, " ", NULL, NULL, NULL,
           "PTHREAD support initialized", NULL);

  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, -1, CSV_SCANNER_ESCAPE_BACKSLASH, 0, " ", NULL, NULL,
           NULL,
           "PTHREAD", "support", "initialized", NULL);

  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, 2, CSV_SCANNER_ESCAPE_BACKSLASH, CSV_SCANNER_GREEDY, " ",
           NULL, NULL, NULL,
           "PTHREAD", "support initialized", NULL);

  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, -1, CSV_SCANNER_ESCAPE_BACKSLASH, 0, " ;,", NULL, NULL,
           NULL,
           "PTHREAD", "support", "initialized", NULL);

  testcase("<15> openvpn[2499]: \"PTHREAD\" \"support\" \"initialized\"", 0, -1, CSV_SCANNER_ESCAPE_BACKSLASH, 0, " ",
           NULL, NULL, NULL,
           "PTHREAD", "support", "initialized", NULL);

  testcase("<15> openvpn[2499]: \"PTHREAD\" \"support\" \"initialized\"", 0, 2, CSV_SCANNER_ESCAPE_BACKSLASH,
           CSV_SCANNER_GREEDY, " ", NULL, NULL, NULL,
           "PTHREAD", "\"support\" \"initialized\"", NULL);

  testcase("<15> openvpn[2499]: \"  PTHREAD \" \"  support\" \"initialized  \"", 0, -1, CSV_SCANNER_ESCAPE_BACKSLASH,
           CSV_SCANNER_STRIP_WHITESPACE, " ", NULL, NULL, NULL,
           "PTHREAD", "support", "initialized", NULL);

  testcase("<15> openvpn[2499]: \"PTHREAD support\" \"initialized\"", 0, -1, CSV_SCANNER_ESCAPE_BACKSLASH, 0, " ", NULL,
           NULL, NULL,
           "PTHREAD support", "initialized", NULL);

  testcase("<15> openvpn[2499]: \"PTHREAD \\\"support initialized\"", 0, -1, CSV_SCANNER_ESCAPE_BACKSLASH, 0, " ", NULL,
           NULL, NULL,
           "PTHREAD \"support initialized", NULL);

  testcase("<15> openvpn[2499]: \"PTHREAD support initialized", 0, -1, CSV_SCANNER_ESCAPE_BACKSLASH, 0, " ", NULL, NULL,
           NULL,
           "PTHREAD support initialized", NULL);

  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, -1, CSV_SCANNER_ESCAPE_DOUBLE_CHAR, 0, " ", NULL, NULL,
           NULL,
           "PTHREAD", "support", "initialized", NULL);

  testcase("<15> openvpn[2499]: \"PTHREAD\" \"support\" \"initialized\"", 0, -1, CSV_SCANNER_ESCAPE_DOUBLE_CHAR, 0, " ",
           NULL, NULL, NULL,
           "PTHREAD", "support", "initialized", NULL);

  testcase("<15> openvpn[2499]: \"PTHREAD\" \"support\" \"initialized\"", 0, 2, CSV_SCANNER_ESCAPE_DOUBLE_CHAR,
           CSV_SCANNER_GREEDY, " ", NULL, NULL, NULL,
           "PTHREAD", "\"support\" \"initialized\"", NULL);

  testcase("<15> openvpn[2499]: \"  PTHREAD \" \"  support\" \"initialized  \"", 0, -1, CSV_SCANNER_ESCAPE_DOUBLE_CHAR,
           CSV_SCANNER_STRIP_WHITESPACE, " ", NULL, NULL, NULL,
           "PTHREAD", "support", "initialized", NULL);

  testcase("<15> openvpn[2499]: \"  PTHREAD \" \"  support\" \"initialized  \"", 0, 2, CSV_SCANNER_ESCAPE_DOUBLE_CHAR,
           CSV_SCANNER_GREEDY + CSV_SCANNER_STRIP_WHITESPACE, " ", NULL, NULL, NULL,
           "PTHREAD", "\"  support\" \"initialized  \"", NULL);

  testcase("<15> openvpn[2499]: \"PTHREAD support\" \"initialized\"", 0, -1, CSV_SCANNER_ESCAPE_DOUBLE_CHAR, 0, " ", NULL,
           NULL, NULL,
           "PTHREAD support", "initialized", NULL);

  testcase("<15> openvpn[2499]: \"PTHREAD \"\"support initialized\"", 0, -1, CSV_SCANNER_ESCAPE_DOUBLE_CHAR, 0, " ", NULL,
           NULL, NULL,
           "PTHREAD \"support initialized", NULL);

  testcase("<15> openvpn[2499]: \"PTHREAD support initialized", 0, -1, CSV_SCANNER_ESCAPE_DOUBLE_CHAR, 0, " ", NULL, NULL,
           NULL,
           "PTHREAD support initialized", NULL);

  testcase("postfix/smtpd", LP_NOPARSE, 2, CSV_SCANNER_ESCAPE_NONE, CSV_SCANNER_GREEDY | CSV_SCANNER_DROP_INVALID, "/",
           NULL, NULL, NULL,
           "postfix", "smtpd", NULL);

  testcase("postfix", LP_NOPARSE, 3, CSV_SCANNER_ESCAPE_NONE, CSV_SCANNER_GREEDY | CSV_SCANNER_DROP_INVALID, "/", NULL,
           NULL, NULL,
           NULL);

  testcase("postfix/smtpd/ququ", LP_NOPARSE, 2, CSV_SCANNER_ESCAPE_NONE, CSV_SCANNER_GREEDY | CSV_SCANNER_DROP_INVALID,
           "/", NULL, NULL, NULL,
           "postfix", "smtpd/ququ", NULL);

  testcase("Jul 27 19:55:33 myhost zabbix: ZabbixConnector.log : 19:55:32,782 INFO  [Thread-2834]     - [ZabbixEventSyncCommand] Processing   message <?xml version=\"1.0\" encoding=\"UTF-8\"?>",
           LP_EXPECT_HOSTNAME, 2, CSV_SCANNER_ESCAPE_NONE, CSV_SCANNER_GREEDY, " ", NULL, NULL, NULL,
           "ZabbixConnector.log",
           ": 19:55:32,782 INFO  [Thread-2834]     - [ZabbixEventSyncCommand] Processing   message <?xml version=\"1.0\" encoding=\"UTF-8\"?>",
           NULL);

  testcase("10.100.20.1 - - [31/Dec/2007:00:17:10 +0100] \"GET /cgi-bin/bugzilla/buglist.cgi?keywords_type=allwords&keywords=public&format=simple HTTP/1.1\" 200 2708 \"-\" \"curl/7.15.5 (i4 86-pc-linux-gnu) libcurl/7.15.5 OpenSSL/0.9.8c zlib/1.2.3 libidn/0.6.5\" 2 bugzilla.balabit",
           LP_NOPARSE, -1, CSV_SCANNER_ESCAPE_BACKSLASH, 0, " ", "\"\"[]", "-", NULL,
           "10.100.20.1", "", "", "31/Dec/2007:00:17:10 +0100",
           "GET /cgi-bin/bugzilla/buglist.cgi?keywords_type=allwords&keywords=public&format=simple HTTP/1.1", "200", "2708", "",
           "curl/7.15.5 (i4 86-pc-linux-gnu) libcurl/7.15.5 OpenSSL/0.9.8c zlib/1.2.3 libidn/0.6.5", "2", "bugzilla.balabit",
           NULL);

  testcase("10.100.20.1 - - [31/Dec/2007:00:17:10 +0100] \"GET /cgi-bin/bugzilla/buglist.cgi?keywords_type=allwords&keywords=public&format=simple HTTP/1.1\" 200 2708 \"-\" \"curl/7.15.5 (i4 86-pc-linux-gnu) libcurl/7.15.5 OpenSSL/0.9.8c zlib/1.2.3 libidn/0.6.5\" 2 bugzilla.balabit",
           LP_NOPARSE, 11, CSV_SCANNER_ESCAPE_BACKSLASH, 0, " ", "\"\"[]", "-", NULL,
           "10.100.20.1", "", "", "31/Dec/2007:00:17:10 +0100",
           "GET /cgi-bin/bugzilla/buglist.cgi?keywords_type=allwords&keywords=public&format=simple HTTP/1.1", "200", "2708", "",
           "curl/7.15.5 (i4 86-pc-linux-gnu) libcurl/7.15.5 OpenSSL/0.9.8c zlib/1.2.3 libidn/0.6.5", "2", "bugzilla.balabit",
           NULL);

  testcase("10.100.20.1 - - [31/Dec/2007:00:17:10 +0100] \"GET /cgi-bin/bugzilla/buglist.cgi?keywords_type=allwords&keywords=public&format=simple HTTP/1.1\" 200 2708 \"-\" \"curl/7.15.5 (i4 86-pc-linux-gnu) libcurl/7.15.5 OpenSSL/0.9.8c zlib/1.2.3 libidn/0.6.5\" 2 bugzilla.balabit",
           LP_NOPARSE, 10, CSV_SCANNER_ESCAPE_BACKSLASH, 0, " ", "\"\"[]", "-", NULL,
           "10.100.20.1", "", "", "31/Dec/2007:00:17:10 +0100",
           "GET /cgi-bin/bugzilla/buglist.cgi?keywords_type=allwords&keywords=public&format=simple HTTP/1.1", "200", "2708", "",
           "curl/7.15.5 (i4 86-pc-linux-gnu) libcurl/7.15.5 OpenSSL/0.9.8c zlib/1.2.3 libidn/0.6.5", "2", NULL);

  testcase("10.100.20.1 - - [31/Dec/2007:00:17:10 +0100] \"GET /cgi-bin/bugzilla/buglist.cgi?keywords_type=allwords&keywords=public&format=simple HTTP/1.1\" 200 2708 \"-\" \"curl/7.15.5 (i4 86-pc-linux-gnu) libcurl/7.15.5 OpenSSL/0.9.8c zlib/1.2.3 libidn/0.6.5\" 2 bugzilla.balabit",
           LP_NOPARSE, 12, CSV_SCANNER_ESCAPE_BACKSLASH, 0, " ", "\"\"[]", "-", NULL,
           "10.100.20.1", "", "", "31/Dec/2007:00:17:10 +0100",
           "GET /cgi-bin/bugzilla/buglist.cgi?keywords_type=allwords&keywords=public&format=simple HTTP/1.1", "200", "2708", "",
           "curl/7.15.5 (i4 86-pc-linux-gnu) libcurl/7.15.5 OpenSSL/0.9.8c zlib/1.2.3 libidn/0.6.5", "2", "bugzilla.balabit", "",
           NULL);

  testcase("10.100.20.1 - - [31/Dec/2007:00:17:10 +0100] \"GET /cgi-bin/bugzilla/buglist.cgi?keywords_type=allwords&keywords=public&format=simple HTTP/1.1\" 200 2708 \"-\" \"curl/7.15.5 (i4 86-pc-linux-gnu) libcurl/7.15.5 OpenSSL/0.9.8c zlib/1.2.3 libidn/0.6.5\" 2 bugzilla.balabit almafa",
           LP_NOPARSE, 11, CSV_SCANNER_ESCAPE_BACKSLASH, CSV_SCANNER_DROP_INVALID, " ", "\"\"[]", "-", NULL,
           NULL);

  testcase("random.vhost 10.0.0.1 - \"GET /index.html HTTP/1.1\" 200", LP_NOPARSE, 5, CSV_SCANNER_ESCAPE_NONE,
           CSV_SCANNER_DROP_INVALID, " ", "\"\"", "-", NULL,
           "random.vhost", "10.0.0.1", "", "GET /index.html HTTP/1.1", "200", NULL);

  testcase("random.vhost 10.0.0.1 - \"GET /index.html HTTP/1.1\" 200", LP_NOPARSE, 5, CSV_SCANNER_ESCAPE_BACKSLASH,
           CSV_SCANNER_DROP_INVALID, " ", "\"\"", "-", NULL,
           "random.vhost", "10.0.0.1", "", "GET /index.html HTTP/1.1", "200", NULL);

  /* greedy column can be empty */
  testcase("random.vhost 10.0.0.1 - \"GET /index.html HTTP/1.1\" 200", LP_NOPARSE, 6, CSV_SCANNER_ESCAPE_NONE,
           CSV_SCANNER_GREEDY | CSV_SCANNER_DROP_INVALID, " ", "\"\"", "-", NULL,
           "random.vhost", "10.0.0.1", "", "GET /index.html HTTP/1.1", "200", "", NULL);

  testcase("random.vhost 10.0.0.1 - \"GET /index.html HTTP/1.1\" 200", LP_NOPARSE, 6, CSV_SCANNER_ESCAPE_BACKSLASH,
           CSV_SCANNER_GREEDY | CSV_SCANNER_DROP_INVALID, " ", "\"\"", "-", NULL,
           "random.vhost", "10.0.0.1", "", "GET /index.html HTTP/1.1", "200", "", NULL);

  testcase("random.vhost\t10.0.0.1\t-\t\"GET /index.html HTTP/1.1\"\t200", LP_NOPARSE, 6, CSV_SCANNER_ESCAPE_BACKSLASH, 0,
           "\t", "\"\"", "-", NULL,
           "random.vhost", "10.0.0.1", "", "GET /index.html HTTP/1.1", "200", "", NULL);
  testcase("random.vhost\t10.0.0.1\t-\t\"GET /index.html HTTP/1.1\"\t\t200", LP_NOPARSE, 7, CSV_SCANNER_ESCAPE_BACKSLASH,
           0, "\t", "\"\"", "-", NULL,
           "random.vhost", "10.0.0.1", "", "GET /index.html HTTP/1.1", "", "200", "", NULL);


  app_shutdown();
  return 0;
}
