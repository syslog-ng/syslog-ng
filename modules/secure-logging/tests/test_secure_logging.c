/*
 * Copyright (c) 2019 Airbus Commercial Aircraft
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

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <criterion/criterion.h>

#include "apphook.h"
#include "cfg.h"
#include "logmatcher.h"
#include "libtest/cr_template.h"
#include "libtest/msg_parse_lib.h"
#include "libtest/stopwatch.h"
#include "timeutils/misc.h"

// Secure logging functions
#include "slog.h"

#define MAX_TEST_MESSAGES 1000
#define MIN_TEST_MESSAGES 10
#define PERFORMANCE_COUNTER 100000

// Local parse options
static MsgFormatOptions test_parse_options;

// Filenames and directory templates
static gchar *testDirTmpl = "/tmp/slog-XXXXXX/";
static gchar *hostKeyFile = "host.key";
static gchar *macFile = "mac.dat";

// Test data for secure logging
static gchar *macAddr = "a08cefa7b520";
static gchar *serial = "CAC7119N43";
static gchar *prefix = "slog/";
static gchar *context_id = "test-context-id";

// Data needed to run a test
typedef struct _testData
{
  guchar hostKey[KEY_LENGTH];
  GString *testName;
  GString *keyFile;
  GString *macFile;
  GString *testDir;
} TestData;

/*************************************************************************/
/* Utility functions needed for testing the secure logging functionality */
/*************************************************************************/

// Generate random number between low and high
int randomNumber(int low, int high)
{
  return rand()%((high+1)-low)+low;
}

// Generate a sample message with a fixed and a random part
LogMessage *create_random_sample_message(void)
{
  LogMessage *msg;

  GString *msg_str = g_string_new("<155>2019-07-11T10:34:56+01:00 aicorp syslog-ng[23323]:");

  // Append a random string
  int num = randomNumber(10, 500);
  for (int i = 0; i<num; i++)
    {
      // 65 to 90 are upper case letters
      g_string_append_c(msg_str, randomNumber(65, 90));
    }

  msg = log_msg_new(msg_str->str, msg_str->len, &test_parse_options);
  log_msg_set_saddr_ref(msg, g_sockaddr_inet_new("10.11.12.13", 1010));
  log_msg_set_match(msg, 0, "whole-match", -1);
  log_msg_set_match(msg, 1, "first-match", -1);
  log_msg_set_tag_by_name(msg, "alma");
  log_msg_set_tag_by_name(msg, "korte");
  log_msg_clear_tag_by_name(msg, "narancs");
  log_msg_set_tag_by_name(msg, "citrom");
  log_msg_set_tag_by_name(msg, "tag,containing,comma");
  msg->rcptid = 555;
  msg->host_id = 0xcafebabe;

  // Fix some externally or automatically defined values
  log_msg_set_value(msg, LM_V_HOST_FROM, "kismacska", -1);
  msg->timestamps[LM_TS_RECVD].ut_sec = 1139684315;
  msg->timestamps[LM_TS_RECVD].ut_usec = 639000;
  msg->timestamps[LM_TS_RECVD].ut_gmtoff = get_local_timezone_ofs(1139684315);

  g_string_free(msg_str, TRUE);
  return msg;
}

// Create a slog template instance
LogTemplate *createTemplate(TestData *testData)
{
  GString *slog_templ_str = g_string_new("slog");

  // Initialize the template
  g_string_printf(slog_templ_str, "$(slog -k %s -m %s $RAWMSG)", testData->keyFile->str, testData->macFile->str);

  const gboolean escaping = FALSE;

  LogTemplate *slog_templ = compile_template(slog_templ_str->str, escaping);

  cr_assert(slog_templ != NULL, "Template '%s' does not compile correctly", slog_templ_str->str);

  g_string_free(slog_templ_str, TRUE);

  return slog_templ;
}

// Create a collection of random log messages for testing purposes
void createLogMessages(gint num, LogMessage **log)
{
  if(num <= 0)
    {
      cr_log_error("Invalid argument passed to createLog. num = %d", num);
    }

  for(int i = 0; i < num; i++)
    {
      log[i] = create_random_sample_message();
    }
}

// Apply the template to a single log message
GString *applyTemplate(LogTemplate *templ, LogMessage *msg)
{
  GString *output = g_string_new(prefix);

  LogTemplateEvalOptions options = {NULL, LTZ_LOCAL, 999, context_id};
  // Execute secure logging template
  log_template_append_format_with_context(
    templ,       // Secure logging template
    &msg,        // Message(s) to pass to the template
    1,           // Number of message to pass to the template
    &options,
    output);     // Output string after applying the template

  return output;
}

// Find an integer in an array of integers
int findInArray(int index, int *buffer, int size)
{
  for (int i = 0; i<size; i++)
    {
      if (buffer[i]==index)
        {
          return 1;
        }
    }
  return 0;
}

// Verify messages with malicious modification and detect which entry is corrupted
GString **verifyMaliciousMessages(guchar *hostkey, gchar *macFileName, GString **templateOutput,
                                  size_t totalNumberOfMessages, int *brokenEntries)
{
  unsigned char keyZero[KEY_LENGTH];
  memcpy(keyZero, hostkey, KEY_LENGTH);

  GHashTable *tab = NULL;

  guint64 next = 0;
  guint64 start = 0;
  guint64 numberOfLogEntries = 0UL;

  GString **outputBuffer = g_new0(GString *, totalNumberOfMessages);

  gchar mac[CMAC_LENGTH];

  int ret = readBigMAC(macFileName, mac);
  cr_assert(ret == 1, "Unable to read aggregated MAC from file %s", macFileName);
  int problemsFound = 0;
  unsigned char cmac_tag[CMAC_LENGTH];
  initVerify(totalNumberOfMessages, hostkey, &next, &start, templateOutput, &tab);

  for (int i = 0; i<totalNumberOfMessages; i++)
    {
      ret = iterateBuffer(1, &templateOutput[i], &next, hostkey, keyZero, 0, &outputBuffer[i], &numberOfLogEntries, cmac_tag,
                          tab);
      if (ret == 0)
        {
          brokenEntries[problemsFound] = i;
          problemsFound++;
        }
    }
  ret = finalizeVerify(start, totalNumberOfMessages, (guchar *)mac, cmac_tag, tab);

  cr_assert(ret == 0, "Aggregated MAC is correct.");

  return outputBuffer;
}

// Verify log messages and compare them with the original
void verifyMessages(guchar *hostkey, gchar *macFileName, GString **templateOutput, LogMessage **original,
                    size_t totalNumberOfMessages)
{
  unsigned char keyZero[KEY_LENGTH];
  memcpy(keyZero, hostkey, KEY_LENGTH);

  GHashTable *tab = NULL;

  guint64 next = 0;
  guint64 start = 0;
  guint64 numberOfLogEntries = 0UL;

  GString **outputBuffer = g_new0(GString *, totalNumberOfMessages);

  gchar mac[CMAC_LENGTH];

  int ret =  readBigMAC(macFileName, mac);
  cr_assert(ret == 1, "Unable to read aggregated MAC from file %s", macFileName);

  unsigned char cmac_tag[CMAC_LENGTH];
  ret = initVerify(totalNumberOfMessages, hostkey, &next, &start, templateOutput, &tab);
  cr_assert(ret == 1, "initVerify failed");

  ret = iterateBuffer(totalNumberOfMessages, templateOutput, &next, hostkey, keyZero, 0, outputBuffer,
                      &numberOfLogEntries, cmac_tag, tab);
  cr_assert(ret == 1, "iterateBuffer failed");

  ret = finalizeVerify(start, totalNumberOfMessages, (guchar *)mac, cmac_tag, tab);
  cr_assert(ret == 1, "finalizeVerify failed");


  for (int i=0; i<totalNumberOfMessages; i++)
    {
      char *plaintextMessage = (outputBuffer[i]->str) + CTR_LEN_SIMPLE + COLON + BLANK;
      LogMessage *result = log_msg_new(plaintextMessage, strlen(plaintextMessage), &test_parse_options);
      log_msg_set_saddr(result, original[i]->saddr);
      assert_log_messages_equal(original[i], result);
      g_string_free(outputBuffer[i], TRUE);
      log_msg_unref(result);
    }

  g_free(outputBuffer);
}

// Generate keys to be used for the tests
void generateHostKey(guchar *hostkey, gchar *hostKeyFileName)
{
  // Create keys for the test
  guchar masterkey[KEY_LENGTH];
  int ret = generateMasterKey(masterkey);
  cr_assert(ret == 1, "Unable to generate master key");

  ret = deriveHostKey(masterkey, macAddr, serial, hostkey);
  cr_assert(ret == 1, "Unable to derive host key from master key for addr %s and serial number %s", macAddr, serial);

  ret = writeKey((gchar *)hostkey, 0, hostKeyFileName);
  cr_assert(ret == 1, "Unable to write host key to file %s", hostKeyFileName);
}

// Create a temporary directory
GString *createTemporaryDirectory(gchar *template)
{
  gchar buf[PATH_MAX];

  // Buffer for temporary path
  g_strlcpy(buf, template, strlen(template)+1);

  // Create random directory
  gchar *tmpDir = g_mkdtemp(buf);

  cr_assert(tmpDir != NULL, "Unable to create temporary directory %s: %s", template, strerror(errno));

  GString *result = g_string_new(tmpDir);

  return result;
}

// Create a fully qualified path to a temporary file
GString *createTemporaryFilePath(GString *dirname, gchar *basename)
{
  GString *filePath = g_string_new(dirname->str);

  g_string_append(filePath, basename);

  return filePath;
}

// Delete temporary file generated by a test
void removeTemporaryFile(gchar *fileName, gboolean force)
{
  // Remove file
  int ret = unlink(fileName);
  if(!force && ret != 0)
    {
      cr_log_info("removeTemporaryFile %s: %s", strerror(errno), fileName);
    }
}

// Remove temporary directory generated by a test
void removeTemporaryDirectory(gchar *dirName, gboolean force)
{
  // Remove directory
  int ret = rmdir(dirName);
  if(!force && ret != 0)
    {
      cr_log_info("removeTemporaryDirectory %s: %s", strerror(errno), dirName);
    }
}

// Initialize a test
TestData *initialize(gchar *name)
{
  cr_log_info("[%s] Initialization", name);

  TestData *testData = g_new0(TestData, 1);

  testData->testName = g_string_new(name);
  testData->testDir = createTemporaryDirectory(testDirTmpl);
  testData->keyFile = createTemporaryFilePath(testData->testDir, hostKeyFile);
  testData->macFile = createTemporaryFilePath(testData->testDir, macFile);

  generateHostKey(testData->hostKey, testData->keyFile->str);

  return testData;
}

// Close a test and free resources
void closure(TestData *testData)
{
  cr_log_info("[%s] Closure", testData->testName->str);

  removeTemporaryFile(testData->keyFile->str, TRUE);
  removeTemporaryFile(testData->macFile->str, TRUE);
  removeTemporaryDirectory(testData->testDir->str, TRUE);

  g_string_free(testData->testName, TRUE);
  g_string_free(testData->testDir, TRUE);
  g_string_free(testData->keyFile, TRUE);
  g_string_free(testData->macFile, TRUE);

  g_free(testData);
}

void corruptKey(TestData *testData)
{
  GError *error = NULL;
  GIOChannel *keyfile = g_io_channel_new_file(testData->keyFile->str, "w+", &error);

  cr_assert(keyfile != NULL, "Cannot open key file: %s", testData->keyFile->str);

  GIOStatus status = g_io_channel_set_encoding(keyfile, NULL, &error);

  cr_assert(status == G_IO_STATUS_NORMAL, " Unable to set encoding for key file %s", testData->keyFile->str);

  guint64 outlen = 0;

  int buflen = KEY_LENGTH + CMAC_LENGTH + sizeof(guint64);

  gchar data[buflen];

  // Overwrite the first 8 byte of the key with random values
  for(int i = 0; i < buflen; i++)
    {
      data[i] = randomNumber(1, 128);
    }

  // Overwrite the first 8 byte of the key with random values
  for(int i = 0; i < 8; i++)
    {
      testData->hostKey[i] = randomNumber(1, 128);
    }

  // Copy the corrupted key to the buffer
  memcpy(data, testData->hostKey, KEY_LENGTH);

  // Write garbage to key file
  status = g_io_channel_write_chars(keyfile, data, buflen, &outlen, &error);

  cr_assert(status == G_IO_STATUS_NORMAL, "Unable to write updated key to file %s", testData->keyFile->str);

  status = g_io_channel_shutdown(keyfile, TRUE, &error);
  g_io_channel_unref(keyfile);

  cr_assert(status == G_IO_STATUS_NORMAL, " Unable to close key file %s", testData->keyFile->str);
}


/*************************************************************************/
/* Unit test setup and teardown                                          */
/*************************************************************************/

void setup(void)
{
  srand(time(NULL));
  app_startup();
  init_parse_options_and_load_syslogformat(&test_parse_options);

  // This flag is required in order to pass the unaltered message to the slog template
  // It is required to be set after the initialization of the template tests above,
  // as this sets the parse options to the defaults
  test_parse_options.flags |= LP_STORE_RAW_MESSAGE;

  cfg_load_module(configuration, "secure-logging");
}

void teardown(void)
{
  deinit_template_tests();
  app_shutdown();
}

/*************************************************************************/
/* Test suite                                                            */
/*************************************************************************/
TestSuite(secure_logging, .init = setup, .fini = teardown);

void test_slog_template_format(void)
{
  TestData *testData = initialize("test_slog_template_format");

  GString *templ = g_string_new("");

  // $(slog -k keyfile)
  g_string_printf(templ, "$(slog -k %s)", testData->keyFile->str);
  assert_template_failure(templ->str, "[SLOG] ERROR: Template parsing failed. Invalid number of arguments");

  // $(slog -k keyfile -m)
  g_string_printf(templ, "$(slog -k %s -m)", testData->keyFile->str);
  assert_template_failure(templ->str, "Missing argument for -m");

  // $(slog -k -m macfile)
  g_string_printf(templ, "$(slog -k -m %s)", testData->macFile->str);
  assert_template_failure(templ->str, "Invalid path or non existing regular file: -m");

  // $(slog -k keyfile -m macfile)
  g_string_printf(templ, "$(slog -k %s -m %s)", testData->keyFile->str, testData->macFile->str);
  assert_template_failure(templ->str, "[SLOG] ERROR: Template parsing failed. Invalid number of arguments");

  g_string_free(templ, TRUE);

  closure(testData);
}

void test_slog_verification(void)
{
  TestData *testData = initialize("test_slog_verification");

  LogMessage *msg = create_random_sample_message();
  LogTemplate *slog_templ = createTemplate(testData);

  GString *output = applyTemplate(slog_templ, msg);
  size_t num = 1;

  verifyMessages(testData->hostKey, testData->macFile->str, &output, &msg, num);

  log_template_unref(slog_templ);

  closure(testData);
}

void test_slog_verification_bulk(void)
{
  TestData *testData = initialize("test_slog_verification_bulk");

  LogTemplate *slog_templ = createTemplate(testData);

  // Create a collection of log messages
  size_t num = randomNumber(MIN_TEST_MESSAGES, MAX_TEST_MESSAGES);
  LogMessage **logs = g_new0(LogMessage *, num);

  createLogMessages(num, logs);

  // Template output
  GString **output = g_new0(GString *, num);

  // Apply slog template to each message
  for(size_t i = 0; i < num; i++)
    {
      output[i] = applyTemplate(slog_templ, logs[i]);
    }

  // Verify the previously created log
  verifyMessages(testData->hostKey, testData->macFile->str, output, logs, num);

  // Release message resources
  for(size_t i = 0; i < num; i++)
    {
      log_msg_unref(logs[i]);
      g_string_free(output[i], TRUE);
    }

  log_template_unref(slog_templ);
  g_free(output);
  g_free(logs);

  closure(testData);
}

void test_slog_corrupted_key(void)
{
  TestData *testData = initialize("test_slog_corrupted_key");

  // Part 1: Log several messages -> They must be encrypted
  // Part 2: Corrupt the key
  // Log several messages -> They should be logged in plain text

  LogTemplate *slog_templ = createTemplate(testData);

  // Create a collection of log messages
  size_t num = randomNumber(MIN_TEST_MESSAGES, MAX_TEST_MESSAGES);

  LogMessage **logs = g_new0(LogMessage *, num);
  createLogMessages(num, logs);

  GString **output = g_new0(GString *, num);

  // Part 1: Apply slog template to each message
  for(size_t i = 0; i < num; i++)
    {
      output[i] = applyTemplate(slog_templ, logs[i]);
    }

  // Verify messages
  verifyMessages(testData->hostKey, testData->macFile->str, output, logs, num);

  // Release message resources
  for(size_t i = 0; i < num; i++)
    {
      log_msg_unref(logs[i]);
      g_string_free(output[i], TRUE);
    }

  log_template_unref(slog_templ);
  g_free(logs);
  g_free(output);

  // Part 2: Corrupt the key
  corruptKey(testData);

  // Re-initialize the template
  slog_templ = createTemplate(testData);

  // Create a collection of log messages
  num = randomNumber(MIN_TEST_MESSAGES, MAX_TEST_MESSAGES);
  logs = g_new0(LogMessage *, num);
  createLogMessages(num, logs);

  output = g_new0(GString *, num);

  // Apply slog template to each message
  for(size_t i = 0; i < num; i++)
    {
      output[i] = applyTemplate(slog_templ, logs[i]);

      // Create new message from the text content of the original message
      LogMessage *myOut = log_msg_new(output[i]->str, output[i]->len, &test_parse_options);

      // Initialize the new message with value from the original
      log_msg_set_saddr(myOut, logs[i]->saddr);
      myOut->timestamps[LM_TS_STAMP].ut_sec = logs[i]->timestamps[LM_TS_STAMP].ut_sec;
      myOut->timestamps[LM_TS_STAMP].ut_usec = logs[i]->timestamps[LM_TS_STAMP].ut_usec;
      myOut->timestamps[LM_TS_STAMP].ut_gmtoff = logs[i]->timestamps[LM_TS_STAMP].ut_gmtoff;
      myOut->pri = logs[i]->pri;
      gssize dlen;
      log_msg_set_value(myOut, LM_V_HOST, log_msg_get_value(logs[i], LM_V_HOST, &dlen), -1);
      log_msg_set_value(myOut, LM_V_PROGRAM, log_msg_get_value(logs[i], LM_V_PROGRAM, &dlen), -1);
      log_msg_set_value(myOut, LM_V_MESSAGE, log_msg_get_value(logs[i], LM_V_MESSAGE, &dlen), -1);
      log_msg_set_value(myOut, LM_V_PID, log_msg_get_value(logs[i], LM_V_PID, &dlen), -1);
      log_msg_set_value(myOut, LM_V_MSGID, log_msg_get_value(logs[i], LM_V_MSGID, &dlen), -1);
      assert_log_messages_equal(myOut, logs[i]);

      // Release message resources
      log_msg_unref(myOut);
      log_msg_unref(logs[i]);
      g_string_free(output[i], TRUE);
    }

  log_template_unref(slog_templ);
  g_free(output);
  g_free(logs);

  closure(testData);
}

void test_slog_malicious_modifications(void)
{
  TestData *testData = initialize("test_slog_malicious_modifications");

  LogTemplate *slog_templ = createTemplate(testData);

  // Create a collection of log messages
  size_t num = randomNumber(MIN_TEST_MESSAGES, MAX_TEST_MESSAGES);

  LogMessage **logs = g_new0(LogMessage *, num);

  createLogMessages(num, logs);

  // Template output
  GString **output = g_new0(GString *, num);

  // Apply slog template to each message
  for(size_t i = 0; i < num; i++)
    {
      output[i] = applyTemplate(slog_templ, logs[i]);
    }

  int mods = randomNumber(1, num-1);
  int entriesToModify[mods];

  // We might modify the same entry twice
  for (int i = 0; i < mods; i++)
    {
      entriesToModify[i] = randomNumber(0, num-1);

      // Overwrite with invalid string (invalid with high probability!)
      g_string_overwrite(output[entriesToModify[i]], randomNumber(COUNTER_LENGTH + COLON,
                                                                  (output[entriesToModify[i]]->len)-1), "999999999999999999999999999999999999999999999999999999999999999");
    }

  // Verify the previously created log
  int brokenEntries[mods];
  for (int i=0; i<mods; i++)
    {
      brokenEntries[i] = -1;
    }
  GString **ob = verifyMaliciousMessages(testData->hostKey, testData->macFile->str, output, num, brokenEntries);

  for (int i=0; i<num; i++)
    {
      if(1 == findInArray(i, entriesToModify, mods))
        {
          cr_assert(1 == findInArray(i, brokenEntries, mods), "Modified entry %d not detected.", i);
        }
      else
        {
          cr_assert(0 == findInArray(i, brokenEntries, mods), "Unmodified entry %d detected as modified.", i);
        }
    }

  // Release message resources
  for(size_t i = 0; i < num; i++)
    {
      log_msg_unref(logs[i]);
      g_string_free(output[i], TRUE);
    }

  log_template_unref(slog_templ);
  g_free(output);
  g_free(logs);
  g_free(ob);

  closure(testData);
}

void test_slog_performance(void)
{
  TestData *testData = initialize("test_slog_performance");

  LogTemplate *slog_templ = createTemplate(testData);

  GString *res = g_string_sized_new(1024);
  gint i;

  LogMessage *msg = create_random_sample_message();

  start_stopwatch();
  for (i = 0; i < PERFORMANCE_COUNTER; i++)
    {
      log_template_format(slog_templ, msg, &DEFAULT_TEMPLATE_EVAL_OPTIONS, res);
    }
  stop_stopwatch_and_display_result(PERFORMANCE_COUNTER, "%-90.*s", (int)strlen(slog_templ->template) - 1,
                                    slog_templ->template);

  // Free resources
  log_template_unref(slog_templ);
  g_string_free(res, TRUE);
  log_msg_unref(msg);

  closure(testData);
}

Test(secure_logging, test_slog_template_format)
{
  test_slog_template_format();
}

Test(secure_logging, test_slog_performance)
{
  test_slog_performance();
}

Test(secure_logging, test_slog_verification_bulk)
{
  test_slog_verification_bulk();
}

Test(secure_logging, test_slog_verification)
{
  test_slog_verification();
}

Test(secure_logging, test_slog_corrupted_key)
{
  test_slog_corrupted_key();
}

Test(secure_logging, test_slog_malicious_modifications)
{
  test_slog_malicious_modifications();
}
