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
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>

#ifdef __APPLE__
#include <machine/endian.h>
#else
#include <endian.h>
#endif

#include <glib.h>

#include <openssl/rand.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

#include "messages.h"

#include "slog.h"

int main(int argc, char **argv)
{
  if (argc<2)
    {
      printf("SYNTAX\n======\n\nTo generate new master key:\n%s -m <master key filename>\n\nTo derive a host key:\n%s -d <master key filename> <host MAC address> <host serial number> <host key filename>\n\nTo show the counter value of a key:\n%s -s <key file name>\n\n",
             argv[0], argv[0], argv[0]);
      return -1;
    }

  int ret = 0;

  // Initialize internal messaging
  msg_init(TRUE);

  argc--;
  if (strcmp(argv[1], "-m")==0)
    {
      guchar masterkey[KEY_LENGTH];
      char *keyfile = argv[2];

      ret = generateMasterKey(masterkey);
      if(!ret)
        {
          msg_error("Unable to create master key");
          return -1;
        }

      ret = writeKey((gchar *)masterkey, 0, keyfile);
      if(!ret)
        {
          msg_error("Unable to write master key to file", evt_tag_str("file", keyfile));
          return -1;
        }
      return ret;
    }
  else if (strcmp(argv[1], "-s")==0)
    {
      // Display key counter
      char key[KEY_LENGTH];
      char *keyfile = argv[2];
      size_t counter;
      ret = readKey(key, &counter, keyfile);
      if(!ret)
        {
          msg_error("Unable to read key file", evt_tag_str("file", keyfile));
          return -1;
        }
      printf("This key's counter value is: %zu\n", counter);
    }
  else if (strcmp(argv[1], "-d")==0)
    {
      // Arguments
      gchar *masterKeyFileName = argv[2];
      gchar *macAddr = argv[3];
      gchar *serial = argv[4];
      gchar *hostKeyFileName = argv[5];

      gchar masterKey[KEY_LENGTH] = { 0 };

      guint64 counter;

      ret = readKey((char *)masterKey, &counter, masterKeyFileName);
      if (ret == 0)
        {
          msg_error("Unable to read master key");
          return -1;
        }

      guchar hostKey[KEY_LENGTH];

      ret = deriveHostKey((guchar *)masterKey, macAddr, serial, hostKey);
      if(!ret)
        {
          msg_error("Unable to derive a host key");
          return -1;
        }

      ret = writeKey((char *)hostKey, 0, hostKeyFileName);
      if(ret == 0)
        {
          msg_error("Unable to write host key to file", evt_tag_str("file", hostKeyFileName));
          return -1;
        }
    }
  else
    {
      msg_error("Unknown option.");
      ret = -1;
    }

  // Release messaging resources
  msg_deinit();

  return ret;
}
