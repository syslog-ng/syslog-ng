/*
 * Copyright (c) 2019-2020 Airbus Commercial Aircraft <secure-logging@airbus.com> 
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
#include <search.h>
#include <endian.h>
#include <errno.h>

#include <glib.h>

#include "messages.h"
#include "slog.h"

//Return 1 on success, 0 on error
int standardMode(int argc, char **argv) {
  if (argc<5) {
    printf("Usage:\n%s <host key file> <input file> <MAC file> <output file> [bufferSize]\n",argv[0]);
    return 0;
  }

  int bufferSize = 1000;
  if (argc>5) {
    bufferSize = atoi(argv[5]);
  }
  
  // Initialize internal messaging
  msg_init(TRUE);

  char key[KEY_LENGTH];
  guint64 counter;
  guint64 outlen=0;
  
  msg_info("[SLOG] INFO: Reading key file", evt_tag_str("name",argv[1]));
  int ret = readKey(key, &counter, argv[1]);
  if (ret!=1) {
    msg_error("[SLOG] ERROR: Key could not be loaded.");
    return 0;
  }
  
  if (counter!=0UL) {
    msg_error("HESU key read has counter different from 0.", evt_tag_long("Counter",counter));
    return 0;
  }
  
  msg_info("[SLOG] INFO: Reading MAC file", evt_tag_str("name", argv[3]));
  FILE *bigMAC = fopen(argv[3],"r");
  if(bigMAC == NULL) {
    msg_error("[SLOG] ERROR: Problem with MAC file");
    return 0;
  }
  
  unsigned char MAC[CMAC_LENGTH];
  if (readBigMAC(argv[3], MAC)==0) {
    msg_warning("[SLOG] WARNING: Cannot properly read MAC file.");
  }
  
  FILE *input = fopen(argv[2],"r");

  if(input == NULL) {
    msg_error("[SLOG] ERROR: Problem with input file");
    return 0;
  }

  // Scan through file ones
  guint64 entries = 0;
  while(!feof(input)) {
    char c = fgetc(input);
    if(c == '\n') {
      entries++;
    }
  }
  fclose(input);
  
  msg_info("[SLOG] INFO: Number of lines in file", evt_tag_long("number",entries));
  msg_info("[SLOG] INFO: Restoring and verifying log entries", evt_tag_int("buffer size", bufferSize));
  ret = fileVerify(key, argv[2], argv[4], MAC, entries, bufferSize);
  
  if (ret == 0) {
    msg_error("[SLOG] ERROR: There is a problem with log verification. Please check log manually");
  }
  return ret;
}


// Return 1 on success, 0 on error
int iterativeMode(int argc, char **argv) {
  if (argc<7) {
    printf("Usage:\n%s -i <PREVIOUS MAC file file> <PREVIOUS key file>  <input file> <CURRENT MAC file> <output file> [bufferSize]\n",argv[0]);
    return 0;
  }
  
  int bufferSize = 1000;
  if (argc>7) {
    bufferSize = atoi(argv[7]);
  }

  // Initialize internal messaging
  msg_init(TRUE);

  char previousKey[KEY_LENGTH];
  guint64 previousKeyCounter = 0;
  
  msg_info("[SLOG] INFO: Reading previous key file", evt_tag_str("name",argv[3]));
  int ret = readKey(previousKey, &previousKeyCounter, argv[3]);
  if (ret!=1) {
    msg_error("[SLOG] ERROR: Key could not be loaded.");
    return 0;
  }


  msg_info("[SLOG] INFO: Reading previous MAC file", evt_tag_str("name", argv[2]));
  FILE *previousBigMAC = fopen(argv[2],"r");
  if(previousBigMAC == NULL) {
    msg_error("[SLOG] ERROR: Problem with MAC file");
    return 0;
  }
  
  unsigned char previousMAC[CMAC_LENGTH];
  if (readBigMAC(argv[2], previousMAC)==0) {
    msg_warning("[SLOG] WARNING: Cannot properly read MAC file.");
  }
  
  msg_info("[SLOG] INFO: Reading current MAC file", evt_tag_str("name", argv[5]));
  FILE *currentBigMAC = fopen(argv[5],"r");
  if(currentBigMAC == NULL) {
    msg_error("[SLOG] ERROR: Problem with MAC file");
    return 0;
  }
  
  unsigned char currentMAC[CMAC_LENGTH];
  if (readBigMAC(argv[5], currentMAC)==0) {
    msg_warning("[SLOG] WARNING: Cannot properly read MAC file.");
  }
  
  FILE *input = fopen(argv[4],"r");
  if(input == NULL) {
    msg_error("[SLOG] ERROR: Problem with input file");
    return 0;
  }

  // Scan through file ones
  guint64 entries = 0;
  while(!feof(input)) {
    char c = fgetc(input);
    if(c == '\n') {
      entries++;
    }
  }
  fclose(input);

  
  msg_info("[SLOG] INFO: Number of lines in file", evt_tag_long("number",entries));
  msg_info("[SLOG] INFO: Restoring and verifying log entries", evt_tag_int("buffer size",bufferSize));
  ret = iterativeFileVerify(previousMAC, previousKey, argv[4], currentMAC, argv[6], entries, bufferSize, previousKeyCounter);
  
  if (ret == 0) {
    msg_error("[SLOG] ERROR: There is a problem with log verification. Please check log manually");
  }
  return ret;  
  
}



// Return 0 for success and 1 on error

int main(int argc, char *argv[]) {
  if (argc<2) {
    printf("Usage:\n%s <host key file> <input file> <input MAC file> <output file> [bufferSize]\n%s -i <PREVIOUS MAC file file> <PREVIOUS key file> <input file> <CURRENT MAC file> <output file> [bufferSize]\n",argv[0],argv[0]);
    return 1;
  }
  if (strcmp(argv[1],"-i")==0) {
    return 1-iterativeMode(argc, argv);
  } else {
    return 1-standardMode(argc, argv);
  }
  
}
