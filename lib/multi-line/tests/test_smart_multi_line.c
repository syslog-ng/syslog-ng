/*
 * Copyright (c) 2023 Balazs Scheidler <bazsi77@gmail.com>
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
 */
#include <criterion/criterion.h>

#include "multi-line/smart-multi-line.h"
#include "scratch-buffers.h"
#include "apphook.h"
#include "cfg.h"
#include "reloc.h"

Test(smart_multi_line, three_unrelated_lines_that_are_not_backtraces)
{
  MultiLineLogic *mll = smart_multi_line_new();
  const gchar *first_line = "this is an initial line that is consumed";
  const gchar *second_line = "another line that is not part of a traceback";
  const gchar *third_line = "yet another line that is not part of a traceback";
  const gchar *fourth_line = "yet-yet another line that is not part of a traceback";

  gint verdict = multi_line_logic_accumulate_line(mll, NULL, 0, (const guchar *) first_line, strlen(first_line));

  cr_assert(verdict & MLL_CONSUME_SEGMENT);
  cr_assert(verdict & MLL_EXTRACTED);

  verdict = multi_line_logic_accumulate_line(mll, NULL, 0, (const guchar *) second_line, strlen(second_line));

  cr_assert(verdict & MLL_CONSUME_SEGMENT);
  cr_assert(verdict & MLL_EXTRACTED);

  verdict = multi_line_logic_accumulate_line(mll, NULL, 0, (const guchar *) second_line, strlen(second_line));

  cr_assert(verdict & MLL_CONSUME_SEGMENT);
  cr_assert(verdict & MLL_EXTRACTED);

  verdict = multi_line_logic_accumulate_line(mll, NULL, 0, (const guchar *) third_line, strlen(third_line));

  cr_assert(verdict & MLL_CONSUME_SEGMENT);
  cr_assert(verdict & MLL_EXTRACTED);

  verdict = multi_line_logic_accumulate_line(mll, NULL, 0, (const guchar *) fourth_line, strlen(fourth_line));

  cr_assert(verdict & MLL_CONSUME_SEGMENT);
  cr_assert(verdict & MLL_EXTRACTED);

  multi_line_logic_free(mll);
}

GString *buffer;
GPtrArray *output_messages;

static void
_feed_line(MultiLineLogic *mll, const gchar *input, gssize input_len)
{
  gboolean repeat;

  if (input_len < 0)
    input_len = strlen(input);

  do
    {
      gint verdict = multi_line_logic_accumulate_line(mll, (const guchar *) buffer->str, buffer->len, (const guchar *) input,
                                                      input_len);

      repeat = FALSE;

      if (verdict & MLL_CONSUME_SEGMENT)
        {
          gint drop_length = (verdict & MLL_CONSUME_PARTIAL_AMOUNT_MASK) >> MLL_CONSUME_PARTIAL_AMOUNT_SHIFT;
          cr_assert(drop_length == 0);

          if (buffer->len > 0)
            g_string_append_c(buffer, '\n');

          g_string_append_len(buffer, input, input_len);
          if (verdict & MLL_EXTRACTED)
            {
              g_ptr_array_add(output_messages, g_strdup(buffer->str));
              g_string_truncate(buffer, 0);
            }
          else if (verdict & MLL_WAITING)
            ;
          else
            g_assert_not_reached();
        }
      else if (verdict & MLL_REWIND_SEGMENT)
        {
          if (verdict & MLL_EXTRACTED)
            {
              g_ptr_array_add(output_messages, g_strdup(buffer->str));
              g_string_truncate(buffer, 0);
              repeat = TRUE;
            }
          else if (verdict & MLL_WAITING)
            {
              repeat = TRUE;
            }
        }
    }
  while (repeat);
}

static void
_feed_lines(MultiLineLogic *mll, const gchar *messages[])
{
  for (gint i = 0; messages[i]; i++)
    {
      _feed_line(mll, messages[i], strlen(messages[i]));
    }
  _feed_line(mll, "ENDOFTEST", -1);
}

static const gchar *
_output_value(gint ndx)
{
  if (output_messages->len <= ndx)
    return "<unset>";
  const gchar *str = g_ptr_array_index(output_messages, ndx);
  return str;
}

static gboolean
_output_equals(gint ndx, const gchar *expected_value)
{
  if (output_messages->len <= ndx)
    return FALSE;

  const gchar *str = g_ptr_array_index(output_messages, ndx);

  return strcmp(str, expected_value) == 0;
}

Test(smart_multi_line, feed_smart_multi_line_with_single_and_multi_line_messages)
{
  MultiLineLogic *mll = smart_multi_line_new();
  const gchar *messages[] =
  {

    "this is something unrelated",
    "again something unrelated",
    "yet again something unrelated, but 3 tracebacks are COMING",
    "Traceback (most recent call last):",
    "File \"./lib/merge-grammar.py\", line 62, in <module>",
    "  for line in fileinput.input(openhook=fileinput.hook_encoded(\"utf-8\")):",
    "File \"/usr/lib/python3.8/fileinput.py\", line 248, in __next__",
    "  line = self._readline()",
    "File \"/usr/lib/python3.8/fileinput.py\", line 368, in _readline",
    "  return self._readline()",
    "Traceback (most recent call last):",
    "File \"./lib/merge-grammar2.py\", line 62, in <module>",
    "  for line in fileinput.input(openhook=fileinput.hook_encoded(\"utf-8\")):",
    "File \"/usr/lib/python3.8/fileinput.py\", line 248, in __next__",
    "  line = self._readline()",
    "File \"/usr/lib/python3.8/fileinput.py\", line 368, in _readline",
    "  return self._readline()",
    "Traceback (most recent call last):",
    "File \"./lib/merge-grammar3.py\", line 62, in <module>",
    "  for line in fileinput.input(openhook=fileinput.hook_encoded(\"utf-8\")):",
    "File \"/usr/lib/python3.8/fileinput.py\", line 248, in __next__",
    "  line = self._readline()",
    "File \"/usr/lib/python3.8/fileinput.py\", line 368, in _readline",
    "  return self._readline()",
    "unrelated line here",
    NULL,
  };

  _feed_lines(mll, messages);

  cr_assert(_output_equals(0, "this is something unrelated"));
  cr_assert(_output_equals(1, "again something unrelated"));
  cr_assert(_output_equals(2, "yet again something unrelated, but 3 tracebacks are COMING"));
  cr_assert(_output_equals(3, "Traceback (most recent call last):\n"
                           "File \"./lib/merge-grammar.py\", line 62, in <module>\n"
                           "  for line in fileinput.input(openhook=fileinput.hook_encoded(\"utf-8\")):\n"
                           "File \"/usr/lib/python3.8/fileinput.py\", line 248, in __next__\n"
                           "  line = self._readline()\n"
                           "File \"/usr/lib/python3.8/fileinput.py\", line 368, in _readline\n"
                           "  return self._readline()"),
            "unexpected_value %s", _output_value(3));

  cr_assert(_output_equals(4, "Traceback (most recent call last):\n"
                           "File \"./lib/merge-grammar2.py\", line 62, in <module>\n"
                           "  for line in fileinput.input(openhook=fileinput.hook_encoded(\"utf-8\")):\n"
                           "File \"/usr/lib/python3.8/fileinput.py\", line 248, in __next__\n"
                           "  line = self._readline()\n"
                           "File \"/usr/lib/python3.8/fileinput.py\", line 368, in _readline\n"
                           "  return self._readline()"),
            "unexpected_value %s", _output_value(4));

  cr_assert(_output_equals(5, "Traceback (most recent call last):\n"
                           "File \"./lib/merge-grammar3.py\", line 62, in <module>\n"
                           "  for line in fileinput.input(openhook=fileinput.hook_encoded(\"utf-8\")):\n"
                           "File \"/usr/lib/python3.8/fileinput.py\", line 248, in __next__\n"
                           "  line = self._readline()\n"
                           "File \"/usr/lib/python3.8/fileinput.py\", line 368, in _readline\n"
                           "  return self._readline()"),
            "unexpected_value %s", _output_value(5));
  cr_assert(_output_equals(6, "unrelated line here"));

  multi_line_logic_free(mll);

}

Test(smart_multi_line, test_python_backtrace)
{
  MultiLineLogic *mll = smart_multi_line_new();
  const gchar *messages[] =
  {
    "Traceback (most recent call last):",
    "File \"./lib/merge-grammar.py\", line 62, in <module>",
    "  for line in fileinput.input(openhook=fileinput.hook_encoded(\"utf-8\")):",
    "File \"/usr/lib/python3.8/fileinput.py\", line 248, in __next__",
    "  line = self._readline()",
    "File \"/usr/lib/python3.8/fileinput.py\", line 368, in _readline",
    "  return self._readline()",
    NULL,
  };

  _feed_lines(mll, messages);

  cr_assert(_output_equals(0, "Traceback (most recent call last):\n"
                           "File \"./lib/merge-grammar.py\", line 62, in <module>\n"
                           "  for line in fileinput.input(openhook=fileinput.hook_encoded(\"utf-8\")):\n"
                           "File \"/usr/lib/python3.8/fileinput.py\", line 248, in __next__\n"
                           "  line = self._readline()\n"
                           "File \"/usr/lib/python3.8/fileinput.py\", line 368, in _readline\n"
                           "  return self._readline()"),
            "unexpected_value %s", _output_value(0));

  multi_line_logic_free(mll);

}

Test(smart_multi_line, test_python_backtrace_with_tailing_exception_text)
{
  MultiLineLogic *mll = smart_multi_line_new();
  const gchar *messages[] =
  {
    "Traceback (most recent call last):",
    "File \"./lib/merge-grammar.py\", line 62, in <module>",
    "  for line in fileinput.input(openhook=fileinput.hook_encoded(\"utf-8\")):",
    "File \"/usr/lib/python3.8/fileinput.py\", line 248, in __next__",
    "  line = self._readline()",
    "File \"/usr/lib/python3.8/fileinput.py\", line 368, in _readline",
    "  return self._readline()",
    "ValueError: whatever exception that happened",
    NULL,
  };

  _feed_lines(mll, messages);

  cr_assert(_output_equals(0, "Traceback (most recent call last):\n"
                           "File \"./lib/merge-grammar.py\", line 62, in <module>\n"
                           "  for line in fileinput.input(openhook=fileinput.hook_encoded(\"utf-8\")):\n"
                           "File \"/usr/lib/python3.8/fileinput.py\", line 248, in __next__\n"
                           "  line = self._readline()\n"
                           "File \"/usr/lib/python3.8/fileinput.py\", line 368, in _readline\n"
                           "  return self._readline()\n"
                           "ValueError: whatever exception that happened"),
            "unexpected_value %s", _output_value(0));

  multi_line_logic_free(mll);
}


Test(smart_multi_line, test_java_backtrace)
{
  MultiLineLogic *mll = smart_multi_line_new();
  const gchar *messages[] =
  {
    "java.lang.RuntimeException: javax.mail.SendFailedException: Invalid Addresses;",
    "  nested exception is:",
    "com.sun.mail.smtp.SMTPAddressFailedException: 550 5.7.1 <[REDACTED_EMAIL_ADDRESS]>... Relaying denied",
    "	at com.nethunt.crm.api.server.adminsync.AutomaticEmailFacade.sendWithSmtp(AutomaticEmailFacade.java:236)",
    "	at com.nethunt.crm.api.server.adminsync.AutomaticEmailFacade.sendSingleEmail(AutomaticEmailFacade.java:285)",
    "	at com.nethunt.crm.api.server.adminsync.AutomaticEmailFacade.lambda$sendSingleEmail$3(AutomaticEmailFacade.java:254)",
    "	at java.util.Optional.ifPresent(Optional.java:159)",
    "	at com.nethunt.crm.api.server.adminsync.AutomaticEmailFacade.sendSingleEmail(AutomaticEmailFacade.java:253)",
    "	at com.nethunt.crm.api.server.adminsync.AutomaticEmailFacade.sendSingleEmail(AutomaticEmailFacade.java:249)",
    "	at com.nethunt.crm.api.email.EmailSender.lambda$notifyPerson$0(EmailSender.java:80)",
    "	at com.nethunt.crm.api.util.ManagedExecutor.lambda$execute$0(ManagedExecutor.java:36)",
    "	at com.nethunt.crm.api.util.RequestContextActivator.lambda$withRequestContext$0(RequestContextActivator.java:36)",
    "	at java.base/java.util.concurrent.ThreadPoolExecutor.runWorker(ThreadPoolExecutor.java:1149)",
    "	at java.base/java.util.concurrent.ThreadPoolExecutor$Worker.run(ThreadPoolExecutor.java:624)",
    "	at java.base/java.lang.Thread.run(Thread.java:748)",
    "Caused by: javax.mail.SendFailedException: Invalid Addresses;",
    "  nested exception is:",
    "com.sun.mail.smtp.SMTPAddressFailedException: 550 5.7.1 <[REDACTED_EMAIL_ADDRESS]>... Relaying denied",
    "	at com.sun.mail.smtp.SMTPTransport.rcptTo(SMTPTransport.java:2064)",
    "	at com.sun.mail.smtp.SMTPTransport.sendMessage(SMTPTransport.java:1286)",
    "	at com.nethunt.crm.api.server.adminsync.AutomaticEmailFacade.sendWithSmtp(AutomaticEmailFacade.java:229)",
    "	... 12 more",
    "Caused by: com.sun.mail.smtp.SMTPAddressFailedException: 550 5.7.1 <[REDACTED_EMAIL_ADDRESS]>... Relaying denied",
    NULL
  };

  _feed_lines(mll, messages);

  cr_assert(_output_equals(0, "java.lang.RuntimeException: javax.mail.SendFailedException: Invalid Addresses;\n"
                           "  nested exception is:\n"
                           "com.sun.mail.smtp.SMTPAddressFailedException: 550 5.7.1 <[REDACTED_EMAIL_ADDRESS]>... Relaying denied\n"
                           "	at com.nethunt.crm.api.server.adminsync.AutomaticEmailFacade.sendWithSmtp(AutomaticEmailFacade.java:236)\n"
                           "	at com.nethunt.crm.api.server.adminsync.AutomaticEmailFacade.sendSingleEmail(AutomaticEmailFacade.java:285)\n"
                           "	at com.nethunt.crm.api.server.adminsync.AutomaticEmailFacade.lambda$sendSingleEmail$3(AutomaticEmailFacade.java:254)\n"
                           "	at java.util.Optional.ifPresent(Optional.java:159)\n"
                           "	at com.nethunt.crm.api.server.adminsync.AutomaticEmailFacade.sendSingleEmail(AutomaticEmailFacade.java:253)\n"
                           "	at com.nethunt.crm.api.server.adminsync.AutomaticEmailFacade.sendSingleEmail(AutomaticEmailFacade.java:249)\n"
                           "	at com.nethunt.crm.api.email.EmailSender.lambda$notifyPerson$0(EmailSender.java:80)\n"
                           "	at com.nethunt.crm.api.util.ManagedExecutor.lambda$execute$0(ManagedExecutor.java:36)\n"
                           "	at com.nethunt.crm.api.util.RequestContextActivator.lambda$withRequestContext$0(RequestContextActivator.java:36)\n"
                           "	at java.base/java.util.concurrent.ThreadPoolExecutor.runWorker(ThreadPoolExecutor.java:1149)\n"
                           "	at java.base/java.util.concurrent.ThreadPoolExecutor$Worker.run(ThreadPoolExecutor.java:624)\n"
                           "	at java.base/java.lang.Thread.run(Thread.java:748)\n"
                           "Caused by: javax.mail.SendFailedException: Invalid Addresses;\n"
                           "  nested exception is:\n"
                           "com.sun.mail.smtp.SMTPAddressFailedException: 550 5.7.1 <[REDACTED_EMAIL_ADDRESS]>... Relaying denied\n"
                           "	at com.sun.mail.smtp.SMTPTransport.rcptTo(SMTPTransport.java:2064)\n"
                           "	at com.sun.mail.smtp.SMTPTransport.sendMessage(SMTPTransport.java:1286)\n"
                           "	at com.nethunt.crm.api.server.adminsync.AutomaticEmailFacade.sendWithSmtp(AutomaticEmailFacade.java:229)\n"
                           "	... 12 more\n"
                           "Caused by: com.sun.mail.smtp.SMTPAddressFailedException: 550 5.7.1 <[REDACTED_EMAIL_ADDRESS]>... Relaying denied"),
            "unexpected_value %s", _output_value(0));


  multi_line_logic_free(mll);
}

Test(smart_multi_line, test_php_backtrace)
{
  MultiLineLogic *mll = smart_multi_line_new();
  const gchar *messages[] =
  {
    "exception 'Exception' with message 'Custom exception' in /home/joe/work/test-php/test.php:5",
    "Stack trace:",
    "#0 /home/joe/work/test-php/test.php(9): func1()",
    "#1 /home/joe/work/test-php/test.php(13): func2()",
    "#2 {main}",

    "PHP Fatal error:  Uncaught exception 'Exception' with message 'message' in /base/data/home/apps/s~crash-example-php/1.388306779641080894/errors.php:60",
    "Stack trace:",
    "#0 [internal function]: ErrorEntryGenerator::{closure}()",
    "#1 /base/data/home/apps/s~crash-example-php/1.388306779641080894/errors.php(20): call_user_func_array(Object(Closure), Array)",
    "#2 /base/data/home/apps/s~crash-example-php/1.388306779641080894/index.php(36): ErrorEntry->__call('raise', Array)",
    "#3 /base/data/home/apps/s~crash-example-php/1.388306779641080894/index.php(36): ErrorEntry->raise()",
    "#4 {main}",
    "  thrown in /base/data/home/apps/s~crash-example-php/1.388306779641080894/errors.php on line 60",
    NULL,
  };

  _feed_lines(mll, messages);

  cr_assert(_output_equals(0,
                           "exception 'Exception' with message 'Custom exception' in /home/joe/work/test-php/test.php:5\n"
                           "Stack trace:\n"
                           "#0 /home/joe/work/test-php/test.php(9): func1()\n"
                           "#1 /home/joe/work/test-php/test.php(13): func2()\n"
                           "#2 {main}"),
            "unexpected_value %s", _output_value(0));
  cr_assert(_output_equals(1,
                           "PHP Fatal error:  Uncaught exception 'Exception' with message 'message' in /base/data/home/apps/s~crash-example-php/1.388306779641080894/errors.php:60\n"
                           "Stack trace:\n"
                           "#0 [internal function]: ErrorEntryGenerator::{closure}()\n"
                           "#1 /base/data/home/apps/s~crash-example-php/1.388306779641080894/errors.php(20): call_user_func_array(Object(Closure), Array)\n"
                           "#2 /base/data/home/apps/s~crash-example-php/1.388306779641080894/index.php(36): ErrorEntry->__call('raise', Array)\n"
                           "#3 /base/data/home/apps/s~crash-example-php/1.388306779641080894/index.php(36): ErrorEntry->raise()\n"
                           "#4 {main}\n"
                           "  thrown in /base/data/home/apps/s~crash-example-php/1.388306779641080894/errors.php on line 60"),
            "unexpected_value %s", _output_value(1));


  multi_line_logic_free(mll);
}

Test(smart_multi_line, test_js_backtrace)
{
  MultiLineLogic *mll = smart_multi_line_new();
  const gchar *messages[] =
  {
    "ReferenceError: myArray is not defined",
    "  at next (/app/node_modules/express/lib/router/index.js:256:14)",
    "  at /app/node_modules/express/lib/router/index.js:615:15",
    "  at next (/app/node_modules/express/lib/router/index.js:271:10)",
    "  at Function.process_params (/app/node_modules/express/lib/router/index.js:330:12)",
    "  at /app/node_modules/express/lib/router/index.js:277:22",
    "  at Layer.handle [as handle_request] (/app/node_modules/express/lib/router/layer.js:95:5)",
    "  at Route.dispatch (/app/node_modules/express/lib/router/route.js:112:3)",
    "  at next (/app/node_modules/express/lib/router/route.js:131:13)",
    "  at Layer.handle [as handle_request] (/app/node_modules/express/lib/router/layer.js:95:5)",
    "  at /app/app.js:52:3",
    NULL,
  };

  _feed_lines(mll, messages);

  cr_assert(_output_equals(0, "ReferenceError: myArray is not defined\n"
                           "  at next (/app/node_modules/express/lib/router/index.js:256:14)\n"
                           "  at /app/node_modules/express/lib/router/index.js:615:15\n"
                           "  at next (/app/node_modules/express/lib/router/index.js:271:10)\n"
                           "  at Function.process_params (/app/node_modules/express/lib/router/index.js:330:12)\n"
                           "  at /app/node_modules/express/lib/router/index.js:277:22\n"
                           "  at Layer.handle [as handle_request] (/app/node_modules/express/lib/router/layer.js:95:5)\n"
                           "  at Route.dispatch (/app/node_modules/express/lib/router/route.js:112:3)\n"
                           "  at next (/app/node_modules/express/lib/router/route.js:131:13)\n"
                           "  at Layer.handle [as handle_request] (/app/node_modules/express/lib/router/layer.js:95:5)\n"
                           "  at /app/app.js:52:3"),
            "unexpected_value %s", _output_value(0));


  multi_line_logic_free(mll);
}

Test(smart_multi_line, test_go_backtrace)
{
  MultiLineLogic *mll = smart_multi_line_new();
  const gchar *messages[] =
  {
    "panic: my panic",
    "",
    "goroutine 4 [running]:",
    "panic(0x45cb40, 0x47ad70)",
    "	/usr/local/go/src/runtime/panic.go:542 +0x46c fp=0xc42003f7b8 sp=0xc42003f710 pc=0x422f7c",
    "main.main.func1(0xc420024120)",
    "	foo.go:6 +0x39 fp=0xc42003f7d8 sp=0xc42003f7b8 pc=0x451339",
    "runtime.goexit()",
    "	/usr/local/go/src/runtime/asm_amd64.s:2337 +0x1 fp=0xc42003f7e0 sp=0xc42003f7d8 pc=0x44b4d1",
    "created by main.main",
    "	foo.go:5 +0x58",
    "",
    "goroutine 1 [chan receive]:",
    "runtime.gopark(0x4739b8, 0xc420024178, 0x46fcd7, 0xc, 0xc420028e17, 0x3)",
    "	/usr/local/go/src/runtime/proc.go:280 +0x12c fp=0xc420053e30 sp=0xc420053e00 pc=0x42503c",
    "runtime.goparkunlock(0xc420024178, 0x46fcd7, 0xc, 0x1000f010040c217, 0x3)",
    "	/usr/local/go/src/runtime/proc.go:286 +0x5e fp=0xc420053e70 sp=0xc420053e30 pc=0x42512e",
    "runtime.chanrecv(0xc420024120, 0x0, 0xc420053f01, 0x4512d8)",
    "	/usr/local/go/src/runtime/chan.go:506 +0x304 fp=0xc420053f20 sp=0xc420053e70 pc=0x4046b4",
    "runtime.chanrecv1(0xc420024120, 0x0)",
    "	/usr/local/go/src/runtime/chan.go:388 +0x2b fp=0xc420053f50 sp=0xc420053f20 pc=0x40439b",
    "main.main()",
    "	foo.go:9 +0x6f fp=0xc420053f80 sp=0xc420053f50 pc=0x4512ef",
    "runtime.main()",
    "	/usr/local/go/src/runtime/proc.go:185 +0x20d fp=0xc420053fe0 sp=0xc420053f80 pc=0x424bad",
    "runtime.goexit()",
    "	/usr/local/go/src/runtime/asm_amd64.s:2337 +0x1 fp=0xc420053fe8 sp=0xc420053fe0 pc=0x44b4d1",
    "",
    "goroutine 2 [force gc (idle)]:",
    "runtime.gopark(0x4739b8, 0x4ad720, 0x47001e, 0xf, 0x14, 0x1)",
    "	/usr/local/go/src/runtime/proc.go:280 +0x12c fp=0xc42003e768 sp=0xc42003e738 pc=0x42503c",
    "runtime.goparkunlock(0x4ad720, 0x47001e, 0xf, 0xc420000114, 0x1)",
    "	/usr/local/go/src/runtime/proc.go:286 +0x5e fp=0xc42003e7a8 sp=0xc42003e768 pc=0x42512e",
    "runtime.forcegchelper()",
    "	/usr/local/go/src/runtime/proc.go:238 +0xcc fp=0xc42003e7e0 sp=0xc42003e7a8 pc=0x424e5c",
    "runtime.goexit()",
    "	/usr/local/go/src/runtime/asm_amd64.s:2337 +0x1 fp=0xc42003e7e8 sp=0xc42003e7e0 pc=0x44b4d1",
    "created by runtime.init.4",
    "	/usr/local/go/src/runtime/proc.go:227 +0x35",
    "",
    "goroutine 3 [GC sweep wait]:",
    "runtime.gopark(0x4739b8, 0x4ad7e0, 0x46fdd2, 0xd, 0x419914, 0x1)",
    "	/usr/local/go/src/runtime/proc.go:280 +0x12c fp=0xc42003ef60 sp=0xc42003ef30 pc=0x42503c",
    "runtime.goparkunlock(0x4ad7e0, 0x46fdd2, 0xd, 0x14, 0x1)",
    "	/usr/local/go/src/runtime/proc.go:286 +0x5e fp=0xc42003efa0 sp=0xc42003ef60 pc=0x42512e",
    "runtime.bgsweep(0xc42001e150)",
    "	/usr/local/go/src/runtime/mgcsweep.go:52 +0xa3 fp=0xc42003efd8 sp=0xc42003efa0 pc=0x419973",
    "runtime.goexit()",
    "	/usr/local/go/src/runtime/asm_amd64.s:2337 +0x1 fp=0xc42003efe0 sp=0xc42003efd8 pc=0x44b4d1",
    "created by runtime.gcenable",
    "	/usr/local/go/src/runtime/mgc.go:216 +0x58",
    NULL,
  };

  _feed_lines(mll, messages);

  cr_assert(_output_equals(0, "panic: my panic\n"
                           "\n"
                           "goroutine 4 [running]:\n"
                           "panic(0x45cb40, 0x47ad70)\n"
                           "	/usr/local/go/src/runtime/panic.go:542 +0x46c fp=0xc42003f7b8 sp=0xc42003f710 pc=0x422f7c\n"
                           "main.main.func1(0xc420024120)\n"
                           "	foo.go:6 +0x39 fp=0xc42003f7d8 sp=0xc42003f7b8 pc=0x451339\n"
                           "runtime.goexit()\n"
                           "	/usr/local/go/src/runtime/asm_amd64.s:2337 +0x1 fp=0xc42003f7e0 sp=0xc42003f7d8 pc=0x44b4d1\n"
                           "created by main.main\n"
                           "	foo.go:5 +0x58\n"
                           "\n"
                           "goroutine 1 [chan receive]:\n"
                           "runtime.gopark(0x4739b8, 0xc420024178, 0x46fcd7, 0xc, 0xc420028e17, 0x3)\n"
                           "	/usr/local/go/src/runtime/proc.go:280 +0x12c fp=0xc420053e30 sp=0xc420053e00 pc=0x42503c\n"
                           "runtime.goparkunlock(0xc420024178, 0x46fcd7, 0xc, 0x1000f010040c217, 0x3)\n"
                           "	/usr/local/go/src/runtime/proc.go:286 +0x5e fp=0xc420053e70 sp=0xc420053e30 pc=0x42512e\n"
                           "runtime.chanrecv(0xc420024120, 0x0, 0xc420053f01, 0x4512d8)\n"
                           "	/usr/local/go/src/runtime/chan.go:506 +0x304 fp=0xc420053f20 sp=0xc420053e70 pc=0x4046b4\n"
                           "runtime.chanrecv1(0xc420024120, 0x0)\n"
                           "	/usr/local/go/src/runtime/chan.go:388 +0x2b fp=0xc420053f50 sp=0xc420053f20 pc=0x40439b\n"
                           "main.main()\n"
                           "	foo.go:9 +0x6f fp=0xc420053f80 sp=0xc420053f50 pc=0x4512ef\n"
                           "runtime.main()\n"
                           "	/usr/local/go/src/runtime/proc.go:185 +0x20d fp=0xc420053fe0 sp=0xc420053f80 pc=0x424bad\n"
                           "runtime.goexit()\n"
                           "	/usr/local/go/src/runtime/asm_amd64.s:2337 +0x1 fp=0xc420053fe8 sp=0xc420053fe0 pc=0x44b4d1\n"
                           "\n"
                           "goroutine 2 [force gc (idle)]:\n"
                           "runtime.gopark(0x4739b8, 0x4ad720, 0x47001e, 0xf, 0x14, 0x1)\n"
                           "	/usr/local/go/src/runtime/proc.go:280 +0x12c fp=0xc42003e768 sp=0xc42003e738 pc=0x42503c\n"
                           "runtime.goparkunlock(0x4ad720, 0x47001e, 0xf, 0xc420000114, 0x1)\n"
                           "	/usr/local/go/src/runtime/proc.go:286 +0x5e fp=0xc42003e7a8 sp=0xc42003e768 pc=0x42512e\n"
                           "runtime.forcegchelper()\n"
                           "	/usr/local/go/src/runtime/proc.go:238 +0xcc fp=0xc42003e7e0 sp=0xc42003e7a8 pc=0x424e5c\n"
                           "runtime.goexit()\n"
                           "	/usr/local/go/src/runtime/asm_amd64.s:2337 +0x1 fp=0xc42003e7e8 sp=0xc42003e7e0 pc=0x44b4d1\n"
                           "created by runtime.init.4\n"
                           "	/usr/local/go/src/runtime/proc.go:227 +0x35\n"
                           "\n"
                           "goroutine 3 [GC sweep wait]:\n"
                           "runtime.gopark(0x4739b8, 0x4ad7e0, 0x46fdd2, 0xd, 0x419914, 0x1)\n"
                           "	/usr/local/go/src/runtime/proc.go:280 +0x12c fp=0xc42003ef60 sp=0xc42003ef30 pc=0x42503c\n"
                           "runtime.goparkunlock(0x4ad7e0, 0x46fdd2, 0xd, 0x14, 0x1)\n"
                           "	/usr/local/go/src/runtime/proc.go:286 +0x5e fp=0xc42003efa0 sp=0xc42003ef60 pc=0x42512e\n"
                           "runtime.bgsweep(0xc42001e150)\n"
                           "	/usr/local/go/src/runtime/mgcsweep.go:52 +0xa3 fp=0xc42003efd8 sp=0xc42003efa0 pc=0x419973\n"
                           "runtime.goexit()\n"
                           "	/usr/local/go/src/runtime/asm_amd64.s:2337 +0x1 fp=0xc42003efe0 sp=0xc42003efd8 pc=0x44b4d1\n"
                           "created by runtime.gcenable\n"
                           "	/usr/local/go/src/runtime/mgc.go:216 +0x58"),
            "unexpected_value %s", _output_value(0));


  multi_line_logic_free(mll);
}


Test(smart_multi_line, test_ruby_backtrace)
{
  MultiLineLogic *mll = smart_multi_line_new();
  const gchar *messages[] =
  {

    " NoMethodError (undefined method `resursivewordload' for #<BooksController:0x007f8dd9a0c738>):",
    "  app/controllers/books_controller.rb:69:in `recursivewordload'",
    "  app/controllers/books_controller.rb:75:in `loadword'",
    "  app/controllers/books_controller.rb:79:in `loadline'",
    "  app/controllers/books_controller.rb:83:in `loadparagraph'",
    "  app/controllers/books_controller.rb:87:in `loadpage'",
    "  app/controllers/books_controller.rb:91:in `onload'",
    "  app/controllers/books_controller.rb:95:in `loadrecursive'",
    "  app/controllers/books_controller.rb:99:in `requestload'",
    "  app/controllers/books_controller.rb:118:in `generror'",
    "  config/error_reporting_logger.rb:62:in `tagged'",

    " ActionController::RoutingError (No route matches [GET] \"/settings\"):",
    "  ",
    "  actionpack (5.1.4) lib/action_dispatch/middleware/debug_exceptions.rb:63:in `call'",
    "  actionpack (5.1.4) lib/action_dispatch/middleware/show_exceptions.rb:31:in `call'",
    "  railties (5.1.4) lib/rails/rack/logger.rb:36:in `call_app'",
    "  railties (5.1.4) lib/rails/rack/logger.rb:24:in `block in call'",
    "  activesupport (5.1.4) lib/active_support/tagged_logging.rb:69:in `block in tagged'",
    "  activesupport (5.1.4) lib/active_support/tagged_logging.rb:26:in `tagged'",
    "  activesupport (5.1.4) lib/active_support/tagged_logging.rb:69:in `tagged'",
    "  railties (5.1.4) lib/rails/rack/logger.rb:24:in `call'",
    "  actionpack (5.1.4) lib/action_dispatch/middleware/remote_ip.rb:79:in `call'",
    "  actionpack (5.1.4) lib/action_dispatch/middleware/request_id.rb:25:in `call'",
    "  rack (2.0.3) lib/rack/method_override.rb:22:in `call'",
    "  rack (2.0.3) lib/rack/runtime.rb:22:in `call'",
    "  activesupport (5.1.4) lib/active_support/cache/strategy/local_cache_middleware.rb:27:in `call'",
    "  actionpack (5.1.4) lib/action_dispatch/middleware/executor.rb:12:in `call'",
    "  rack (2.0.3) lib/rack/sendfile.rb:111:in `call'",
    "  railties (5.1.4) lib/rails/engine.rb:522:in `call'",
    "  puma (3.10.0) lib/puma/configuration.rb:225:in `call'",
    "  puma (3.10.0) lib/puma/server.rb:605:in `handle_request'",
    "  puma (3.10.0) lib/puma/server.rb:437:in `process_client'",
    "  puma (3.10.0) lib/puma/server.rb:301:in `block in run'",
    "  puma (3.10.0) lib/puma/thread_pool.rb:120:in `block in spawn_thread'",
    NULL,
  };

  _feed_lines(mll, messages);

  cr_assert(_output_equals(0,
                           " NoMethodError (undefined method `resursivewordload' for #<BooksController:0x007f8dd9a0c738>):\n"
                           "  app/controllers/books_controller.rb:69:in `recursivewordload'\n"
                           "  app/controllers/books_controller.rb:75:in `loadword'\n"
                           "  app/controllers/books_controller.rb:79:in `loadline'\n"
                           "  app/controllers/books_controller.rb:83:in `loadparagraph'\n"
                           "  app/controllers/books_controller.rb:87:in `loadpage'\n"
                           "  app/controllers/books_controller.rb:91:in `onload'\n"
                           "  app/controllers/books_controller.rb:95:in `loadrecursive'\n"
                           "  app/controllers/books_controller.rb:99:in `requestload'\n"
                           "  app/controllers/books_controller.rb:118:in `generror'\n"
                           "  config/error_reporting_logger.rb:62:in `tagged'"),
            "unexpected_value %s", _output_value(0));

  cr_assert(_output_equals(1, " ActionController::RoutingError (No route matches [GET] \"/settings\"):\n"
                           "  \n"
                           "  actionpack (5.1.4) lib/action_dispatch/middleware/debug_exceptions.rb:63:in `call'\n"
                           "  actionpack (5.1.4) lib/action_dispatch/middleware/show_exceptions.rb:31:in `call'\n"
                           "  railties (5.1.4) lib/rails/rack/logger.rb:36:in `call_app'\n"
                           "  railties (5.1.4) lib/rails/rack/logger.rb:24:in `block in call'\n"
                           "  activesupport (5.1.4) lib/active_support/tagged_logging.rb:69:in `block in tagged'\n"
                           "  activesupport (5.1.4) lib/active_support/tagged_logging.rb:26:in `tagged'\n"
                           "  activesupport (5.1.4) lib/active_support/tagged_logging.rb:69:in `tagged'\n"
                           "  railties (5.1.4) lib/rails/rack/logger.rb:24:in `call'\n"
                           "  actionpack (5.1.4) lib/action_dispatch/middleware/remote_ip.rb:79:in `call'\n"
                           "  actionpack (5.1.4) lib/action_dispatch/middleware/request_id.rb:25:in `call'\n"
                           "  rack (2.0.3) lib/rack/method_override.rb:22:in `call'\n"
                           "  rack (2.0.3) lib/rack/runtime.rb:22:in `call'\n"
                           "  activesupport (5.1.4) lib/active_support/cache/strategy/local_cache_middleware.rb:27:in `call'\n"
                           "  actionpack (5.1.4) lib/action_dispatch/middleware/executor.rb:12:in `call'\n"
                           "  rack (2.0.3) lib/rack/sendfile.rb:111:in `call'\n"
                           "  railties (5.1.4) lib/rails/engine.rb:522:in `call'\n"
                           "  puma (3.10.0) lib/puma/configuration.rb:225:in `call'\n"
                           "  puma (3.10.0) lib/puma/server.rb:605:in `handle_request'\n"
                           "  puma (3.10.0) lib/puma/server.rb:437:in `process_client'\n"
                           "  puma (3.10.0) lib/puma/server.rb:301:in `block in run'\n"
                           "  puma (3.10.0) lib/puma/thread_pool.rb:120:in `block in spawn_thread'"),
            "unexpected_value %s", _output_value(1));


  multi_line_logic_free(mll);
}

Test(smart_multi_line, test_dart_backtrace)
{
  MultiLineLogic *mll = smart_multi_line_new();
  const gchar *messages[] =
  {
    "Unhandled exception:",
    "RangeError (index): Invalid value: Valid value range is empty: 1",
    "#0      List.[] (dart:core-patch/growable_array.dart:151)",
    "#1      main.<anonymous closure> (file:///path/to/code/dartFile.dart:31:23)",
    "#2      printError (file:///path/to/code/dartFile.dart:42:13)",
    "#3      main (file:///path/to/code/dartFile.dart:29:3)",
    "#4      _startIsolate.<anonymous closure> (dart:isolate-patch/isolate_patch.dart:265)",
    "#5      _RawReceivePortImpl._handleMessage (dart:isolate-patch/isolate_patch.dart:151)",
    NULL,
  };

  _feed_lines(mll, messages);

  cr_assert(_output_equals(0,
                           "Unhandled exception:\n"
                           "RangeError (index): Invalid value: Valid value range is empty: 1\n"
                           "#0      List.[] (dart:core-patch/growable_array.dart:151)\n"
                           "#1      main.<anonymous closure> (file:///path/to/code/dartFile.dart:31:23)\n"
                           "#2      printError (file:///path/to/code/dartFile.dart:42:13)\n"
                           "#3      main (file:///path/to/code/dartFile.dart:29:3)\n"
                           "#4      _startIsolate.<anonymous closure> (dart:isolate-patch/isolate_patch.dart:265)\n"
                           "#5      _RawReceivePortImpl._handleMessage (dart:isolate-patch/isolate_patch.dart:151)"),
            "unexpected_value %s", _output_value(0));


  multi_line_logic_free(mll);
}


void
setup(void)
{
  override_installation_path_for("${pkgdatadir}/smart-multi-line.fsm", TOP_SRCDIR "/lib/multi-line/smart-multi-line.fsm");
  app_startup();

  configuration = cfg_new_snippet();
  buffer = g_string_sized_new(256);
  output_messages = g_ptr_array_new();
}

void
teardown(void)
{
  g_string_free(buffer, TRUE);
  g_ptr_array_foreach(output_messages, (GFunc) g_free, NULL);
  g_ptr_array_free(output_messages, TRUE);
  scratch_buffers_explicit_gc();
  app_shutdown();
}

TestSuite(smart_multi_line, .init = setup, .fini = teardown);
