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

#include <glib.h>

#include <openssl/rand.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

#include "messages.h"
#include "slog.h"

// Options
static gboolean master = FALSE;
static gboolean host = FALSE;
static gboolean sequence = FALSE;

static GOptionEntry entries[] =
{
  { "master-key", 'm', 0, G_OPTION_ARG_NONE, &master, "Generate a master key", NULL },
  { "derive-host-key", 'd', 0, G_OPTION_ARG_NONE, &host, "Derive a host key from an existing master key", NULL },
  { "sequence", 's', 0, G_OPTION_ARG_NONE, &sequence, "Display current host key sequence counter", NULL },
  { NULL }
};

int main(int argc, char **argv)
{
  GError *error = NULL;
  GOptionContext *context = g_option_context_new("- secure logging key management\n\n  " \
                                                 "Master key generation:     slogkey -m MASTERKEY\n  " \
                                                 "Host key derivation:       slogkey -d MASTERKEY MACADDRESS SERIALNUMBER HOSTKEYFILE\n  " \
                                                 "Host key sequence display: slogkey -s HOSTKEY");

  g_option_context_add_main_entries (context, entries, NULL);
  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_print ("Invalid option: %s\n", error->message);
      exit (1);
    }

  if (argc < 2)
    {
      printf("%s", g_option_context_get_help(context, TRUE, NULL));
      g_option_context_free(context);
      return -1;
    }

  gboolean ok = TRUE;

  // Optionsע are mutually exclusive
  if (master && host)
    {
      ok = FALSE;
    }
  else if (master && sequence)
    {
      ok = FALSE;
    }
  else if (host && sequence)
    {
      ok = FALSE;
    }
  else if (master && host && sequence)
    {
      ok = FALSE;
    }

  if (argc >= 2 && !ok)
    {
      printf("%s", g_option_context_get_help(context, TRUE, NULL));
      g_option_context_free(context);
      return -1;
    }

  // Host key derivation needs one option and four arguments
  if(host && argc != 5)
    {
      printf("%s", g_option_context_get_help(context, TRUE, NULL));
      g_option_context_free(context);
      return -1;
    }

  // Option parsing successful -> context can be released
  g_option_context_free(context);

  int ret = 0;
  int success = 0;
  int index = 1;

  // Initialize internal messaging
  msg_init(TRUE);

  if (master)
    {
      guchar masterkey[KEY_LENGTH];
      char *keyfile = argv[index];

      success = generateMasterKey(masterkey);
      if(!success)
        {
          msg_error("Unable to create master key");
          ret = -1;
        }
      else
        {
          success = writeKey((gchar *)masterkey, 0, keyfile);
          if(!success)
            {
              msg_error("[SLOG] ERROR: Unable to write master key to file", evt_tag_str("file", keyfile));
              ret = -1;
            }
        }
      return ret;
    }
  else if (sequence)
    {
      // Display key sequence counter
      char key[KEY_LENGTH];
      char *keyfile = argv[index];
      size_t counter;
      success = readKey(key, &counter, keyfile);
      if(!success)
        {
          msg_error("[SLOG] ERROR: Unable to read key file", evt_tag_str("file", keyfile));
          return ret;
        }
      printf("sequence=%zu\n", counter);
    }
  else if (host)
    {
      // Arguments
      gchar *masterKeyFileName = argv[index++];
      gchar *macAddr = argv[index++];
      gchar *serial = argv[index++];
      gchar *hostKeyFileName = argv[index++];

      gchar masterKey[KEY_LENGTH] = { 0 };

      guint64 counter;

      success = readKey((char *)masterKey, &counter, masterKeyFileName);
      if (!success)
        {
          msg_error("[SLOG] ERROR: Unable to read master key", evt_tag_str("file", masterKeyFileName));
          ret = -1;
        }
      else
        {
          guchar hostKey[KEY_LENGTH];

          success = deriveHostKey((guchar *)masterKey, macAddr, serial, hostKey);
          if(!success)
            {
              msg_error("[SLOG] ERROR: Unable to derive a host key");
              ret = -1;
            }
          else
            {
              success = writeKey((char *)hostKey, 0, hostKeyFileName);
              if(success == 0)
                {
                  msg_error("[SLOG] ERROR: Unable to write host key to file", evt_tag_str("file", hostKeyFileName));
                  ret = -1;
                }
            }
        }
    }

  // Release messaging resources
  msg_deinit();

  return ret;
}
