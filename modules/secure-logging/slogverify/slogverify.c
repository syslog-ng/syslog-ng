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

// Arguments and options
static gboolean iterative = FALSE;
static char *hostKey = NULL;
static char *prevHostKey = NULL;
static char *curMacFile = NULL;
static char *prevMacFile = NULL;
static char *inputLog = NULL;
static char *outputLog = NULL;
static int bufSize = DEF_BUF_SIZE;

// Return TRUE on success, FALSE on error
gboolean normalMode(char *hostkey, char *MACfile, char *inputlog, char *outputlog, int bufsize)
{
  guchar key[KEY_LENGTH];
  guint64 counter;

  msg_info(SLOG_INFO_PREFIX, evt_tag_str("Reason", "Reading key file"), evt_tag_str("name", hostkey));
  gboolean success = readKey(key, &counter, hostkey);
  if (!success)
    {
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Unable to read host key"), evt_tag_str("file", hostkey));
      return FALSE; //-- ERROR
    }

  if (counter!=0UL)
    {
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason",
                                               "Initial key k0 is required for verification and decryption but the supplied key read has a counter > 0."),
                evt_tag_long("Counter", counter));
      return FALSE; //-- ERROR
    }

  msg_info(SLOG_INFO_PREFIX, evt_tag_str("Reason", "Reading MAC file"), evt_tag_str("name", MACfile));
  FILE *bigMAC = fopen(MACfile, "r");
  if (bigMAC == NULL)
    {
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Unable to read MAC"), evt_tag_str("file", MACfile));
      return FALSE; //-- ERROR
    }

  guchar MAC[CMAC_LENGTH]; //-- aggregated MAC
  if (!readAggregatedMAC(MACfile, MAC))
    {
      msg_warning(SLOG_WARNING_PREFIX, evt_tag_str("Reason", "Unable to read MAC"), evt_tag_str("file", MACfile));
    }

  //-- initial MAC0 ---
  char pathMac0[PATH_MAX]; //-- full path of MAC0 file mac0.dat
  guchar MAC0[CMAC_LENGTH]; //-- initial MAC
  memset(MAC0, 0, CMAC_LENGTH);
  if (TRUE == get_path_mac0(MACfile, pathMac0, PATH_MAX))
    {
      if (!readAggregatedMAC(pathMac0, MAC0))
        {
          msg_warning(SLOG_WARNING_PREFIX, evt_tag_str("Reason", "Unable to read MAC0"), evt_tag_str("file", pathMac0));
        }
      else
        {
          msg_info(SLOG_INFO_PREFIX, evt_tag_str("Reason", "Read MAC0"), evt_tag_str("file", pathMac0));
        }
    }
  else
    {
      msg_warning(SLOG_WARNING_PREFIX, evt_tag_str("Reason", "Invalid pathMac0")); //-- fileVerify will fail later
    }


  FILE *input = fopen(inputlog, "r");

  if (input == NULL)
    {
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Unable to open input log"), evt_tag_str("file", inputlog));
      return FALSE; //-- ERROR
    }

  // Scan through file ones
  guint64 entries = 0;
  while (!feof(input))
    {
      char c = fgetc(input);
      if (c == '\n')
        {
          entries++;
        }
    }
  fclose(input);

  msg_info(SLOG_INFO_PREFIX, evt_tag_str("Reason", "Number of lines in file"), evt_tag_long("number", entries));
  msg_info(SLOG_INFO_PREFIX, evt_tag_str("Reason", "Restoring and verifying log entries"), evt_tag_int("buffer size",
           bufsize));
  gboolean result = fileVerify(key, inputlog, outputlog, MAC, entries, bufsize, MAC0);
  if (!result)
    {
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason",
                                               "There is a problem with log verification. Please check log manually"));
      return FALSE; //-- ERROR

    }
  return TRUE; //-- SUCCESS
}


// Return TRUE on success, FALSE on error
gboolean iterativeMode(char *prevKey, char *prevMAC, char *curMAC, char *inputlog, char *outputlog, int bufsize)
{
  guchar previousKey[KEY_LENGTH];
  guint64 previousKeyCounter = 0;

  msg_info(SLOG_INFO_PREFIX, evt_tag_str("Reason", "Reading previous key file"), evt_tag_str("name", prevKey));
  gboolean success = readKey(previousKey, &previousKeyCounter, prevKey);
  if (!success)
    {
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Previous key could not be loaded."), evt_tag_str("file", prevKey));
      return FALSE; //-- ERROR
    }

  msg_info(SLOG_INFO_PREFIX, evt_tag_str("Reason", "Reading previous MAC file"), evt_tag_str("name", prevMAC));
  FILE *previousBigMAC = fopen(prevMAC, "r");
  if (previousBigMAC == NULL)
    {
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Unable to read previous MAC"), evt_tag_str("file", prevMAC));
      return FALSE; //-- ERROR
    }

  guchar previousMAC[CMAC_LENGTH];
  if (!readAggregatedMAC(prevMAC, previousMAC))
    {
      msg_warning(SLOG_WARNING_PREFIX, evt_tag_str("Reason", "Unable to read previous MAC"), evt_tag_str("file", prevMAC));
    }

  msg_info(SLOG_INFO_PREFIX, evt_tag_str("Reason", "Reading current MAC file"), evt_tag_str("name", curMAC));
  FILE *currentBigMAC = fopen(curMAC, "r");
  if (currentBigMAC == NULL)
    {
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Unable to read current MAC"), evt_tag_str("file", curMAC));
      return FALSE; //-- ERROR
    }

  guchar currentMAC[CMAC_LENGTH];
  if (!readAggregatedMAC(curMAC, currentMAC))
    {
      msg_warning(SLOG_WARNING_PREFIX, evt_tag_str("Reason", "Unable to read current MAC"), evt_tag_str("file", curMAC));
    }

  FILE *input = fopen(inputlog, "r");
  if (input == NULL)
    {
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Unable to open input log"), evt_tag_str("file", inputlog));
      return FALSE; //-- ERROR
    }

  // Scan through file ones
  guint64 entries = 0;
  while (!feof(input))
    {
      char c = fgetc(input);
      if (c == '\n')
        {
          entries++;
        }
    }
  fclose(input);

  msg_info(SLOG_INFO_PREFIX, evt_tag_str("Reason", "Number of lines in file"), evt_tag_long("number", entries));
  msg_info(SLOG_INFO_PREFIX, evt_tag_str("Reason", "Restoring and verifying log entries"), evt_tag_int("buffer size",
           bufSize));
  gboolean result = iterativeFileVerify(previousMAC, previousKey, inputlog, currentMAC, outputlog,
                                        entries,
                                        bufsize, previousKeyCounter);

  if (!result)
    {
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason",
                                               "There is a problem with log verification. Please check log manually"));
      return FALSE; //-- ERROR (note: fix bug, was 1)

    }

  return TRUE; //-- SUCCESS
}


//
// main logic: 0 usually indicates success, and non-zero values indicate an error
//
int main(int argc, char *argv[])
{
  int index = 0;
  SLogOptions options[] =
  {
    { "iterative", 'i', "Iterative verification", NULL, NULL },
    { "key-file", 'k', "Initial host key file", "FILE", NULL },
    { "mac-file", 'm', "Current MAC file", "FILE", NULL },
    { "prev-key-file", 'p', "Previous host key file in iterative mode", "FILE", NULL },
    { "prev-mac-file", 'r', "Previous MAC file in iterative mode", "FILE", NULL },
    { NULL }
  };

  GOptionEntry entries[] =
  {
    { options[0].longname, options[0].shortname, 0, G_OPTION_ARG_NONE, &iterative, options[0].description, NULL },
    { options[1].longname, options[1].shortname, 0, G_OPTION_ARG_CALLBACK, &validFileNameArg, options[1].description, options[1].type },
    { options[2].longname, options[2].shortname, 0, G_OPTION_ARG_CALLBACK, &validFileNameArg, options[2].description, options[2].type },
    { options[3].longname, options[3].shortname, 0, G_OPTION_ARG_CALLBACK, &validFileNameArg, options[3].description, options[3].type },
    { options[4].longname, options[4].shortname, 0, G_OPTION_ARG_CALLBACK, &validFileNameArg, options[4].description, options[4].type },
    { NULL }
  };

  GError *error = NULL;
  GOptionContext *context = g_option_context_new("INPUTLOG OUTPUTLOG [COUNTER] - Log archive verification");
  GOptionGroup *group = g_option_group_new("Basic options", "Basic log archive verification options", "basic", &options,
                                           NULL);

  g_option_group_add_entries(group, entries);
  g_option_context_set_main_group(context, group);

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      GString *errorMsg = g_string_new(error->message);
      (void) slog_usage(context, group, errorMsg);
      return 1; //-- ERROR
    }

  if (argc < 2 || argc > 4)
    {
      (void) slog_usage(context, group, NULL);
      return 1; //-- ERROR
    }

  // Initialize internal messaging
  msg_init(TRUE);

  // Assign option arguments
  index = 1;
  hostKey = options[index].arg;
  index++;
  curMacFile = options[index].arg;
  index++;
  prevHostKey = options[index].arg;
  index++;
  prevMacFile = options[index].arg;
  index++;

  // Input and output file arguments
  index = 1;
  inputLog = argv[index];
  index++;
  if (!g_file_test(inputLog, G_FILE_TEST_IS_REGULAR))
    {
      GString *errorMsg = g_string_new(FILE_ERROR);
      g_string_append(errorMsg, inputLog);
      (void) slog_usage(context, group, errorMsg);
      return 1; //-- ERROR
    }

  outputLog = argv[index];
  index++;
  if (outputLog == NULL)
    {
      (void) slog_usage(context, group, NULL);
      return 1; //-- ERROR
    }

  // Buffer size arguments if applicable
  if (argc == 4)
    {
      bufSize = atoi(argv[index]);

      if (bufSize <= MIN_BUF_SIZE || bufSize > MAX_BUF_SIZE)
        {
          msg_error(SLOG_ERROR_PREFIX,
                    evt_tag_str("Reason", "Invalid buffer size."),
                    evt_tag_int("Size", bufSize),
                    evt_tag_int("Minimum buffer size", MIN_BUF_SIZE),
                    evt_tag_int("Maximum buffer size", MAX_BUF_SIZE));
          g_option_context_free(context);
          return 1; //-- ERROR
        }
    }

  int ret = 0; //-- main logic, 0 errors
  if (iterative)
    {
      if (prevHostKey == NULL || prevMacFile == NULL || curMacFile == NULL)
        {
          g_print("%s", g_option_context_get_help(context, TRUE, NULL));
          g_option_context_free(context);
          return 1; //-- ERROR
        }

      gboolean result = iterativeMode(prevHostKey, prevMacFile, curMacFile, inputLog, outputLog, bufSize);
      if (!result)
        {
          ret = 1; //-- ERROR
        }
    }
  else
    {
      if (hostKey == NULL || curMacFile == NULL)
        {
          g_print("%s", g_option_context_get_help(context, TRUE, NULL));
          g_option_context_free(context);
          return 1; //-- ERROR
        }
      gboolean result = normalMode(hostKey, curMacFile, inputLog, outputLog, bufSize);
      if (!result)
        {
          ret = 1; // ERROR
        }
    }

  // Release messaging resources
  msg_deinit();

  return ret;
}
