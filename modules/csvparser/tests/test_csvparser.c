/*
 * Copyright (c) 2008-2018 Balabit
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
#include "scratch-buffers.h"

#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <criterion/criterion.h>
#include <criterion/parameterized.h>

MsgFormatOptions parse_options;

typedef struct _csvparser_test_param
{
  const gchar *msg;
  guint parse_flags;
  gint max_columns;
  gboolean drop_invalid;
  gint dialect;
  guint32 flags;
  const gchar *delimiters;
  const gchar *quotes;
  const gchar *null_value;
  const gchar *string_delims[2];
  const gchar *expected_values[13];
} CsvParserTestParam;

ParameterizedTestParameters(parser, test_csv_parser)
{
  static CsvParserTestParam parser_params[] =
  {
    // string delim & single char & a char is in the string
    {
      .msg = "<15> openvpn[2499]: PTHREAD support :initialized",
      .parse_flags = 0,
      .max_columns = -1,
      .drop_invalid = FALSE,
      .dialect = CSV_SCANNER_ESCAPE_NONE,
      .flags = 0,
      .delimiters = " ",
      .quotes = NULL,
      .null_value = NULL,
      .string_delims = {" :", NULL},
      .expected_values = {"PTHREAD", "support", "initialized", NULL}
    },

    // empty message
    {
      .msg = "<15> openvpn[2499]:",
      .parse_flags = 0,
      .max_columns = -1,
      .drop_invalid = TRUE,
      .dialect = CSV_SCANNER_ESCAPE_NONE,
      .flags = 0,
      .delimiters = " ",
      .quotes = NULL,
      .null_value = NULL,
      .string_delims = {" :", NULL},
      .expected_values = {NULL}
    },

    // string delim & single char & a char not in the string
    {
      .msg = "<15> openvpn[2499]: PTHREAD,support :initialized",
      .parse_flags = 0,
      .max_columns = -1,
      .drop_invalid = FALSE,
      .dialect = CSV_SCANNER_ESCAPE_NONE,
      .flags = 0,
      .delimiters = ",",
      .quotes = NULL,
      .null_value = NULL,
      .string_delims = {" :", NULL},
      .expected_values = {"PTHREAD", "support", "initialized", NULL}
    },

    // string delim & multi char & a char is in the string
    {
      .msg = "<15> openvpn[2499]: PTHREAD support :initialized",
      .parse_flags = 0,
      .max_columns = -1,
      .drop_invalid = FALSE,
      .dialect = CSV_SCANNER_ESCAPE_NONE,
      .flags = 0,
      .delimiters = " :",
      .quotes = NULL,
      .null_value = NULL,
      .string_delims = {" :", NULL},
      .expected_values = {"PTHREAD", "support", "initialized", NULL}
    },

    // string delim & multi char & a char not in the string
    {
      .msg = "<15> openvpn[2499]: PTHREAD,support :initialized",
      .parse_flags = 0,
      .max_columns = -1,
      .drop_invalid = FALSE,
      .dialect = CSV_SCANNER_ESCAPE_NONE,
      .flags = 0,
      .delimiters = ";,",
      .quotes = NULL,
      .null_value = NULL,
      .string_delims = {" :", NULL},
      .expected_values = {"PTHREAD", "support", "initialized", NULL}
    },

    // quote + string delim & multi char & char is in the string too
    {
      .msg = "<15> openvpn[2499]: 'PTHREAD' 'support' :'initialized'",
      .parse_flags = 0,
      .max_columns = -1,
      .drop_invalid = FALSE,
      .dialect = CSV_SCANNER_ESCAPE_NONE,
      .flags = 0,
      .delimiters = " :",
      .quotes = "''",
      .null_value = NULL,
      .string_delims = {" :", NULL},
      .expected_values = {"PTHREAD", "support", "initialized", NULL}
    },

    // BE + quote + string delim & multi char & char is in the string too
    {
      .msg = "<15> openvpn[2499]: 'PTHRE\\\'AD' 'support' :'initialized'",
      .parse_flags = 0,
      .max_columns = -1,
      .drop_invalid = FALSE,
      .dialect = CSV_SCANNER_ESCAPE_BACKSLASH,
      .flags = 0,
      .delimiters = " :",
      .quotes = "''",
      .null_value = NULL,
      .string_delims = {" :", NULL},
      .expected_values = {"PTHRE'AD", "support", "initialized", NULL}
    },

    // DCE + quote + string delim & multi char & char not in the string
    {
      .msg = "<15> openvpn[2499]: 'PTHREAD','sup''port' :'initialized'",
      .parse_flags = 0,
      .max_columns = -1,
      .drop_invalid = FALSE,
      .dialect = CSV_SCANNER_ESCAPE_DOUBLE_CHAR,
      .flags = 0,
      .delimiters = ";,",
      .quotes = "''",
      .null_value = NULL,
      .string_delims = {" :", NULL},
      .expected_values = {"PTHREAD", "sup'port", "initialized", NULL}
    },

    {
      .msg = "<15> openvpn[2499]: PTHREAD support initialized",
      .parse_flags = 0,
      .max_columns = 3,
      .drop_invalid = FALSE,
      .dialect = CSV_SCANNER_ESCAPE_NONE,
      .flags = 0,
      .delimiters = " ",
      .quotes = NULL,
      .null_value = NULL,
      .string_delims = {NULL},
      .expected_values = {"PTHREAD", "support", "initialized", NULL}
    },

    {
      .msg = "<16> openvpn[2499]: PTHREAD support initialized",
      .parse_flags = 0,
      .max_columns = 2,
      .drop_invalid = TRUE,
      .dialect = CSV_SCANNER_ESCAPE_NONE,
      .flags = 0,
      .delimiters = " ",
      .quotes = NULL,
      .null_value = NULL,
      .string_delims = {NULL},
      .expected_values = {NULL}
    },

    {
      .msg = "<17> openvpn[2499]: PTHREAD support initialized",
      .parse_flags = 0,
      .max_columns = 2,
      .drop_invalid = FALSE,
      .dialect = CSV_SCANNER_ESCAPE_NONE,
      .flags = CSV_SCANNER_GREEDY,
      .delimiters = " ",
      .quotes = NULL,
      .null_value = NULL,
      .string_delims = {NULL},
      .expected_values = {"PTHREAD", "support initialized", NULL}
    },

    {
      .msg = "<18> openvpn[2499]: PTHREAD support initialized",
      .parse_flags = 0,
      .max_columns = -1,
      .drop_invalid = FALSE,
      .dialect = CSV_SCANNER_ESCAPE_NONE,
      .flags = 0,
      .delimiters = " ,;",
      .quotes = NULL,
      .null_value = NULL,
      .string_delims = {NULL},
      .expected_values = {"PTHREAD", "support", "initialized", NULL}
    },

    {
      .msg = "<19> openvpn[2499]: PTHREAD support initialized",
      .parse_flags = 0,
      .max_columns = -1,
      .drop_invalid = FALSE,
      .dialect = CSV_SCANNER_ESCAPE_NONE,
      .flags = 0,
      .delimiters = " ,;",
      .quotes = NULL,
      .null_value = "support",
      .string_delims = {NULL},
      .expected_values = {"PTHREAD", "", "initialized", NULL}
    },

    {
      .msg = "<15> openvpn[2499]: \"PTHREAD\" \"support\" \"initialized\"",
      .parse_flags = 0,
      .max_columns = -1,
      .drop_invalid = FALSE,
      .dialect = CSV_SCANNER_ESCAPE_NONE,
      .flags = 0,
      .delimiters = " ",
      .quotes = NULL,
      .null_value = NULL,
      .string_delims = {NULL},
      .expected_values = {"PTHREAD", "support", "initialized", NULL}
    },

    {
      .msg = "<15> openvpn[2499]: \"  PTHREAD  \" \" support\" \"initialized \"",
      .parse_flags = 0,
      .max_columns = -1,
      .drop_invalid = FALSE,
      .dialect = CSV_SCANNER_ESCAPE_NONE,
      .flags = CSV_SCANNER_STRIP_WHITESPACE,
      .delimiters = " ",
      .quotes = NULL,
      .null_value = NULL,
      .string_delims = {NULL},
      .expected_values = {"PTHREAD", "support", "initialized", NULL}
    },

    {
      .msg = "<15> openvpn[2499]: \"PTHREAD support\" \"initialized\"",
      .parse_flags = 0,
      .max_columns = -1,
      .drop_invalid = FALSE,
      .dialect = CSV_SCANNER_ESCAPE_NONE,
      .flags = 0,
      .delimiters = " ",
      .quotes = NULL,
      .null_value = NULL,
      .string_delims = {NULL},
      .expected_values = {"PTHREAD support", "initialized", NULL}
    },

    {
      .msg = "<15> openvpn[2499]: \"PTHREAD support initialized\"",
      .parse_flags = 0,
      .max_columns = -1,
      .drop_invalid = FALSE,
      .dialect = CSV_SCANNER_ESCAPE_NONE,
      .flags = 0,
      .delimiters = " ",
      .quotes = NULL,
      .null_value = NULL,
      .string_delims = {NULL},
      .expected_values = {"PTHREAD support initialized", NULL}
    },

    {
      .msg = "<15> openvpn[2499]: \"PTHREAD support initialized",
      .parse_flags = 0,
      .max_columns = -1,
      .drop_invalid = FALSE,
      .dialect = CSV_SCANNER_ESCAPE_NONE,
      .flags = 0,
      .delimiters = " ",
      .quotes = NULL,
      .null_value = NULL,
      .string_delims = {NULL},
      .expected_values = {"PTHREAD support initialized", NULL}
    },

    {
      .msg = "<20> openvpn[2499]: PTHREAD support initialized",
      .parse_flags = 0,
      .max_columns = -1,
      .drop_invalid = FALSE,
      .dialect = CSV_SCANNER_ESCAPE_BACKSLASH,
      .flags = 0,
      .delimiters = " ",
      .quotes = NULL,
      .null_value = NULL,
      .string_delims = {NULL},
      .expected_values = {"PTHREAD", "support", "initialized", NULL}
    },

    {
      .msg = "<21> openvpn[2499]: PTHREAD support initialized",
      .parse_flags = 0,
      .max_columns = 2,
      .drop_invalid = FALSE,
      .dialect = CSV_SCANNER_ESCAPE_BACKSLASH,
      .flags = CSV_SCANNER_GREEDY,
      .delimiters = " ",
      .quotes = NULL,
      .null_value = NULL,
      .string_delims = {NULL},
      .expected_values = {"PTHREAD", "support initialized", NULL}
    },

    {
      .msg = "<22> openvpn[2499]: PTHREAD support initialized",
      .parse_flags = 0,
      .max_columns = -1,
      .drop_invalid = FALSE,
      .dialect = CSV_SCANNER_ESCAPE_BACKSLASH,
      .flags = 0,
      .delimiters = " ;,",
      .quotes = NULL,
      .null_value = NULL,
      .string_delims = {NULL},
      .expected_values = {"PTHREAD", "support", "initialized", NULL}
    },

    {
      .msg = "<15> openvpn[2499]: \"PTHREAD\" \"support\" \"initialized\"",
      .parse_flags = 0,
      .max_columns = -1,
      .drop_invalid = FALSE,
      .dialect = CSV_SCANNER_ESCAPE_BACKSLASH,
      .flags = 0,
      .delimiters = " ",
      .quotes = NULL,
      .null_value = NULL,
      .string_delims = {NULL},
      .expected_values = {"PTHREAD", "support", "initialized", NULL}
    },

    {
      .msg = "<15> openvpn[2499]: \"PTHREAD\" \"support\" \"initialized\"",
      .parse_flags = 0,
      .max_columns = 2,
      .drop_invalid = FALSE,
      .dialect = CSV_SCANNER_ESCAPE_BACKSLASH,
      .flags = CSV_SCANNER_GREEDY,
      .delimiters = " ",
      .quotes = NULL,
      .null_value = NULL,
      .string_delims = {NULL},
      .expected_values = {"PTHREAD", "\"support\" \"initialized\"", NULL}
    },

    {
      .msg = "<15> openvpn[2499]: \"  PTHREAD \" \"  support\" \"initialized  \"",
      .parse_flags = 0,
      .max_columns = -1,
      .drop_invalid = FALSE,
      .dialect = CSV_SCANNER_ESCAPE_BACKSLASH,
      .flags = CSV_SCANNER_STRIP_WHITESPACE,
      .delimiters = " ",
      .quotes = NULL,
      .null_value = NULL,
      .string_delims = {NULL},
      .expected_values = {"PTHREAD", "support", "initialized", NULL}
    },

    {
      .msg = "<15> openvpn[2499]: \"PTHREAD support\" \"initialized\"",
      .parse_flags = 0,
      .max_columns = -1,
      .drop_invalid = FALSE,
      .dialect = CSV_SCANNER_ESCAPE_BACKSLASH,
      .flags = 0,
      .delimiters = " ",
      .quotes = NULL,
      .null_value = NULL,
      .string_delims = {NULL},
      .expected_values = {"PTHREAD support", "initialized", NULL}
    },

    {
      .msg = "<15> openvpn[2499]: \"PTHREAD \\\"support initialized\"",
      .parse_flags = 0,
      .max_columns = -1,
      .drop_invalid = FALSE,
      .dialect = CSV_SCANNER_ESCAPE_BACKSLASH,
      .flags = 0,
      .delimiters = " ",
      .quotes = NULL,
      .null_value = NULL,
      .string_delims = {NULL},
      .expected_values = {"PTHREAD \"support initialized", NULL}
    },

    {
      .msg = "<15> openvpn[2499]: \"PTHREAD support initialized",
      .parse_flags = 0,
      .max_columns = -1,
      .drop_invalid = FALSE,
      .dialect = CSV_SCANNER_ESCAPE_BACKSLASH,
      .flags = 0,
      .delimiters = " ",
      .quotes = NULL,
      .null_value = NULL,
      .string_delims = {NULL},
      .expected_values = {"PTHREAD support initialized", NULL}
    },

    {
      .msg = "<23> openvpn[2499]: PTHREAD support initialized",
      .parse_flags = 0,
      .max_columns = -1,
      .drop_invalid = FALSE,
      .dialect = CSV_SCANNER_ESCAPE_DOUBLE_CHAR,
      .flags = 0,
      .delimiters = " ",
      .quotes = NULL,
      .null_value = NULL,
      .string_delims = {NULL},
      .expected_values = {"PTHREAD", "support", "initialized", NULL}
    },

    {
      .msg = "<15> openvpn[2499]: \"PTHREAD\" \"support\" \"initialized\"",
      .parse_flags = 0,
      .max_columns = -1,
      .drop_invalid = FALSE,
      .dialect = CSV_SCANNER_ESCAPE_DOUBLE_CHAR,
      .flags = 0,
      .delimiters = " ",
      .quotes = NULL,
      .null_value = NULL,
      .string_delims = {NULL},
      .expected_values = {"PTHREAD", "support", "initialized", NULL}
    },

    {
      .msg = "<15> openvpn[2499]: \"PTHREAD\" \"support\" \"initialized\"",
      .parse_flags = 0,
      .max_columns = 2,
      .drop_invalid = FALSE,
      .dialect = CSV_SCANNER_ESCAPE_DOUBLE_CHAR,
      .flags = CSV_SCANNER_GREEDY,
      .delimiters = " ",
      .quotes = NULL,
      .null_value = NULL,
      .string_delims = {NULL},
      .expected_values = {"PTHREAD", "\"support\" \"initialized\"", NULL}
    },

    {
      .msg = "<15> openvpn[2499]: \"  PTHREAD \" \"  support\" \"initialized  \"",
      .parse_flags = 0,
      .max_columns = -1,
      .drop_invalid = FALSE,
      .dialect = CSV_SCANNER_ESCAPE_DOUBLE_CHAR,
      .flags = CSV_SCANNER_STRIP_WHITESPACE,
      .delimiters = " ",
      .quotes = NULL,
      .null_value = NULL,
      .string_delims = {NULL},
      .expected_values = {"PTHREAD", "support", "initialized", NULL}
    },

    {
      .msg = "<15> openvpn[2499]: \"  PTHREAD \" \"  support\" \"initialized  \"",
      .parse_flags = 0,
      .max_columns = 2,
      .drop_invalid = FALSE,
      .dialect = CSV_SCANNER_ESCAPE_DOUBLE_CHAR,
      .flags = CSV_SCANNER_GREEDY + CSV_SCANNER_STRIP_WHITESPACE,
      .delimiters = " ",
      .quotes = NULL,
      .null_value = NULL,
      .string_delims = {NULL},
      .expected_values = {"PTHREAD", "\"  support\" \"initialized  \"", NULL}
    },

    {
      .msg = "<15> openvpn[2499]: \"PTHREAD support\" \"initialized\"",
      .parse_flags = 0,
      .max_columns = -1,
      .drop_invalid = FALSE,
      .dialect = CSV_SCANNER_ESCAPE_DOUBLE_CHAR,
      .flags = 0,
      .delimiters = " ",
      .quotes = NULL,
      .null_value = NULL,
      .string_delims = {NULL},
      .expected_values = {"PTHREAD support", "initialized", NULL}
    },

    {
      .msg = "<15> openvpn[2499]: \"PTHREAD \"\"support initialized\"",
      .parse_flags = 0,
      .max_columns = -1,
      .drop_invalid = FALSE,
      .dialect = CSV_SCANNER_ESCAPE_DOUBLE_CHAR,
      .flags = 0,
      .delimiters = " ",
      .quotes = NULL,
      .null_value = NULL,
      .string_delims = {NULL},
      .expected_values = {"PTHREAD \"support initialized", NULL}
    },

    {
      .msg = "<15> openvpn[2499]: \"PTHREAD support initialized",
      .parse_flags = 0,
      .max_columns = -1,
      .drop_invalid = FALSE,
      .dialect = CSV_SCANNER_ESCAPE_DOUBLE_CHAR,
      .flags = 0,
      .delimiters = " ",
      .quotes = NULL,
      .null_value = NULL,
      .string_delims = {NULL},
      .expected_values = {"PTHREAD support initialized", NULL}
    },

    {
      .msg = "postfix/smtpd",
      .parse_flags = LP_NOPARSE,
      .max_columns = 2,
      .drop_invalid = TRUE,
      .dialect = CSV_SCANNER_ESCAPE_NONE,
      .flags = CSV_SCANNER_GREEDY,
      .delimiters = "/",
      .quotes = NULL,
      .null_value = NULL,
      .string_delims = {NULL},
      .expected_values = {"postfix", "smtpd", NULL}
    },

    {
      .msg = "postfix",
      .parse_flags = LP_NOPARSE,
      .max_columns = 3,
      .drop_invalid = TRUE,
      .dialect = CSV_SCANNER_ESCAPE_NONE,
      .flags = CSV_SCANNER_GREEDY,
      .delimiters = "/",
      .quotes = NULL,
      .null_value = NULL,
      .string_delims = {NULL},
      .expected_values = {NULL}
    },

    {
      .msg = "postfix/smtpd/ququ",
      .parse_flags = LP_NOPARSE,
      .max_columns = 2,
      .drop_invalid = TRUE,
      .dialect = CSV_SCANNER_ESCAPE_NONE,
      .flags = CSV_SCANNER_GREEDY,
      .delimiters = "/",
      .quotes = NULL,
      .null_value = NULL,
      .string_delims = {NULL},
      .expected_values = {"postfix", "smtpd/ququ", NULL}
    },

    {
      .msg = "Jul 27 19:55:33 myhost zabbix: ZabbixConnector.log : 19:55:32,782 INFO  [Thread-2834]     - [ZabbixEventSyncCommand] Processing   message <?xml version=\"1.0\" encoding=\"UTF-8\"?>",
      .parse_flags = LP_EXPECT_HOSTNAME,
      .max_columns = 2,
      .drop_invalid = FALSE,
      .dialect = CSV_SCANNER_ESCAPE_NONE,
      .flags = CSV_SCANNER_GREEDY,
      .delimiters = " ",
      .quotes = NULL,
      .null_value = NULL,
      .string_delims = {NULL},
      .expected_values = {"ZabbixConnector.log",
        ": 19:55:32,782 INFO  [Thread-2834]     - [ZabbixEventSyncCommand] Processing   message <?xml version=\"1.0\" encoding=\"UTF-8\"?>", NULL
      }
    },

    {
      .msg = "10.100.20.1 - - [31/Dec/2007:00:17:10 +0100] \"GET /cgi-bin/bugzilla/buglist.cgi?keywords_type=allwords&keywords=public&format=simple HTTP/1.1\" 200 2708 \"-\" \"curl/7.15.5 (i4 86-pc-linux-gnu) libcurl/7.15.5 OpenSSL/0.9.8c zlib/1.2.3 libidn/0.6.5\" 2 bugzilla.balabit",
      .parse_flags = LP_NOPARSE,
      .max_columns = -1,
      .drop_invalid = FALSE,
      .dialect = CSV_SCANNER_ESCAPE_BACKSLASH,
      .flags = 0,
      .delimiters = " ",
      .quotes = "\"\"[]",
      .null_value = "-",
      .string_delims = {NULL},
      .expected_values = {"10.100.20.1",
        "",
        "",
        "31/Dec/2007:00:17:10 +0100",
        "GET /cgi-bin/bugzilla/buglist.cgi?keywords_type=allwords&keywords=public&format=simple HTTP/1.1",
        "200",
        "2708",
        "",
        "curl/7.15.5 (i4 86-pc-linux-gnu) libcurl/7.15.5 OpenSSL/0.9.8c zlib/1.2.3 libidn/0.6.5",
        "2",
        "bugzilla.balabit",
        NULL
      }
    },

    {
      .msg = "10.100.20.1 - - [31/Dec/2007:00:17:10 +0100] \"GET /cgi-bin/bugzilla/buglist.cgi?keywords_type=allwords&keywords=public&format=simple HTTP/1.1\" 200 2708 \"-\" \"curl/7.15.5 (i4 86-pc-linux-gnu) libcurl/7.15.5 OpenSSL/0.9.8c zlib/1.2.3 libidn/0.6.5\" 2 bugzilla.balabit",
      .parse_flags = LP_NOPARSE,
      .max_columns = 11,
      .drop_invalid = FALSE,
      .dialect = CSV_SCANNER_ESCAPE_BACKSLASH,
      .flags = 0,
      .delimiters = " ",
      .quotes = "\"\"[]",
      .null_value = "-",
      .string_delims = {NULL},
      .expected_values = {"10.100.20.1",
        "",
        "",
        "31/Dec/2007:00:17:10 +0100",
        "GET /cgi-bin/bugzilla/buglist.cgi?keywords_type=allwords&keywords=public&format=simple HTTP/1.1",
        "200",
        "2708",
        "",
        "curl/7.15.5 (i4 86-pc-linux-gnu) libcurl/7.15.5 OpenSSL/0.9.8c zlib/1.2.3 libidn/0.6.5", "2", "bugzilla.balabit",
        NULL
      }
    },

    {
      .msg = "10.100.20.1 - - [31/Dec/2007:00:17:10 +0100] \"GET /cgi-bin/bugzilla/buglist.cgi?keywords_type=allwords&keywords=public&format=simple HTTP/1.1\" 200 2708 \"-\" \"curl/7.15.5 (i4 86-pc-linux-gnu) libcurl/7.15.5 OpenSSL/0.9.8c zlib/1.2.3 libidn/0.6.5\" 2 bugzilla.balabit",
      .parse_flags = LP_NOPARSE,
      .max_columns = 10,
      .drop_invalid = FALSE,
      .dialect = CSV_SCANNER_ESCAPE_BACKSLASH,
      .flags = 0,
      .delimiters = " ",
      .quotes = "\"\"[]",
      .null_value = "-",
      .string_delims = {NULL},
      .expected_values = {"10.100.20.1",
        "",
        "",
        "31/Dec/2007:00:17:10 +0100",
        "GET /cgi-bin/bugzilla/buglist.cgi?keywords_type=allwords&keywords=public&format=simple HTTP/1.1",
        "200",
        "2708",
        "",
        "curl/7.15.5 (i4 86-pc-linux-gnu) libcurl/7.15.5 OpenSSL/0.9.8c zlib/1.2.3 libidn/0.6.5",
        "2",
        NULL
      }
    },

    {
      .msg = "10.100.20.1 - - [31/Dec/2007:00:17:10 +0100] \"GET /cgi-bin/bugzilla/buglist.cgi?keywords_type=allwords&keywords=public&format=simple HTTP/1.1\" 200 2708 \"-\" \"curl/7.15.5 (i4 86-pc-linux-gnu) libcurl/7.15.5 OpenSSL/0.9.8c zlib/1.2.3 libidn/0.6.5\" 2 bugzilla.balabit",
      .parse_flags = LP_NOPARSE,
      .max_columns = 12,
      .drop_invalid = FALSE,
      .dialect = CSV_SCANNER_ESCAPE_BACKSLASH,
      .flags = 0,
      .delimiters = " ",
      .quotes = "\"\"[]",
      .null_value = "-",
      .string_delims = {NULL},
      .expected_values = {"10.100.20.1",
        "",
        "",
        "31/Dec/2007:00:17:10 +0100",
        "GET /cgi-bin/bugzilla/buglist.cgi?keywords_type=allwords&keywords=public&format=simple HTTP/1.1",
        "200",
        "2708",
        "",
        "curl/7.15.5 (i4 86-pc-linux-gnu) libcurl/7.15.5 OpenSSL/0.9.8c zlib/1.2.3 libidn/0.6.5",
        "2",
        "bugzilla.balabit",
        "",
        NULL
      }
    },

    {
      .msg = "10.100.20.1 - - [31/Dec/2007:00:17:10 +0100] \"GET /cgi-bin/bugzilla/buglist.cgi?keywords_type=allwords&keywords=public&format=simple HTTP/1.1\" 200 2708 \"-\" \"curl/7.15.5 (i4 86-pc-linux-gnu) libcurl/7.15.5 OpenSSL/0.9.8c zlib/1.2.3 libidn/0.6.5\" 2 bugzilla.balabit almafa",
      .parse_flags = LP_NOPARSE,
      .max_columns = 11,
      .drop_invalid = TRUE,
      .dialect = CSV_SCANNER_ESCAPE_BACKSLASH,
      .flags = 0,
      .delimiters = " ",
      .quotes = "\"\"[]",
      .null_value = "-",
      .string_delims = {NULL},
      .expected_values = {NULL}
    },

    {
      .msg = "random.vhost 10.0.0.1 - \"GET /index.html HTTP/1.1\" 200",
      .parse_flags = LP_NOPARSE,
      .max_columns = 5,
      .drop_invalid = TRUE,
      .dialect = CSV_SCANNER_ESCAPE_NONE,
      .flags = 0,
      .delimiters = " ",
      .quotes = "\"\"[]",
      .null_value = "-",
      .string_delims = {NULL},
      .expected_values = {"random.vhost", "10.0.0.1", "", "GET /index.html HTTP/1.1", "200", NULL}
    },

    {
      .msg = "random.vhost 10.0.0.1 - \"GET /index.html HTTP/1.1\" 200",
      .parse_flags = LP_NOPARSE,
      .max_columns = 5,
      .drop_invalid = TRUE,
      .dialect = CSV_SCANNER_ESCAPE_BACKSLASH,
      .flags = 0,
      .delimiters = " ",
      .quotes = "\"\"[]",
      .null_value = "-",
      .string_delims = {NULL},
      .expected_values = {"random.vhost", "10.0.0.1", "", "GET /index.html HTTP/1.1", "200", NULL}
    },

    /* greedy column can be empty */
    {
      .msg = "random.vhost 10.0.0.1 - \"GET /index.html HTTP/1.1\" 200",
      .parse_flags = LP_NOPARSE,
      .max_columns = 6,
      .drop_invalid = TRUE,
      .dialect = CSV_SCANNER_ESCAPE_NONE,
      .flags = CSV_SCANNER_GREEDY,
      .delimiters = " ",
      .quotes = "\"\"[]",
      .null_value = "-",
      .string_delims = {NULL},
      .expected_values = {"random.vhost", "10.0.0.1", "", "GET /index.html HTTP/1.1", "200", "", NULL}
    },

    {
      .msg = "random.vhost 10.0.0.1 - \"GET /index.html HTTP/1.1\" 200",
      .parse_flags = LP_NOPARSE,
      .max_columns = 6,
      .drop_invalid = TRUE,
      .dialect = CSV_SCANNER_ESCAPE_BACKSLASH,
      .flags = CSV_SCANNER_GREEDY,
      .delimiters = " ",
      .quotes = "\"\"[]",
      .null_value = "-",
      .string_delims = {NULL},
      .expected_values = {"random.vhost", "10.0.0.1", "", "GET /index.html HTTP/1.1", "200", "", NULL}
    },

    {
      .msg = "random.vhost\t10.0.0.1\t-\t\"GET /index.html HTTP/1.1\"\t200",
      .parse_flags = LP_NOPARSE,
      .max_columns = 6,
      .drop_invalid = FALSE,
      .dialect = CSV_SCANNER_ESCAPE_BACKSLASH,
      .flags = 0,
      .delimiters = "\t",
      .quotes = "\"\"",
      .null_value = "-",
      .string_delims = {NULL},
      .expected_values = {"random.vhost", "10.0.0.1", "", "GET /index.html HTTP/1.1", "200", "", NULL}
    },

    {
      .msg = "random.vhost\t10.0.0.1\t-\t\"GET /index.html HTTP/1.1\"\t\t200",
      .parse_flags = LP_NOPARSE,
      .max_columns = 7,
      .drop_invalid = FALSE,
      .dialect = CSV_SCANNER_ESCAPE_BACKSLASH,
      .flags = 0,
      .delimiters = "\t",
      .quotes = "\"\"",
      .null_value = "-",
      .string_delims = {NULL},
      .expected_values = {"random.vhost", "10.0.0.1", "", "GET /index.html HTTP/1.1", "", "200", "", NULL}
    },
  };

  return cr_make_param_array(CsvParserTestParam, parser_params, G_N_ELEMENTS(parser_params));
}

ParameterizedTest(CsvParserTestParam *param, parser, test_csv_parser)
{
  LogMessage *logmsg;
  LogParser *p, *pclone;
  gint i;
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

  if (param->max_columns != -1)
    {
      cr_assert(param->max_columns < (sizeof(column_array) / sizeof(column_array[0])));
      column_array[param->max_columns] = NULL;
    }

  parse_options.flags = param->parse_flags;
  logmsg = log_msg_new(param->msg, strlen(param->msg), NULL, &parse_options);

  p = csv_parser_new(NULL);
  csv_parser_set_drop_invalid(p, param->drop_invalid);
  csv_scanner_options_set_flags(csv_parser_get_scanner_options(p), param->flags);
  csv_scanner_options_set_dialect(csv_parser_get_scanner_options(p), param->dialect);
  csv_scanner_options_set_columns(csv_parser_get_scanner_options(p), string_array_to_list(column_array));
  if (param->delimiters)
    csv_scanner_options_set_delimiters(csv_parser_get_scanner_options(p), param->delimiters);
  if (param->quotes)
    csv_scanner_options_set_quote_pairs(csv_parser_get_scanner_options(p), param->quotes);
  if (param->null_value)
    csv_scanner_options_set_null_value(csv_parser_get_scanner_options(p), param->null_value);

  csv_scanner_options_set_string_delimiters(csv_parser_get_scanner_options(p),
                                            string_array_to_list(param->string_delims));

  pclone = (LogParser *) log_pipe_clone(&p->super);
  log_pipe_unref(&p->super);

  nvtable = nv_table_ref(logmsg->payload);
  success = log_parser_process(pclone, &logmsg, NULL, log_msg_get_value(logmsg, LM_V_MESSAGE, NULL), -1);
  nv_table_unref(nvtable);

  cr_assert_not((success && !param->expected_values[0]), "unexpected match; msg=%s\n", param->msg);
  cr_assert_not((!success && param->expected_values[0]), "unexpected non-match; msg=%s\n", param->msg);

  log_pipe_unref(&pclone->super);

  i = 0;
  while (param->expected_values[i] && column_array[i])
    {
      const gchar *value;
      gssize value_len;

      value = log_msg_get_value_by_name(logmsg, column_array[i], &value_len);

      if (param->expected_values[i] && param->expected_values[i][0])
        {
          cr_assert(value
                    && value[0],
                    "Testcase failed: expected value set, but no actual value; msg=\n'%s'\n, cond='value && value[0]', value='%s', expected_value='%s'\n",
                    param->msg,
                    value,
                    param->expected_values[i]);

          cr_assert(strlen(param->expected_values[i]) == value_len,
                    "Testcase failed: value length doesn't match actual length; msg=\n'%s'\n, cond='strlen(expected_value) == value_len', value_len='%d', strlen(expected_value)='%d', value=%s, expected_value=%s\n",
                    param->msg, (int)value_len,
                    (int)strlen(param->expected_values[i]),
                    value,
                    param->expected_values[i]);

          cr_assert(strncmp(value, param->expected_values[i], value_len) == 0,
                    "Testcase failed: value does not match expected value; msg=\n'%s'\n, cond='strncmp(value, expected_value, value_len) == 0', value='%s', expected_value='%s' value_len=%d\n",
                    param->msg,
                    value,
                    param->expected_values[i],
                    (int)value_len);
        }
      else
        {
          cr_assert(!(value
                      && value[0]),
                    "Testcase failed: expected unset, but actual value present; msg='%s', cond='!(value && value[0])', value='%s', expected_value='%s'\n",
                    param->msg, value, param->expected_values[i]);
        }

      i++;
    }

  log_msg_unref(logmsg);
}

void setup(void)
{
  app_startup();

  setenv("TZ", "MET-1METDST", TRUE);
  tzset();

  configuration = cfg_new_snippet();
  cfg_load_module(configuration, "syslogformat");
  msg_format_options_defaults(&parse_options);
  msg_format_options_init(&parse_options, configuration);
}

void teardown(void)
{
  scratch_buffers_explicit_gc();
  app_shutdown();
}

TestSuite(parser, .init = setup, .fini = teardown);
