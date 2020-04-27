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


int main(int argc, char *argv[])
{
  GError *error = NULL;
  GOptionContext *context = g_option_context_new("- Import log files using secure logging\n\n  " \
     "The following argments must be supplied in exactly this order\n\n  " \
     "HOSTKEYFILE OUTPUTHOSTKEYFILE INPUTLOGFILE " \
     "OUTPUTLOGFILE INPUTMACFILE OUTPUTMACFILE [COUNTER]\n\n  where\n\n  "
     "HOSTKEYFILE\t\tThe current host key file\n  "
     "OUTPUTHOSTKEYFILE\tThe name of the file receiving the newly created host key\n  "
     "INPUTLOGFILE\t\tThe log file to import\n  "
     "OUTPUTLOGFILE\t\tThe name of the encrypted log file receiving the import\n  "
     "INPUTMACFILE\t\tThe current MAC file\n  "
     "OUTPUTMACFILE\t\tThe resulting MAC file updated after the import\n  "
     "[COUNTER]\t\tAn optional counter for controlling the import buffer\n\n  "
     "Arguments in brackets [] are optional, all other arguments are required");

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_print ("Invalid option: %s\n", error->message);
      exit (1);
    }

  if (argc < 7)
    {
      printf("%s", g_option_context_get_help(context, TRUE, NULL));
      g_option_context_free(context);
      return -1;
    }

  g_option_context_free(context);

  // Initialize internal messaging
  msg_init(TRUE);

  char key[KEY_LENGTH];
  char mac[CMAC_LENGTH];

  int index = 1;
  char* hostkey = argv[index++];
  char* newhostKey = argv[index++];
  char* inputlog = argv[index++];
  char* outputlog = argv[index++];
  char* inputMAC = argv[index++];
  char* outputMAC = argv[index++];
  
  // Read key and counter
  size_t counter;
  int ret = readKey(key, &counter, hostkey);
  if (ret!=1)
    {
      msg_error("[SLOG] ERROR: Unable to read host key", evt_tag_str("file", hostkey));
      return -1;
    }

  if (argc==8)
    {
      errno = 0;
      gboolean ok = TRUE;
      char* counterString = argv[index];
      int len = sscanf(counterString, "%zu", &counter);
      if(len <= 0)
	{
	  msg_error("[SLOG] ERROR: Invalid counter value", evt_tag_str("value", counterString));
	  ok = FALSE;
	}
      if(errno != 0)
	{
	  msg_error("[SLOG] ERROR: Unable to process input value", evt_tag_str("error", strerror(errno)));
	  ok = FALSE;
	}
      if(!ok)
	{
	  return -1;
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
