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
static gboolean counter = FALSE;

static GOptionEntry entries[] =
{
  { "master-key", 'm', 0, G_OPTION_ARG_NONE, &master, "Generate a master key", NULL },
  { "derive-host-key", 'd', 0, G_OPTION_ARG_NONE, &host, "Derive a host key from an existing master key", NULL },
  { "counter", 'c', 0, G_OPTION_ARG_NONE, &counter, "Display current host key counter", NULL },
  { NULL }
};

int main(int argc, char **argv)
{
  GError *error = NULL;
  GOptionContext *context = g_option_context_new("- secure logging key management\n\n  " \
                                                 "Master key generation:\tslogkey -m MASTERKEY\n  " \
                                                 "Host key derivation:\t\tslogkey -d MASTERKEY MACADDRESS SERIALNUMBER HOSTKEY\n  " \
                                                 "Host key counter display:\tslogkey -c HOSTKEY");

  g_option_context_add_main_entries (context, entries, NULL);
  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_print ("Invalid option: %s\n", error->message);
      exit (1);
    }

  if (argc < 2)
    {
      g_print("%s", g_option_context_get_help(context, TRUE, NULL));
      g_option_context_free(context);
      return -1;
    }

  gboolean ok = TRUE;

  // Options are mutually exclusive
  if (master && host)
    {
      ok = FALSE;
    }
  else if (master && counter)
    {
      ok = FALSE;
    }
  else if (host && counter)
    {
      ok = FALSE;
    }
  else if (master && host && counter)
    {
      ok = FALSE;
    }

  if (!ok)
    {
      g_print("%s", g_option_context_get_help(context, TRUE, NULL));
      g_option_context_free(context);
      return -1;
    }

  // Master key generation and sequence counter display need one option and a single argument
  if ((master || counter) && argc > 2)
    {
      g_print("%s", g_option_context_get_help(context, TRUE, NULL));
      g_option_context_free(context);
      return -1;
    }

  // Host key derivation needs one option and four arguments
  if(host && argc != 5)
    {
      g_print("%s", g_option_context_get_help(context, TRUE, NULL));
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
  else if (counter)
    {
      // Display key counter
      char key[KEY_LENGTH];
      char *keyfile = argv[index];
      guint64 counterValue;
      success = readKey(key, &counterValue, keyfile);
      if(!success)
        {
          msg_error("[SLOG] ERROR: Unable to read key file", evt_tag_str("file", keyfile));
          return ret;
        }
      printf("counter=%zu\n", counterValue);
    }
  else if (host)
    {
      // Arguments
      gchar *masterKeyFileName = argv[index];
      index++;
      gchar *macAddr = argv[index];
      index++;
      gchar *serial = argv[index];
      index++;
      gchar *hostKeyFileName = argv[index];

      gchar masterKey[KEY_LENGTH] = { 0 };

      guint64 counterValue;

      success = readKey((char *)masterKey, &counterValue, masterKeyFileName);
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
