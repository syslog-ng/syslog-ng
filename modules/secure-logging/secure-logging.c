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

#include "plugin.h"
#include "template/simple-function.h"
#include "cfg.h"
#include "uuid.h"
#include "str-format.h"
#include "plugin-types.h"
#include "compat/openssl_support.h"
#include <openssl/evp.h>

#include <glib.h>

#include <openssl/rand.h>
#include <sys/mman.h>

// Secure logging declarations
#include "slog.h"

/*
 * $(slog [opts] $RAWMSG)
 *
 * Maintains the state of the secure logging template
 *
 * Returns the current encryption key and MAC for the next
 * round and maintains the counter for the total number of
 * of log entries processed.
 *
 * Note: The flag store-raw-message has to be enabled in the
 *       source configuration in order to use the $RAWMSG macro.
 *
 * Options:
 *      -k        Full path to encryption key file
 *      -m        Full path to MAC file
 */
typedef struct _TFSlogState
{
  TFSimpleFuncState super;

  gchar *keypath;
  gchar *macpath;
  guint64 numberOfLogEntries;

  gboolean badKey;
  guchar key[KEY_LENGTH];
  guchar bigMAC[CMAC_LENGTH];

} TFSlogState;

/*
 * Initialize the secure logging template
 */
static gboolean
tf_slog_prepare(LogTemplateFunction *self, gpointer s, LogTemplate *parent, gint argc, gchar *argv[], GError **error)
{
  // Get key filename and store in internal state
  // generate initial BigMAC file

  TFSlogState *state = (TFSlogState *) s;

  gchar *keypathbuffer, *macpathbuffer;
  GOptionContext *ctx;

  if ((mlock(state->key, KEY_LENGTH)!=0)||(mlock(state->bigMAC, CMAC_LENGTH)!=0))
    {
      msg_warning("[SLOG] WARNING: Cannot mlock memory.");
    }

  state->badKey = FALSE;

  GOptionEntry slog_options[] =
  {
    { "key-file", 'k', 0, G_OPTION_ARG_FILENAME, &keypathbuffer, NULL, NULL },
    { "mac-file", 'm', 0, G_OPTION_ARG_FILENAME, &macpathbuffer, NULL, NULL },
    { NULL }
  };

  ctx = g_option_context_new("slog");
  g_option_context_add_main_entries(ctx, slog_options, NULL);

  if (!g_option_context_parse(ctx, &argc, &argv, error))
    {
      state->badKey = TRUE;
      g_option_context_free(ctx);
      return FALSE;
    }
  g_option_context_free(ctx);


  if (argc < 2)
    {
      state->badKey = TRUE;
      g_set_error(error, LOG_TEMPLATE_ERROR, LOG_TEMPLATE_ERROR_COMPILE,
                  "[SLOG] ERROR: $(slog) parsing failed, invalid number of arguments");
      return FALSE;
    }

  if (!tf_simple_func_prepare(self, state, parent, argc, argv, error))
    {
      state->badKey = TRUE;
      return FALSE;
    }

  state->numberOfLogEntries = 0;
  state->keypath = keypathbuffer;
  state->macpath = macpathbuffer;

  int res = readKey((char *)state->key, (guint64 *)&(state->numberOfLogEntries), state->keypath);

  if (res == 0)
    {
      state->badKey = TRUE;
      msg_warning("[SLOG] WARNING: $(slog) parsing failed, key file not found or invalid. Reverting to cleartext logging.");
      return TRUE;
    }

  msg_info("SLOG: Key successfully loaded");

  res = readBigMAC(state->macpath, (char *)state->bigMAC);

  if (res == 0)
    {
      msg_warning("[SLOG] WARNING: Aggregated MAC not found or invalid", evt_tag_str("File", state->macpath));
      return TRUE;
    }

  msg_info("[SLOG] $(slog) template with key and MAC file successfully initialized.");

  return TRUE;
}

/*
 * Create a new encrypted log entry
 */
static void
tf_slog_call(LogTemplateFunction *self, gpointer s, const LogTemplateInvokeArgs *args, GString *result)
{

  TFSlogState *state = (TFSlogState *) s;

  // If we do not have a good key, just forward input
  if (state->badKey == TRUE)
    {
      g_string_append (result, args->argv[0]->str);
      return;
    }

  // Compute authenticated encryption of input
  guchar outputmacdata[CMAC_LENGTH];

  // Empty string received? Parsing error?
  if(args->argv[0]->len==0)
    {
      msg_error("[SLOG] ERROR: String of length 0 received");
      GString *errorString = g_string_new("[SLOG] ERROR: String of length 0 received");
      sLogEntry(state->numberOfLogEntries, errorString, state->key, state->bigMAC, result, outputmacdata);
      g_string_free(errorString, TRUE);
    }
  else
    {
      sLogEntry(state->numberOfLogEntries, args->argv[0], state->key, state->bigMAC, result, outputmacdata);
    }

  memcpy(state->bigMAC, outputmacdata, CMAC_LENGTH);
  evolveKey(state->key);
  state->numberOfLogEntries++;

  int res = writeKey((char *)state->key, state->numberOfLogEntries, state->keypath);

  if (res == 0)
    {
      msg_error("[SLOG] ERROR: Cannot write key to file");
      return;
    }

  res = writeBigMAC(state->macpath, (char *)state->bigMAC);

  if (res == 0)
    {
      msg_error("[SLOG] ERROR: Unable to write aggregated MAC", evt_tag_str("File", state->macpath));
      return;
    }
}

// Secure logging free state function
void
tf_slog_func_free_state(gpointer s)
{
  TFSlogState *state = (TFSlogState *) s;

  free(state->keypath);
  free(state->macpath);

  tf_simple_func_free_state((gpointer)&state->super);
}


/*
 * Declare the template function for secure logging
 */
TEMPLATE_FUNCTION(TFSlogState, tf_slog, tf_slog_prepare, tf_simple_func_eval, tf_slog_call, tf_slog_func_free_state,
                  NULL);

static Plugin secure_logging_plugins[] =
{
  TEMPLATE_FUNCTION_PLUGIN(tf_slog, "slog"),
};

gboolean
secure_logging_module_init(PluginContext *context, CfgArgs *args)
{
  plugin_register(context, secure_logging_plugins, G_N_ELEMENTS(secure_logging_plugins));
  return TRUE;
}

const ModuleInfo module_info =
{
  .canonical_name = "secure_logging",
  .version = SYSLOG_NG_VERSION,
  .description = "The secure logging module provides template functions enabling forward integrity and confidentiality of logs.",
  .core_revision = SYSLOG_NG_SOURCE_REVISION,
  .plugins = secure_logging_plugins,
  .plugins_len = G_N_ELEMENTS(secure_logging_plugins),
};






