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

#define MIN_BUF_SIZE 10
#define MAX_BUF_SIZE 100000

// Options
static gboolean iterative = FALSE;

static GOptionEntry options[] =
{
  { "iterative", 'i', 0, G_OPTION_ARG_NONE, &iterative, "Iterative verification", NULL },
  { NULL }
};

// Return 1 on success, 0 on error
int normalMode(int argc, char **argv)
{
  int bufferSize = 1000;
  if (argc > 5)
    {
      int size = atoi(argv[5]);

      if(size <= MIN_BUF_SIZE || size > MAX_BUF_SIZE)
        {
          msg_error("[SLOG] ERROR: Invalid buffer size.", evt_tag_int("Size", size), evt_tag_int("Minimum buffer size",
                    MIN_BUF_SIZE), evt_tag_int("Maximum buffer size", MAX_BUF_SIZE));
          return 0;
        }
      else
        {
          bufferSize = size;
        }
    }

  char key[KEY_LENGTH];
  guint64 counter;
  char* hostkey = argv[1];
  
  msg_info("[SLOG] INFO: Reading key file", evt_tag_str("name", argv[1]));
  int ret = readKey(key, &counter, hostkey);
  if (ret!=1)
    {
      msg_error("[SLOG] ERROR: Unable to read host key", evt_tag_str("file", hostkey));
      return 0;
    }

  if (counter!=0UL)
    {
      msg_error("Host key read has counter different from 0.", evt_tag_long("Counter", counter));
      return 0;
    }

  char* MACfile = argv[3];
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

  char* inputlog = argv[2];
  char* outputlog = argv[4];
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
  msg_info("[SLOG] INFO: Restoring and verifying log entries", evt_tag_int("buffer size", bufferSize));
  ret = fileVerify((unsigned char *)key, inputlog, outputlog, MAC, entries, bufferSize);

  if (ret == 0)
    {
      msg_error("[SLOG] ERROR: There is a problem with log verification. Please check log manually");
    }
  return ret;
}


// Return 1 on success, 0 on error
int iterativeMode(int argc, char **argv)
{
  int bufferSize = 1000;
  if (argc > 7)
    {
      int size = atoi(argv[7]);

      if(size <= MIN_BUF_SIZE || size > MAX_BUF_SIZE)
        {
          msg_error("[SLOG] ERROR: Invalid buffer size.", evt_tag_int("Size", size), evt_tag_int("Minimum buffer size",
                    MIN_BUF_SIZE), evt_tag_int("Maximum buffer size", MAX_BUF_SIZE));
          return 0;
        }
      else
        {
          bufferSize = size;
        }
    }

  char previousKey[KEY_LENGTH];
  guint64 previousKeyCounter = 0;

  char* prevKey = argv[3];
  msg_info("[SLOG] INFO: Reading previous key file", evt_tag_str("name", prevKey));
  int ret = readKey(previousKey, &previousKeyCounter, prevKey);
  if (ret!=1)
    {
      msg_error("[SLOG] ERROR: Previous key could not be loaded.", evt_tag_str("file", prevKey));
      return 0;
    }

  char* prevMAC = argv[2];
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

  char* curMAC = argv[5];
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

  char* inputlog = argv[4];
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

  char* outputlog = argv[6];
  
  msg_info("[SLOG] INFO: Number of lines in file", evt_tag_long("number", entries));
  msg_info("[SLOG] INFO: Restoring and verifying log entries", evt_tag_int("buffer size", bufferSize));
  ret = iterativeFileVerify(previousMAC, (unsigned char *)previousKey, argv[4], currentMAC, outputlog, entries, bufferSize,
                            previousKeyCounter);

  if (ret == 0)
    {
      msg_error("[SLOG] ERROR: There is a problem with log verification. Please check log manually");
    }
  return ret;

}


// Return 0 for success and 1 on error
int main(int argc, char *argv[])
{
  GError *error = NULL;
  GOptionContext *context = g_option_context_new("- Log archive verification\n\n  " \
     "NORMAL MODE: The following argments must be supplied in exactly this order\n\n  " \
     "HOSTKEY INPUTLOG MACFILE OUTPUTLOG [BUFFERSIZE]\n\n  where\n\n  " \
     "HOSTKEY\t\tThe current host key file\n  " \
     "INPUTLOG\t\tThe log file to verify\n  " \
     "MACFILE\t\tThe current MAC file\n  "
     "OUTPUTLOG\t\tThe name of the file receiving the cleartext log entries after verification\n  " \
     "[BUFFERSIZE]\t\tAn optional buffer size useful for verifying very large log files\n\n  " \
     "ITERATIVE MODE: In addition to the -i option, the following arguments are required\n\n  " \
     "PREVIOUSMAC PREVIOUSKEY INPUTLOG CURRENTMAC OUTPUTLOG [BUFFERSIZE]\n\n  where\n\n  " \
     "PREVIOUSMAC\t\tThe current MAC file\n  " \
     "PREVIOUSKEY\t\tThe current host key file\n  " \
     "INPUTLOG\t\tThe log file to verify\n  " \
     "CURRENTMAC\t\tThe current MAC file\n  " \
     "OUTPUTLOG\t\tThe name of the file receiving the cleartext log entries after verification\n  " \
     "[BUFFERSIZE]\t\tAn optional buffer size useful for verifying very large log files\n\n  ");

  g_option_context_add_main_entries (context, options, NULL);
  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_print ("Invalid option: %s\n", error->message);
      exit (1);
    }

  if (argc < 2)
    {
      printf("%s", g_option_context_get_help(context, TRUE, NULL));
      g_option_context_free(context);
      return 1;
    }

  // Initialize internal messaging
  msg_init(TRUE);

  int ret = 0;

  if (iterative)
    {
      if (argc < 7)
	{
	  printf("%s", g_option_context_get_help(context, TRUE, NULL));
	  g_option_context_free(context);
	  return 1;
	}

      ret = 1 - iterativeMode(argc, argv);
    }
  else
    {
      if (argc < 5)
	{
	  printf("%s", g_option_context_get_help(context, TRUE, NULL));
	  g_option_context_free(context);
	  return 1;
	}

      ret = 1 - normalMode(argc, argv);
    }

  // Release messaging resources
  msg_deinit();

  return ret;
}
