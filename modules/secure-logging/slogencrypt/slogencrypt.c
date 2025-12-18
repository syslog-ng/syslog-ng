/*
 * Copyright (c) 2019-2025 Airbus Commercial Aircraft
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <glib.h>

#include "messages.h"
#include "slog.h"
#include "compat/string.h"

// Arguments and options
static char *hostkey = NULL;
static char *newhostKey = NULL;
static char *inputMAC = NULL;
static char *outputMAC = NULL;
static char *inputlog = NULL;
static char *outputlog = NULL;
static guint64 bufSize = DEF_BUF_SIZE;


//
// main logic: 0 usually indicates success, and non-zero values indicate an error
//
int main(int argc, char *argv[])
{
  int index = 0;
  SLogOptions options[] =
  {
    { "key-file", 'k', "Current host key file", "FILE", NULL },
    { "mac-file", 'm', "Current MAC file", "FILE", NULL },
    { NULL }
  };

  GOptionEntry entries[] =
  {
    { options[0].longname, options[0].shortname, 0, G_OPTION_ARG_CALLBACK, &validFileNameArg, options[0].description, options[0].type },
    { options[1].longname, options[1].shortname, 0, G_OPTION_ARG_CALLBACK, &validFileNameArgCheckDirOnly, options[1].description, options[1].type },
    { NULL }
  };

  GError *error = NULL;
  GOptionContext *context =
    g_option_context_new("NEWKEY NEWMAC INPUTLOG OUTPUTLOG [COUNTER] - Encrypt log files using secure logging");
  GOptionGroup *group = g_option_group_new("Basic options", "Basic log encryption options", "basic", &options, NULL);

  g_option_group_add_entries(group, entries);
  g_option_context_set_main_group(context, group);


  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      // Fix: In case there is no mac file yet available, this tool shall not
      // fail. Therefore only given mac file folder is checked instead full
      // file name. This is done by replacement of validFileNameArg by validFileNameArgCheckDirOnly

      g_print("!g_opton_context_parse, argc: %d\n", argc);
      GString *errorMsg = g_string_new(error->message);
      (void) slog_usage(context, group, errorMsg);
      return 1; //-- ERROR
    }

  // Note: When Options -k, --key-file or -m, --mac-file are not provided, the
  // corresponding validFileNameArg is not called and therefore no check is
  // performed!
  // The argument checks therefore must be done by checking for NULL or by verifying
  // argc in case of ordered arguments.

  // Note: When all data is provided correctly, argc is 5 or 6 after parsing
  if(argc < 5 || argc > 6)
    {
      g_print("ERROR: Count of arguments is out of range!\n\n");
      (void) slog_usage(context, group, NULL);
      return 1; //-- ERROR
    }

  // Initialize internal messaging
  msg_init(TRUE);

  unsigned char key[KEY_LENGTH];
  unsigned char mac[CMAC_LENGTH];
  gsize nk = G_N_ELEMENTS(key);
  memset(key, 0, nk);
  gsize nm = G_N_ELEMENTS(mac);
  memset(mac, 0, nm);

  // Assign option arguments
  index = 0;
  hostkey = options[index].arg;
  if (NULL == hostkey)
    {
      g_print("ERROR: Option --key-file or -k with a valid path to a Host key is missing!\n\n");
      msg_error(SLOG_ERROR_PREFIX,
                evt_tag_str("Reason",
                            "Option --key-file or -k with a valid path to a Host key is missing!"));
      (void) slog_usage(context, group, NULL);
      return 1; //-- ERROR
    }

  index++;
  inputMAC = options[index].arg;
  if (NULL == inputMAC)
    {
      g_print("ERROR: Option --mac-file or -m does not provide a valid path to a MAC file!\n\n");
      msg_error(SLOG_ERROR_PREFIX,
                evt_tag_str("Reason",
                            "Option --mac-file or -m does not provide a valid path to a MAC file!"));
      (void) slog_usage(context, group, NULL);
      return 1; //-- ERROR
    }

  // Input and output file arguments
  index = 1;
  newhostKey = argv[index];
  if(newhostKey == NULL)
    {
      // Safe. Will not be reached due check of argc above
      g_print("ERROR: Path to new host key is missing!\n\n");
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Path to new host key is missing!"));
      (void) slog_usage(context, group, NULL);
      g_assert_not_reached();
      return 1; //-- ERROR
    }
  index++;

  outputMAC = argv[index];
  if(outputMAC == NULL)
    {
      // Safe. Will not be reached due check of argc above
      g_print("ERROR: Path to new MAC file is missing!\n\n");
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Path to new MAC file is missing!"));
      (void) slog_usage(context, group, NULL);
      g_assert_not_reached();
      return 1; //-- ERROR
    }
  index++;

  inputlog = argv[index];
  index++;
  if(!g_file_test(inputlog, G_FILE_TEST_IS_REGULAR))
    {
      GString *errorMsg = g_string_new(FILE_ERROR);
      g_string_append(errorMsg, inputlog);
      (void) slog_usage(context, group, errorMsg);
      return 1; //-- ERROR
    }

  outputlog = argv[index];
  index++;
  if(outputlog == NULL)
    {
      // Safe. Will not be reached due check of argc above
      g_print("ERROR: Path to output log is missing!\n\n");
      (void) slog_usage(context, group, NULL);
      g_assert_not_reached();
      return 1; //-- ERROR
    }

  // Read key and counter
  guint64 counter;
  gboolean ret = readKey(key, &counter, hostkey);
  if (!ret)
    {
      msg_error(SLOG_ERROR_PREFIX,
                evt_tag_str("Reason", "Unable to read host key!"),
                evt_tag_str("file", hostkey));
      return -1; //-- ERROR
    }

  // Buffer size arguments if applicable
  if (argc == 6)
    {
      int result = sscanf(argv[index], "%"G_GUINT64_FORMAT, &bufSize);

      if(result == EOF || bufSize <= MIN_BUF_SIZE || bufSize > MAX_BUF_SIZE)
        {
          msg_error(SLOG_ERROR_PREFIX,
                    evt_tag_str("Reason", "Invalid buffer size."),
                    evt_tag_int("Size", bufSize),
                    evt_tag_int("Minimum buffer size", MIN_BUF_SIZE),
                    evt_tag_int("Maximum buffer size", MAX_BUF_SIZE));
          g_option_context_free(context);
          return -1; //-- ERROR
        }
    }

  // Open input file
  FILE *inputFile = fopen(inputlog, "r");
  if (inputFile == NULL)
    {
      msg_error(SLOG_ERROR_PREFIX,
                evt_tag_str("Reason", "Unable to open input log file!"),
                evt_tag_str("file", inputlog));
      return -1; //-- ERROR
    }

  // Open output file
  FILE *outputFile = fopen(outputlog, "w");
  if (outputFile == NULL)
    {
      msg_error(SLOG_ERROR_PREFIX,
                evt_tag_str("Reason", "Unable to open output log file!"),
                evt_tag_str("file", outputlog));
      return -1; //-- ERROR
    }

  //-- initial MAC file ---
  if(!g_file_test(inputMAC, G_FILE_TEST_IS_REGULAR))
    {
      //-- This is normal the first time, slogencrypt is called.
      msg_info(SLOG_INFO_PREFIX,
               evt_tag_str("Reason", "MAC file, specified by parameter -m, was not found and is created now (initial MAC file)."),
               evt_tag_str("file", inputMAC));
      create_initial_mac0(key, mac);
      if (writeAggregatedMAC(inputMAC, mac) == FALSE)
        {
          //-- ERROR: file was not written. Anyhow do not exit here.
          msg_warning(SLOG_WARNING_PREFIX,
                      evt_tag_str("Reason", "writeAggregatedMAC was not successful!"),
                      evt_tag_str("file", inputMAC));
        }
      if(!g_file_test(inputMAC, G_FILE_TEST_IS_REGULAR))
        {
          //-- ERROR: file not found. Anyhow do not exit here.
          msg_warning(SLOG_WARNING_PREFIX,
                      evt_tag_str("Reason", "Initial MAC file was not written as expected!"),
                      evt_tag_str("file", inputMAC));
        }
    }

  // Read MAC (if possible)
  if (readAggregatedMAC(inputMAC, mac)==0)
    {
      msg_warning(SLOG_WARNING_PREFIX,
                  evt_tag_str("Reason", "Unable to open input MAC file!"),
                  evt_tag_str("file", inputMAC));
      // This is an error but we can proceed anyhow. But later, when
      // slogverify is used, a dummy mac file must be provided for the first verification instead
      // and this will cause an verification error.
    }

  size_t readLen = 0;
  char *line = NULL;
  gboolean outcome = TRUE;

  msg_info(SLOG_INFO_PREFIX, evt_tag_str("Reason", "Importing data..."));

  // Parse data
  while(getline(&line, &readLen, inputFile) != -1)
    {
      char outputmacdata[CMAC_LENGTH];
      GString *result = g_string_new(NULL);
      GString *inputGString = g_string_new(line);

#if defined(IS_LIMIT_LOGSTR) && ((IS_LIMIT_LOGSTR) == 1)

      //-- 2025-12-03
      //   In the classic secure logging (not the crash recovery variant)
      //   there shall be no limitation in regard to the length of a
      //   log string.
      //   No truncation might be a security risk and this might also cause
      //   a problem when buffers are created on the stack instead of the heap.

      //-- 2025-10-20, The code is designed to work with any length of log line.
      //   For security reason the length of the utf-8 string is limited.
      //   Later, Crash Recovery shall be integrated and there, the length
      //   is limited to 2048 bytes (MESSAGE_LEN, slog.h).
      truncate_utf8_gstring(inputGString, MESSAGE_LEN);

#endif /* IS_LIMIT_LOGSTR, see slog.h */

      // Remove trailing '\n' from string
      g_string_truncate(inputGString, (inputGString->len)-1);

      gsize outputmacdata_capacity = G_N_ELEMENTS(outputmacdata);

      outcome = sLogEntry(counter, inputGString, (unsigned char *)key, (unsigned char *)mac, result,
                          (unsigned char *)outputmacdata,
                          outputmacdata_capacity);
      if(!outcome)
        {
          msg_warning(SLOG_WARNING_PREFIX,
                      evt_tag_str("Reason", "Unable to encrypt log entry!"),
                      evt_tag_str("line", inputGString->str));
          return -1; //-- ERROR;
        }

      fprintf(outputFile, "%s\n", result->str);

      // Update keys, MAC, etc
      memcpy(mac, outputmacdata, CMAC_LENGTH);
      outcome = evolveKey((unsigned char *)key);
      if(!outcome)
        {
          msg_warning(SLOG_WARNING_PREFIX, evt_tag_str("Reason", "Unable to evolve key!"));
          return -1; //-- ERROR
        }

      counter++;

      readLen = 0;
      free(line);
      g_string_free(result, TRUE);
      g_string_free(inputGString, TRUE);
      line = NULL;
    }

  // Write whole log MAC
  if (!writeAggregatedMAC(outputMAC, mac))
    {
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Problem with output MAC file!"));
      return -1; //-- ERROR
    }

  // Write key
  ret = writeKey(key, counter, newhostKey);
  if (!ret)
    {
      msg_error(SLOG_ERROR_PREFIX,
                evt_tag_str("Reason", "Unable to write new host key"),
                evt_tag_str("file", outputlog));
      return -1; //-- ERROR
    }

  fclose(outputFile);

  msg_info("All data successfully imported.");

  return 0; //-- SUCCESS
}
