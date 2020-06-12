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
static char *hostkey = NULL;
static char *newhostKey = NULL;
static char *inputMAC = NULL;
static char *outputMAC = NULL;
static char *inputlog = NULL;
static char *outputlog = NULL;
static guint64 bufSize = DEF_BUF_SIZE;

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
    { options[1].longname, options[1].shortname, 0, G_OPTION_ARG_CALLBACK, &validFileNameArg, options[1].description, options[1].type },
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
      GString *errorMsg = g_string_new(error->message);

      return slog_usage(context, group, errorMsg);
    }

  if(argc < 5 || argc > 6)
    {
      return slog_usage(context, group, NULL);
    }

  // Initialize internal messaging
  msg_init(TRUE);

  char key[KEY_LENGTH];
  char mac[CMAC_LENGTH];

  // Assign option arguments
  index = 0;
  hostkey = options[index].arg;
  index++;
  inputMAC = options[index].arg;

  // Input and output file arguments
  index = 1;
  newhostKey = argv[index];
  index++;
  if(newhostKey == NULL)
    {
      return slog_usage(context, group, NULL);
    }

  outputMAC = argv[index];
  index++;
  if(outputMAC == NULL)
    {
      return slog_usage(context, group, NULL);
    }

  inputlog = argv[index];
  index++;
  if(!g_file_test(inputlog, G_FILE_TEST_IS_REGULAR))
    {
      GString *errorMsg = g_string_new(FILE_ERROR);
      g_string_append(errorMsg, inputlog);
      return slog_usage(context, group, errorMsg);
    }

  outputlog = argv[index];
  index++;
  if(outputlog == NULL)
    {
      return slog_usage(context, group, NULL);
    }

  // Read key and counter
  guint64 counter;
  int ret = readKey(key, &counter, hostkey);
  if (ret!=1)
    {
      msg_error("[SLOG] ERROR: Unable to read host key", evt_tag_str("file", hostkey));
      return -1;
    }

  // Buffer size arguments if applicable
  if (argc == 6)
    {
      sscanf(argv[index], "%"G_GUINT64_FORMAT, &bufSize);

      if(bufSize <= MIN_BUF_SIZE || bufSize > MAX_BUF_SIZE)
        {
          msg_error("[SLOG] ERROR: Invalid buffer size.", evt_tag_int("Size", bufSize),
                    evt_tag_int("Minimum buffer size", MIN_BUF_SIZE), evt_tag_int("Maximum buffer size", MAX_BUF_SIZE));
          g_option_context_free(context);
          return 0;
        }
    }

  // Open input file
  FILE *inputFile = fopen(inputlog, "r");
  if (inputFile == NULL)
    {
      msg_error("[SLOG] ERROR: Unable to open input log file", evt_tag_str("file", inputlog));
      return -1;
    }

  // Open output file
  FILE *outputFile = fopen(outputlog, "w");
  if (outputFile == NULL)
    {
      msg_error("[SLOG] ERROR: Unable to open output log file", evt_tag_str("file", outputlog));
      return -1;
    }

  // Read MAC (if possible)
  if (readBigMAC(inputMAC, mac)==0)
    {
      msg_warning("[SLOG] ERROR: Unable to open input MAC file", evt_tag_str("file", inputMAC));
    }

  size_t readLen = 0;
  char *line = NULL;

  msg_info("Importing data...");

  // Parse data
  while(getline(&line, &readLen, inputFile)!=-1)
    {
      char outputmacdata[CMAC_LENGTH];

      GString *result = g_string_new(NULL);
      GString *inputGString = g_string_new(line);

      // Remove trailing '\n' from string
      g_string_truncate(inputGString, (inputGString->len)-1);

      sLogEntry(counter, inputGString, (unsigned char *)key, (unsigned char *)mac, result, (unsigned char *)outputmacdata);

      fprintf(outputFile, "%s\n", result->str);

      // Update keys, MAC, etc
      memcpy(mac, outputmacdata, CMAC_LENGTH);
      evolveKey((unsigned char *)key);
      counter++;

      readLen = 0;
      free(line);
      g_string_free(result, TRUE);
      g_string_free(inputGString, TRUE);
      line = NULL;
    }


  // Write whole log MAC
  if (writeBigMAC(outputMAC, mac)==0)
    {
      msg_error("Problem with output MAC file");
      return -1;
    }

  // Write key
  ret = writeKey(key, counter, newhostKey);
  if (ret!=1)
    {
      msg_error("[SLOG] ERROR: Unable to write new host key", evt_tag_str("file", outputlog));
      return -1;
    }

  fclose(outputFile);

  msg_info("All data successfully imported.");

  return 1;
}
