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

// Return 1 on success, 0 on error
int normalMode(char *hostkey, char *MACfile, char *inputlog, char *outputlog, int bufsize)
{
  char key[KEY_LENGTH];
  guint64 counter;

  msg_info("[SLOG] INFO: Reading key file", evt_tag_str("name", hostkey));
  int ret = readKey(key, &counter, hostkey);
  if (ret!=1)
    {
      msg_error("[SLOG] ERROR: Unable to read host key", evt_tag_str("file", hostkey));
      return 0;
    }

  if (counter!=0UL)
    {
      msg_error("[SLOG] ERROR: Initial key k0 is required for verification and decryption but the supplied key read has a counter > 0.",
                evt_tag_long("Counter", counter));
      return 0;
    }

  msg_info("[SLOG] INFO: Reading MAC file", evt_tag_str("name", MACfile));
  FILE *bigMAC = fopen(MACfile, "r");
  if(bigMAC == NULL)
    {
      msg_error("[SLOG] ERROR: Unable to read MAC", evt_tag_str("file", MACfile));
      return 0;
    }

  unsigned char MAC[CMAC_LENGTH];
  if (readBigMAC(MACfile, (char *)MAC)==0)
    {
      msg_warning("[SLOG] WARNING: Unable to read MAC", evt_tag_str("file", MACfile));
    }

  FILE *input = fopen(inputlog, "r");

  if(input == NULL)
    {
      msg_error("[SLOG] ERROR: Unable to open input log", evt_tag_str("file", inputlog));
      return 0;
    }

  // Scan through file ones
  guint64 entries = 0;
  while(!feof(input))
    {
      char c = fgetc(input);
      if(c == '\n')
        {
          entries++;
        }
    }
  fclose(input);

  msg_info("[SLOG] INFO: Number of lines in file", evt_tag_long("number", entries));
  msg_info("[SLOG] INFO: Restoring and verifying log entries", evt_tag_int("buffer size", bufsize));
  ret = fileVerify((unsigned char *)key, inputlog, outputlog, MAC, entries, bufsize);

  if (ret == 0)
    {
      msg_error("[SLOG] ERROR: There is a problem with log verification. Please check log manually");
    }
  return ret;
}


// Return 1 on success, 0 on error
int iterativeMode(char *prevKey, char *prevMAC, char *curMAC, char *inputlog, char *outputlog, int bufsize)
{
  char previousKey[KEY_LENGTH];
  guint64 previousKeyCounter = 0;

  msg_info("[SLOG] INFO: Reading previous key file", evt_tag_str("name", prevKey));
  int ret = readKey(previousKey, &previousKeyCounter, prevKey);
  if (ret!=1)
    {
      msg_error("[SLOG] ERROR: Previous key could not be loaded.", evt_tag_str("file", prevKey));
      return 0;
    }

  msg_info("[SLOG] INFO: Reading previous MAC file", evt_tag_str("name", prevMAC));
  FILE *previousBigMAC = fopen(prevMAC, "r");
  if(previousBigMAC == NULL)
    {
      msg_error("[SLOG] ERROR: Unable to read previous MAC", evt_tag_str("file", prevMAC));
      return 0;
    }

  unsigned char previousMAC[CMAC_LENGTH];
  if (readBigMAC(prevMAC, (char *)previousMAC)==0)
    {
      msg_warning("[SLOG] WARNING: Unable to read previous MAC", evt_tag_str("file", prevMAC));
    }

  msg_info("[SLOG] INFO: Reading current MAC file", evt_tag_str("name", curMAC));
  FILE *currentBigMAC = fopen(curMAC, "r");
  if(currentBigMAC == NULL)
    {
      msg_error("[SLOG] ERROR: Unable to read current MAC", evt_tag_str("file", curMAC));
      return 0;
    }

  unsigned char currentMAC[CMAC_LENGTH];
  if (readBigMAC(curMAC, (char *)currentMAC)==0)
    {
      msg_warning("[SLOG] WARNING: Unable to read current MAC", evt_tag_str("file", curMAC));
    }

  FILE *input = fopen(inputlog, "r");
  if(input == NULL)
    {
      msg_error("[SLOG] ERROR: Unable to open input log", evt_tag_str("file", inputlog));
      return 0;
    }

  // Scan through file ones
  guint64 entries = 0;
  while(!feof(input))
    {
      char c = fgetc(input);
      if(c == '\n')
        {
          entries++;
        }
    }
  fclose(input);

  msg_info("[SLOG] INFO: Number of lines in file", evt_tag_long("number", entries));
  msg_info("[SLOG] INFO: Restoring and verifying log entries", evt_tag_int("buffer size", bufSize));
  ret = iterativeFileVerify(previousMAC, (unsigned char *)previousKey, inputlog, currentMAC, outputlog, entries,
                            bufsize, previousKeyCounter);

  if (ret == 0)
    {
      msg_error("[SLOG] ERROR: There is a problem with log verification. Please check log manually");
    }

  return ret;
}

// Return 0 for success and 1 on error
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

      return slog_usage(context, group, errorMsg);
    }

  if(argc < 2 || argc > 4)
    {
      return slog_usage(context, group, NULL);
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
  if(!g_file_test(inputLog, G_FILE_TEST_IS_REGULAR))
    {
      GString *errorMsg = g_string_new(FILE_ERROR);
      g_string_append(errorMsg, inputLog);
      return slog_usage(context, group, errorMsg);
    }

  outputLog = argv[index];
  index++;
  if(outputLog == NULL)
    {
      return slog_usage(context, group, NULL);
    }

  // Buffer size arguments if applicable
  if (argc == 4)
    {
      bufSize = atoi(argv[index]);

      if(bufSize <= MIN_BUF_SIZE || bufSize > MAX_BUF_SIZE)
        {
          msg_error("[SLOG] ERROR: Invalid buffer size.", evt_tag_int("Size", bufSize),
                    evt_tag_int("Minimum buffer size", MIN_BUF_SIZE), evt_tag_int("Maximum buffer size", MAX_BUF_SIZE));
          g_option_context_free(context);
          return 1;
        }
    }

  int ret = 0;

  if (iterative)
    {
      if (prevHostKey == NULL || prevMacFile == NULL || curMacFile == NULL)
        {
          printf("%s", g_option_context_get_help(context, TRUE, NULL));
          g_option_context_free(context);
          return 1;
        }

      ret = 1 - iterativeMode(prevHostKey, prevMacFile, curMacFile, inputLog, outputLog, bufSize);
    }
  else
    {
      if (hostKey == NULL || curMacFile == NULL)
        {
          printf("%s", g_option_context_get_help(context, TRUE, NULL));
          g_option_context_free(context);
          return 1;
        }

      ret = 1 - normalMode(hostKey, curMacFile, inputLog, outputLog, bufSize);
    }

  // Release messaging resources
  msg_deinit();

  return ret;
}
