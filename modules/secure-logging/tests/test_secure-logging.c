/*
 * Copyright (c) 2019 Airbus Commercial Aircraft
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

// Key and MAC for secure logging
static guchar masterkey[KEY_LENGTH];
static guchar hostkey[KEY_LENGTH];
static gchar mac[CMAC_LENGTH];

// Test data for secure logging
static gchar *masterKeyFileName = "/tmp/master.key";
static gchar *hostKeyFileName = "/tmp/host.key";
static gchar *macFileName = "/tmp/mac.dat";
static gchar *macAddr = "a08cefa7b520";
static gchar *serial = "CAC7119N43";
static gchar *prefix = "slog/";
static gchar *context_id = "test-context-id";

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

  GSockAddr *saddr;

  saddr = g_sockaddr_inet_new("10.11.12.13", 1010);
  msg = log_msg_new(msg_str->str, msg_str->len, saddr, &test_parse_options);
  g_sockaddr_unref(saddr);
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
LogTemplate *createTemplate(void)
{
  GString *slog_templ_str = g_string_new("slog");

  // Initialize the template
  g_string_printf(slog_templ_str, "$(slog -k %s -m %s $RAWMSG)", hostKeyFileName, macFileName);

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

  // Execute secure logging template
  log_template_append_format_with_context(
    templ,       // Secure logging template
    &msg,        // Message(s) to pass to the template
    1,           // Number of message to pass to the template
    NULL,        // Template options (no options in this case)
    LTZ_LOCAL,   // Timezone
    999,         // Sequence number for recursive template invocations
    context_id,  // Context identifiertc.logtc.log
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
GString **verifyMaliciousMessages(GString **templateOutput, size_t totalNumberOfMessages, int *brokenEntries)
{
  unsigned char keyZero[KEY_LENGTH];
  memcpy(keyZero, hostkey, KEY_LENGTH);

  GHashTable *tab = NULL;

  size_t next = 0;
  size_t start = 0;
  size_t numberOfLogEntries = 0UL;

  GString **outputBuffer = (GString **) malloc(sizeof(GString *) * totalNumberOfMessages);

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
void verifyMessages(GString **templateOutput, LogMessage **original, size_t totalNumberOfMessages)
{
  unsigned char keyZero[KEY_LENGTH];
  memcpy(keyZero, hostkey, KEY_LENGTH);

  GHashTable *tab = NULL;

  size_t next = 0;
  size_t start = 0;
  size_t numberOfLogEntries = 0UL;

  GString **outputBuffer = (GString **) malloc(sizeof(GString *) * totalNumberOfMessages);

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
      LogMessage *result = log_msg_new(plaintextMessage, strlen(plaintextMessage), original[i]->saddr, &test_parse_options);
      assert_log_messages_equal(original[i], result);
      g_string_free(outputBuffer[i], TRUE);
      log_msg_unref(result);
    }

  free(outputBuffer);
}

// Generate keys to be used for the tests
void generateKeys(void)
{
  // Create keys for the test
  int ret = generateMasterKey(masterkey);
  cr_assert(ret == 1, "Unable to generate master key");

  ret = writeKey((gchar *)masterkey, 0, masterKeyFileName);
  cr_assert(ret == 1, "Unable to write master key to file %s", masterKeyFileName);

  ret = deriveHostKey(masterkey, macAddr, serial, hostkey);
  cr_assert(ret == 1, "Unable to derive host key from master key for addr %s and serial number %s", macAddr, serial);

  ret = writeKey((gchar *)hostkey, 0, hostKeyFileName);
  cr_assert(ret == 1, "Unable to write host key to file %s", hostKeyFileName);
}

// Delete any keys from a previous test
void removeKeys(gboolean force)
{
  // Remove temporary test keys
  int ret = unlink(hostKeyFileName);
  if(!force && ret != 0)
    {
      cr_log_info("removeKeys %s: %s", strerror(errno), hostKeyFileName);
    }

  ret = unlink(masterKeyFileName);
  if(!force && ret != 0)
    {
      cr_log_info("removeKeys %s: %s", strerror(errno), masterKeyFileName);
    }

  // Remove any existing MAC file
  ret = unlink(macFileName);
  if(!force && ret != 0)
    {
      cr_log_info("removeKeys %s: %s", strerror(errno), macFileName);
    }
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

void
teardown(void)
{
  deinit_template_tests();
  app_shutdown();
}

/*************************************************************************/
/* Test suite                                                            */
/*************************************************************************/
TestSuite(secure-logging, .init = setup, .fini = teardown);

void test_slog_template_format(void)
{
  cr_log_info("[test_slog_template_format] Start");

  assert_template_failure("$(slog -k keyfile)", "$(slog) parsing failed, invalid number of arguments");
  assert_template_failure("$(slog -k keyfile -m)", "Missing argument for -m");
  assert_template_failure("$(slog -k keyfile -m macfile)", "$(slog) parsing failed, invalid number of arguments");
  cr_log_info("[test_slog_template_format] Successfully completed");
}

void test_slog_verification(void)
{
  cr_log_info("[test_slog_verification] Start");

  // Get rid of any existing key and MAC files before starting the test
  removeKeys(TRUE);

  // Generate a fresh set of keys for the tests
  generateKeys();

  LogMessage *msg = create_random_sample_message();
  LogTemplate *slog_templ = createTemplate();

  GString *output = applyTemplate(slog_templ, msg);
  size_t num = 1;

  verifyMessages(&output, &msg, num);

  log_template_unref(slog_templ);
  g_string_free(output, TRUE);

  cr_log_info("[test_slog_verification] Completed successfully");
}

void test_slog_verification_bulk(void)
{
  cr_log_info("[test_slog_verification_bulk] Start");

  // Get rid of any existing key and MAC files before starting the test
  removeKeys(TRUE);

  // Generate a fresh set of keys for the tests
  generateKeys();

  LogTemplate *slog_templ = createTemplate();

  // Create a collection of log messages
  size_t num = randomNumber(MIN_TEST_MESSAGES, MAX_TEST_MESSAGES);
  LogMessage **logs = malloc(num * sizeof(LogMessage *));

  createLogMessages(num, logs);

  // Template output
  GString **output = malloc(num * sizeof(GString *));

  // Apply slog template to each message
  for(size_t i = 0; i < num; i++)
    {
      output[i] = applyTemplate(slog_templ, logs[i]);
    }

  // Verify the previously created log
  verifyMessages(output, logs, num);

  // Release message resources
  for(size_t i = 0; i < num; i++)
    {
      log_msg_unref(logs[i]);
      g_string_free(output[i], TRUE);
    }

  log_template_unref(slog_templ);
  free(output);
  free(logs);
  cr_log_info("[test_slog_verification_bulk] Successfully completed");
}

void test_slog_corrupted_key(void)
{
  cr_log_info("[test_slog_corrupted_key] Start");

  // Log several messages -> They must be encrypted
  // Delete the key
  // Log several messages -> They should be logged in plain text

  // Get rid of any existing key and MAC files before starting the test
  removeKeys(TRUE);

  // Generate a fresh set of keys for the tests
  generateKeys();

  LogTemplate *slog_templ = createTemplate();

  // Create a collection of log messages
  size_t num = randomNumber(MIN_TEST_MESSAGES, MAX_TEST_MESSAGES);

  LogMessage **logs = malloc(num * sizeof(LogMessage *));
  createLogMessages(num, logs);

  GString **output = malloc(num * sizeof(GString *));

  // Apply slog template to each message
  for(size_t i = 0; i < num; i++)
    {
      output[i] = applyTemplate(slog_templ, logs[i]);
    }

  // Verify messages
  verifyMessages(output, logs, num);

  // Release message resources
  for(size_t i = 0; i < num; i++)
    {
      log_msg_unref(logs[i]);
      g_string_free(output[i], TRUE);
    }

  log_template_unref(slog_templ);
  free(logs);
  free(output);


  // Part 2: delete the key file
  int ret = unlink(hostKeyFileName);
  if(ret != 0)
    {
      cr_log_info("%s: %s", strerror(errno), hostKeyFileName);
    }

  // Create a collection of log messages
  num = randomNumber(MIN_TEST_MESSAGES, MAX_TEST_MESSAGES);
  logs = malloc(num * sizeof(LogMessage *));
  createLogMessages(num, logs);

  slog_templ = createTemplate();
  output = malloc(num * sizeof(GString *));

  // Apply slog template to each message
  for(size_t i = 0; i < num; i++)
    {
      output[i] = applyTemplate(slog_templ, logs[i]);
      LogMessage *myOut = log_msg_new(output[i]->str, output[i]->len, logs[i]->saddr, &test_parse_options);
      assert_log_messages_equal(myOut, logs[i]);

      // Release message resources
      log_msg_unref(myOut);
      log_msg_unref(logs[i]);
      g_string_free(output[i], TRUE);
    }

  log_template_unref(slog_templ);
  free(output);
  free(logs);

  cr_log_info("[test_slog_corrupted_key] Completed successfully");
}

void test_slog_malicious_modifications(void)
{
  cr_log_info("[test_slog_malicious_modifications] Start");

  // Get rid of any existing key and MAC files before starting the test
  removeKeys(TRUE);

  // Generate a fresh set of keys for the tests
  generateKeys();

  LogTemplate *slog_templ = createTemplate();

  // Create a collection of log messages
  size_t num = randomNumber(MIN_TEST_MESSAGES, MAX_TEST_MESSAGES);

  LogMessage **logs = malloc(num * sizeof(LogMessage *));

  createLogMessages(num, logs);

  // Template output
  GString **output = malloc(num * sizeof(GString *));

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

      //Overwrite with invalid string (invalid with high probability!)
      g_string_overwrite(output[entriesToModify[i]], randomNumber(COUNTER_LENGTH + COLON,
                                                                  (output[entriesToModify[i]]->len)-1), "999999999999999999999999999999999999999999999999999999999999999");
    }

  // Verify the previously created log
  int brokenEntries[mods];
  for (int i=0; i<mods; i++)
    {
      brokenEntries[i] = -1;
    }
  GString **ob = verifyMaliciousMessages(output, num, brokenEntries);

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
  free(output);
  free(logs);
  free(ob);

  cr_log_info("[test_slog_malicious_modifications] Completed successfully");
}

void test_slog_performance(void)
{
  cr_log_info("[test_slog_performance] Start");

  // Get rid of any existing key and MAC files before starting the test
  removeKeys(TRUE);

  // Generate a fresh set of keys for the tests
  generateKeys();

  LogTemplate *slog_templ = createTemplate();

  GString *res = g_string_sized_new(1024);
  gint i;

  LogMessage *msg = create_random_sample_message();

  start_stopwatch();
  for (i = 0; i < PERFORMANCE_COUNTER; i++)
    {
      log_template_format(slog_templ, msg, NULL, LTZ_LOCAL, 0, NULL, res);
    }
  stop_stopwatch_and_display_result(PERFORMANCE_COUNTER, "%-90.*s", (int)strlen(slog_templ->template) - 1,
                                    slog_templ->template);

  // Free resources
  log_template_unref(slog_templ);
  g_string_free(res, TRUE);
  log_msg_unref(msg);

  cr_log_info("[test_slog_performance] Completed successfully");
}

Test(secure-logging, test_slog_functionality)
{
  test_slog_template_format();
  test_slog_performance();
  test_slog_verification();
  test_slog_verification_bulk();
  test_slog_corrupted_key();
  test_slog_malicious_modifications();
}
