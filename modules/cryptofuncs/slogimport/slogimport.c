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

#include <glib.h>
#include "messages.h"


#include "slog.h"


int main(int argc, char *argv[])
{
  if (argc<7)
    {
      printf("Usage:\n%s <host key file> <output key file> <input log file> <output log file> <input MAC file> <output MAC file> [counter]\n",
             argv[0]);
      return -1;
    }

  // Initialize internal messaging
  msg_init(TRUE);


  char key[KEY_LENGTH];
  char mac[CMAC_LENGTH];

  // Read key and counter
  size_t counter;
  int ret = readKey(key, &counter, argv[1]);
  if (ret!=1)
    {
      perror("Problem with key file.");
      return -1;
    }

  if (argc==8)
    {
      sscanf(argv[7], "%zu", &counter);
    }

  // Open input file
  FILE *inputFile = fopen(argv[3], "r");
  if (inputFile == NULL)
    {
      msg_error("Problem with input file.");
      return -1;
    }

  // Open output file
  FILE *outputFile = fopen(argv[4], "w");
  if (outputFile == NULL)
    {
      msg_error("Problem with output file.");
      return -1;
    }

  // Read MAC (if possible)
  if (readBigMAC(argv[5], mac)==0)
    {
      msg_warning("Problem with input MAC file");
    }

  size_t readLen = 0;
  char *line = NULL;

  msg_info("Importing data");

  // Parse data
  while(getline(&line, &readLen, inputFile)!=-1)
    {
      char outputmacdata[CMAC_LENGTH];

      GString *result = g_string_new(NULL);
      GString *inputGString = g_string_new(line);
      // Remove trailing '\n' from string
      g_string_truncate(inputGString, (inputGString->len)-1);

      sLogEntry(counter, inputGString, key, mac, result, outputmacdata);

      fprintf(outputFile, "%s\n", result->str);

      // Update keys, MAC, etc
      memcpy(mac, outputmacdata, CMAC_LENGTH);
      evolveKey(key);
      counter++;


      readLen = 0;
      free(line);
      line = NULL;
    }


  // Write whole log MAC
  if (writeBigMAC(argv[6], mac)==0)
    {
      msg_error("Problem with output MAC file");
      return -1;
    }

  //Write key
  ret = writeKey(key, counter, argv[2]);
  if (ret!=1)
    {
      msg_error("Problem writing output key file.");
      return -1;
    }

  fclose(outputFile);

  msg_info("All data successfully imported.");

  return 1;
}
