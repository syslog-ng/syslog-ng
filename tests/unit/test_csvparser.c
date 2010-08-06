#include "syslog-ng.h"
#include "logmsg.h"
#include "apphook.h"
#include "logparser.h"
#include "misc.h"

#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>


#define TEST_ASSERT(x, desc)		\
  do 				\
    { 				\
        if (!(x))		\
          {			\
            fprintf(stderr, "Testcase failed: %s; msg='%s', cond='%s', value='%s', expected_value='%s'\n", desc, msg, #x, value, expected_value); \
            exit(1);		\
          }			\
    }				\
  while (0)

int
testcase(gchar *msg, guint parse_flags, gint max_columns, guint32 flags, gchar *delimiters, gchar *quotes, gchar *null_value, gchar *first_value, ...)
{
  LogMessage *logmsg;
  LogColumnParser *p;
  gchar *expected_value;
  gint i;
  va_list va;

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

  logmsg = log_msg_new(msg, strlen(msg), NULL, parse_flags, NULL, -1, 0xFFFF);

  p = log_csv_parser_new();
  log_csv_parser_set_flags(p, flags);
  log_column_parser_set_columns(p, string_array_to_list(column_array));
  if (delimiters)
    log_csv_parser_set_delimiters(p, delimiters);
  if (quotes)
    log_csv_parser_set_quote_pairs(p, quotes);
  if (null_value)
    log_csv_parser_set_null_value(p, null_value);
  success = log_parser_process(&p->super, logmsg);

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
  log_parser_free(&p->super);

  va_start(va, first_value);
  expected_value = first_value;
  i = 0;
  while (expected_value && column_array[i])
    {
      const gchar *value;
      gssize value_len;

      value = log_msg_get_value(logmsg, log_msg_get_value_handle(column_array[i]), &value_len);

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

  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, -1, LOG_CSV_PARSER_ESCAPE_NONE, " ", NULL, NULL,
           "PTHREAD", "support", "initialized", NULL);

  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, 3, LOG_CSV_PARSER_ESCAPE_NONE, " ", NULL, NULL,
           "PTHREAD", "support", "initialized", NULL);

  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, 2, LOG_CSV_PARSER_ESCAPE_NONE | LOG_CSV_PARSER_DROP_INVALID, " ", NULL, NULL,
          NULL);

  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, 2, LOG_CSV_PARSER_GREEDY | LOG_CSV_PARSER_ESCAPE_NONE, " ", NULL, NULL,
           "PTHREAD", "support initialized", NULL);

  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, -1, LOG_CSV_PARSER_ESCAPE_NONE, " ,;", NULL, NULL,
           "PTHREAD", "support", "initialized", NULL);

  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, -1, LOG_CSV_PARSER_ESCAPE_NONE, " ,;", NULL, "support",
           "PTHREAD", "", "initialized", NULL);

  testcase("<15> openvpn[2499]: \"PTHREAD\" \"support\" \"initialized\"", 0, -1, LOG_CSV_PARSER_ESCAPE_NONE, " ", NULL, NULL,
           "PTHREAD", "support", "initialized", NULL);

  testcase("<15> openvpn[2499]: \"PTHREAD\"+\"support\" \"initialized\"", 0, -1, LOG_CSV_PARSER_ESCAPE_NONE, " ", NULL, NULL,
           "PTHREAD", "+\"support\"", "initialized", NULL);

  testcase("<15> openvpn[2499]: \"  PTHREAD  \" \" support\" \"initialized \"", 0, -1, LOG_CSV_PARSER_ESCAPE_NONE + LOG_CSV_PARSER_STRIP_WHITESPACE, " ", NULL, NULL,
           "PTHREAD", "support", "initialized", NULL);

  testcase("<15> openvpn[2499]: \"PTHREAD support\" \"initialized\"", 0, -1, LOG_CSV_PARSER_ESCAPE_NONE, " ", NULL, NULL,
           "PTHREAD support", "initialized", NULL);

  testcase("<15> openvpn[2499]: \"PTHREAD support initialized\"", 0, -1, LOG_CSV_PARSER_ESCAPE_NONE, " ", NULL, NULL,
           "PTHREAD support initialized", NULL);

  testcase("<15> openvpn[2499]: \"PTHREAD support initialized", 0, -1, LOG_CSV_PARSER_ESCAPE_NONE, " ", NULL, NULL,
           "PTHREAD support initialized", NULL);

  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, -1, LOG_CSV_PARSER_ESCAPE_BACKSLASH, " ", NULL, NULL,
           "PTHREAD", "support", "initialized", NULL);

  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, 2, LOG_CSV_PARSER_GREEDY | LOG_CSV_PARSER_ESCAPE_BACKSLASH, " ", NULL, NULL,
           "PTHREAD", "support initialized", NULL);

  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, -1, LOG_CSV_PARSER_ESCAPE_BACKSLASH, " ;,", NULL, NULL,
           "PTHREAD", "support", "initialized", NULL);

  testcase("<15> openvpn[2499]: \"PTHREAD\" \"support\" \"initialized\"", 0, -1, LOG_CSV_PARSER_ESCAPE_BACKSLASH, " ", NULL, NULL,
           "PTHREAD", "support", "initialized", NULL);

  testcase("<15> openvpn[2499]: \"PTHREAD\" \"support\" \"initialized\"", 0, 2, LOG_CSV_PARSER_GREEDY | LOG_CSV_PARSER_ESCAPE_BACKSLASH, " ", NULL, NULL,
           "PTHREAD", "\"support\" \"initialized\"", NULL);

  testcase("<15> openvpn[2499]: \"  PTHREAD \" \"  support\" \"initialized  \"", 0, -1, LOG_CSV_PARSER_ESCAPE_BACKSLASH + LOG_CSV_PARSER_STRIP_WHITESPACE, " ", NULL, NULL,
           "PTHREAD", "support", "initialized", NULL);

  testcase("<15> openvpn[2499]: \"PTHREAD support\" \"initialized\"", 0, -1, LOG_CSV_PARSER_ESCAPE_BACKSLASH, " ", NULL, NULL,
           "PTHREAD support", "initialized", NULL);

  testcase("<15> openvpn[2499]: \"PTHREAD \\\"support initialized\"", 0, -1, LOG_CSV_PARSER_ESCAPE_BACKSLASH, " ", NULL, NULL,
           "PTHREAD \"support initialized", NULL);

  testcase("<15> openvpn[2499]: \"PTHREAD support initialized", 0, -1, LOG_CSV_PARSER_ESCAPE_BACKSLASH, " ", NULL, NULL,
           "PTHREAD support initialized", NULL);

  testcase("<15> openvpn[2499]: PTHREAD support initialized", 0, -1, LOG_CSV_PARSER_ESCAPE_DOUBLE_CHAR, " ", NULL, NULL,
           "PTHREAD", "support", "initialized", NULL);

  testcase("<15> openvpn[2499]: \"PTHREAD\" \"support\" \"initialized\"", 0, -1, LOG_CSV_PARSER_ESCAPE_DOUBLE_CHAR, " ", NULL, NULL,
           "PTHREAD", "support", "initialized", NULL);

  testcase("<15> openvpn[2499]: \"PTHREAD\" \"support\" \"initialized\"", 0, 2, LOG_CSV_PARSER_GREEDY | LOG_CSV_PARSER_ESCAPE_DOUBLE_CHAR, " ", NULL, NULL,
           "PTHREAD", "\"support\" \"initialized\"", NULL);

  testcase("<15> openvpn[2499]: \"  PTHREAD \" \"  support\" \"initialized  \"", 0, -1, LOG_CSV_PARSER_ESCAPE_DOUBLE_CHAR + LOG_CSV_PARSER_STRIP_WHITESPACE, " ", NULL, NULL,
           "PTHREAD", "support", "initialized", NULL);

  testcase("<15> openvpn[2499]: \"  PTHREAD \" \"  support\" \"initialized  \"", 0, 2, LOG_CSV_PARSER_GREEDY + LOG_CSV_PARSER_ESCAPE_DOUBLE_CHAR + LOG_CSV_PARSER_STRIP_WHITESPACE, " ", NULL, NULL,
           "PTHREAD", "\"  support\" \"initialized  \"", NULL);

  testcase("<15> openvpn[2499]: \"PTHREAD support\" \"initialized\"", 0, -1, LOG_CSV_PARSER_ESCAPE_DOUBLE_CHAR, " ", NULL, NULL,
           "PTHREAD support", "initialized", NULL);

  testcase("<15> openvpn[2499]: \"PTHREAD \"\"support initialized\"", 0, -1, LOG_CSV_PARSER_ESCAPE_DOUBLE_CHAR, " ", NULL, NULL,
           "PTHREAD \"support initialized", NULL);

  testcase("<15> openvpn[2499]: \"PTHREAD support initialized", 0, -1, LOG_CSV_PARSER_ESCAPE_DOUBLE_CHAR, " ", NULL, NULL,
           "PTHREAD support initialized", NULL);

  testcase("postfix/smtpd", LP_NOPARSE, 2, LOG_CSV_PARSER_ESCAPE_NONE | LOG_CSV_PARSER_GREEDY | LOG_CSV_PARSER_DROP_INVALID, "/", NULL, NULL,
           "postfix", "smtpd", NULL);

  testcase("postfix", LP_NOPARSE, 3, LOG_CSV_PARSER_ESCAPE_NONE | LOG_CSV_PARSER_GREEDY | LOG_CSV_PARSER_DROP_INVALID, "/", NULL, NULL,
           NULL);

  testcase("postfix/smtpd/ququ", LP_NOPARSE, 2, LOG_CSV_PARSER_ESCAPE_NONE | LOG_CSV_PARSER_GREEDY | LOG_CSV_PARSER_DROP_INVALID, "/", NULL, NULL,
           "postfix", "smtpd/ququ", NULL);

  testcase("Jul 27 19:55:33 myhost zabbix: ZabbixConnector.log : 19:55:32,782 INFO  [Thread-2834]     - [ZabbixEventSyncCommand] Processing   message <?xml version=\"1.0\" encoding=\"UTF-8\"?>", 0, 2, LOG_CSV_PARSER_GREEDY, " ", NULL, NULL,
           "ZabbixConnector.log", ": 19:55:32,782 INFO  [Thread-2834]     - [ZabbixEventSyncCommand] Processing   message <?xml version=\"1.0\" encoding=\"UTF-8\"?>", NULL);

  testcase("10.100.20.1 - - [31/Dec/2007:00:17:10 +0100] \"GET /cgi-bin/bugzilla/buglist.cgi?keywords_type=allwords&keywords=public&format=simple HTTP/1.1\" 200 2708 \"-\" \"curl/7.15.5 (i4 86-pc-linux-gnu) libcurl/7.15.5 OpenSSL/0.9.8c zlib/1.2.3 libidn/0.6.5\" 2 bugzilla.balabit", LP_NOPARSE, -1, LOG_CSV_PARSER_ESCAPE_BACKSLASH, " ", "\"\"[]", "-",
           "10.100.20.1", "", "", "31/Dec/2007:00:17:10 +0100", "GET /cgi-bin/bugzilla/buglist.cgi?keywords_type=allwords&keywords=public&format=simple HTTP/1.1", "200", "2708", "", "curl/7.15.5 (i4 86-pc-linux-gnu) libcurl/7.15.5 OpenSSL/0.9.8c zlib/1.2.3 libidn/0.6.5", "2", "bugzilla.balabit", NULL);

  testcase("10.100.20.1 - - [31/Dec/2007:00:17:10 +0100] \"GET /cgi-bin/bugzilla/buglist.cgi?keywords_type=allwords&keywords=public&format=simple HTTP/1.1\" 200 2708 \"-\" \"curl/7.15.5 (i4 86-pc-linux-gnu) libcurl/7.15.5 OpenSSL/0.9.8c zlib/1.2.3 libidn/0.6.5\" 2 bugzilla.balabit", LP_NOPARSE, 11, LOG_CSV_PARSER_ESCAPE_BACKSLASH, " ", "\"\"[]", "-",
           "10.100.20.1", "", "", "31/Dec/2007:00:17:10 +0100", "GET /cgi-bin/bugzilla/buglist.cgi?keywords_type=allwords&keywords=public&format=simple HTTP/1.1", "200", "2708", "", "curl/7.15.5 (i4 86-pc-linux-gnu) libcurl/7.15.5 OpenSSL/0.9.8c zlib/1.2.3 libidn/0.6.5", "2", "bugzilla.balabit", NULL);

  testcase("10.100.20.1 - - [31/Dec/2007:00:17:10 +0100] \"GET /cgi-bin/bugzilla/buglist.cgi?keywords_type=allwords&keywords=public&format=simple HTTP/1.1\" 200 2708 \"-\" \"curl/7.15.5 (i4 86-pc-linux-gnu) libcurl/7.15.5 OpenSSL/0.9.8c zlib/1.2.3 libidn/0.6.5\" 2 bugzilla.balabit", LP_NOPARSE, 10, LOG_CSV_PARSER_ESCAPE_BACKSLASH, " ", "\"\"[]", "-",
           "10.100.20.1", "", "", "31/Dec/2007:00:17:10 +0100", "GET /cgi-bin/bugzilla/buglist.cgi?keywords_type=allwords&keywords=public&format=simple HTTP/1.1", "200", "2708", "", "curl/7.15.5 (i4 86-pc-linux-gnu) libcurl/7.15.5 OpenSSL/0.9.8c zlib/1.2.3 libidn/0.6.5", "2", NULL);

  testcase("10.100.20.1 - - [31/Dec/2007:00:17:10 +0100] \"GET /cgi-bin/bugzilla/buglist.cgi?keywords_type=allwords&keywords=public&format=simple HTTP/1.1\" 200 2708 \"-\" \"curl/7.15.5 (i4 86-pc-linux-gnu) libcurl/7.15.5 OpenSSL/0.9.8c zlib/1.2.3 libidn/0.6.5\" 2 bugzilla.balabit", LP_NOPARSE, 12, LOG_CSV_PARSER_ESCAPE_BACKSLASH, " ", "\"\"[]", "-",
           "10.100.20.1", "", "", "31/Dec/2007:00:17:10 +0100", "GET /cgi-bin/bugzilla/buglist.cgi?keywords_type=allwords&keywords=public&format=simple HTTP/1.1", "200", "2708", "", "curl/7.15.5 (i4 86-pc-linux-gnu) libcurl/7.15.5 OpenSSL/0.9.8c zlib/1.2.3 libidn/0.6.5", "2", "bugzilla.balabit", "", NULL);

  testcase("10.100.20.1 - - [31/Dec/2007:00:17:10 +0100] \"GET /cgi-bin/bugzilla/buglist.cgi?keywords_type=allwords&keywords=public&format=simple HTTP/1.1\" 200 2708 \"-\" \"curl/7.15.5 (i4 86-pc-linux-gnu) libcurl/7.15.5 OpenSSL/0.9.8c zlib/1.2.3 libidn/0.6.5\" 2 bugzilla.balabit almafa", LP_NOPARSE, 11, LOG_CSV_PARSER_ESCAPE_BACKSLASH | LOG_CSV_PARSER_DROP_INVALID, " ", "\"\"[]", "-",
           NULL);

  testcase("random.vhost 10.0.0.1 - \"GET /index.html HTTP/1.1\" 200", LP_NOPARSE, 5, LOG_CSV_PARSER_ESCAPE_NONE | LOG_CSV_PARSER_DROP_INVALID, " ", "\"\"", "-",
           "random.vhost", "10.0.0.1", "", "GET /index.html HTTP/1.1", "200", NULL);

  testcase("random.vhost 10.0.0.1 - \"GET /index.html HTTP/1.1\" 200", LP_NOPARSE, 5, LOG_CSV_PARSER_ESCAPE_BACKSLASH | LOG_CSV_PARSER_DROP_INVALID, " ", "\"\"", "-",
           "random.vhost", "10.0.0.1", "", "GET /index.html HTTP/1.1", "200", NULL);

  /* greedy column can be empty */
  testcase("random.vhost 10.0.0.1 - \"GET /index.html HTTP/1.1\" 200", LP_NOPARSE, 6, LOG_CSV_PARSER_ESCAPE_NONE | LOG_CSV_PARSER_GREEDY | LOG_CSV_PARSER_DROP_INVALID, " ", "\"\"", "-",
           "random.vhost", "10.0.0.1", "", "GET /index.html HTTP/1.1", "200", "", NULL);

  testcase("random.vhost 10.0.0.1 - \"GET /index.html HTTP/1.1\" 200", LP_NOPARSE, 6, LOG_CSV_PARSER_ESCAPE_BACKSLASH | LOG_CSV_PARSER_GREEDY | LOG_CSV_PARSER_DROP_INVALID, " ", "\"\"", "-",
           "random.vhost", "10.0.0.1", "", "GET /index.html HTTP/1.1", "200", "", NULL);


  app_shutdown();
  return 0;
}


