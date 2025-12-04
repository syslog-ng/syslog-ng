/*
 * Copyright (c) 2019-2025 Airbus Commercial Aircraft
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
  guchar aggMAC[CMAC_LENGTH];
} TFSlogState;



/*
 * Initialize the secure logging template
 */
static gboolean
tf_slog_prepare(LogTemplateFunction *self, gpointer s, LogTemplate *parent, gint argc, gchar *argv[], GError **error)
{
  // Get key filename and store in internal state
  // generate initial BigMAC file
  TFSlogState *state = (TFSlogState *)s;

  gchar *keypathbuffer = NULL;
  gchar *macpathbuffer = NULL;
  GOptionContext *ctx;
  GOptionGroup *grp;
  if ((mlock(state->key, KEY_LENGTH)!=0)||(mlock(state->aggMAC, CMAC_LENGTH)!=0))
    {
      msg_warning("[SLOG] WARNING: Unable to acquire memory lock");
    }

  state->badKey = FALSE;

  SLogOptions options[] =
  {
    { "key-file", 'k', "Name of the host key file", "FILE", NULL },
    { "mac-file", 'm', "Name of the MAC file", "FILE", NULL },
    { NULL }
  };

  GOptionEntry slog_options[] =
  {
    { options[0].longname, options[0].shortname, 0, G_OPTION_ARG_CALLBACK, &validFileNameArg, options[0].description, options[0].type },
    { options[1].longname, options[1].shortname, 0, G_OPTION_ARG_FILENAME, &macpathbuffer, options[1].description, options[1].type },
    { NULL }
  };

  ctx = g_option_context_new("- Secure logging template");
  grp = g_option_group_new("Basic options", "Basic template options", "basic", &options, NULL);

  g_option_group_add_entries(grp, slog_options);
  g_option_context_set_main_group(ctx, grp);

  GError *argError = NULL;

  if (!g_option_context_parse(ctx, &argc, &argv, &argError))
    {
      if (argError != NULL)
        {
          g_propagate_error (error, argError);
        }

      g_option_context_free(ctx);
      return FALSE;
    }

  if (argc < 2)
    {
      state->badKey = TRUE;
      g_set_error(error, LOG_TEMPLATE_ERROR, LOG_TEMPLATE_ERROR_COMPILE,
                  "[SLOG] ERROR: Template parsing failed. Invalid number of arguments. Usage: $(slog --key-file FILE --mac-file FILE $RAWMSG)\\n");
      g_option_context_free(ctx);
      return FALSE;
    }

  keypathbuffer = options[0].arg;

  if(keypathbuffer == NULL)
    {
      state->badKey = TRUE;

      g_set_error(error, LOG_TEMPLATE_ERROR, LOG_TEMPLATE_ERROR_COMPILE,
                  "[SLOG] ERROR: Template parsing failed. Invalid or missing key file");
      g_option_context_free(ctx);
      return FALSE;
    }

  if(macpathbuffer == NULL)
    {
      state->badKey = TRUE;

      g_set_error(error, LOG_TEMPLATE_ERROR, LOG_TEMPLATE_ERROR_COMPILE,
                  "[SLOG] ERROR: Template parsing failed. Invalid or missing MAC file");
      g_option_context_free(ctx);
      return FALSE;
    }

  if (!tf_simple_func_prepare(self, state, parent, argc, argv, error))
    {
      state->badKey = TRUE;
      g_option_context_free(ctx);
      return FALSE;
    }

  state->numberOfLogEntries = 0;
  state->keypath = keypathbuffer;
  state->macpath = macpathbuffer;

  // Done with argument parsing
  g_option_context_free(ctx);

  //------------
  // Provide an initial MAC.
  // This first MAC is also stored as a file which path is given by
  // state->macpath specified in the template. It is referenced
  // and updated as aggregated MAC later. When it does not exist, it will
  // be created here.
  // It is the user’s responsibility to clear an existing the log file,
  // when no valid MAC files or keys are available.
  //
  // Start Conditions
  // Case-1: No MAC file is available
  //         Both MAC and MAC0 are created.
  //         Normal use case when nothing has been logged so far.
  //         Error when key has been used (counter>0) or MAC0 is available.
  //
  // Case-2: There is an invalid MAC file found.
  //
  // Case-3: There is a valid MAC file but no MAC0
  //
  // Case-4: There is a valid MAC file and an invalid MAC0
  //
  // Case-5: Expected files are available.
  //         Normal use case when syslog-ng is restarted.
  //         Both MAC and MAC0 and keys are available.
  //         The log file already contains entries and previous
  //         process used above files.
  //
  //  NOTES:
  //  There is no way to check whether MAC0, MAC, Keys
  //  and a possible existing log file like messages.slog are
  //  belong to each other.
  //  When they are manipulated and manually provided from
  //  different processes, verification will fail later.
  //  When MAC files are re-created, and there is already a
  //  log file providing log entries, it can not be verified
  //  then!

  gboolean is_good_start = TRUE;
  unsigned char key[KEY_LENGTH];
  unsigned char mac[CMAC_LENGTH];
  char pathMac0[PATH_MAX]; //-- full path of MAC0 file mac0.dat
  memset(key, 0, KEY_LENGTH);
  memset(mac, 0, CMAC_LENGTH);

  if (FALSE == get_path_mac0(state->macpath, pathMac0, PATH_MAX))
    {
      //-- ERROR, never ever
      msg_warning("[SLOG] ERROR: failed to get path of MAC to create MAC0!", evt_tag_str("File", state->macpath));
    }

  if (!g_file_test(state->macpath, G_FILE_TEST_IS_REGULAR))
    {
      if (g_file_test(pathMac0, G_FILE_TEST_IS_REGULAR))
        {
          //-- No MAC but there is a MAC0. Invalid state. Report a problem.
          msg_warning("[SLOG] ERROR: MAC0 found without MAC!", evt_tag_str("File", pathMac0));
          is_good_start = FALSE;
        }
      else
        {
          //-- Neither MAC nor MAC0 found. Check key usage count.
          guint64 key_counter = 42;
          (void) readKey(state->key, &key_counter, state->keypath);
          if (key_counter > 0)
            {
              msg_warning("[SLOG] ERROR: Number of log entries is greater than 0 but no MAC files provided!", evt_tag_long("Count",
                          key_counter));
              is_good_start = FALSE;
            }
        }
      /* Case-1 */
      //-- No MAC file available. Normal case when first run.
      //-- create initial aggregated MAC mac0
      create_initial_mac0(key, mac);
      //-- write aggregated MAC and MAC0 file
      (void) writeAggregatedMAC(state->macpath, mac);
      (void) writeAggregatedMAC(pathMac0, mac);
      msg_info("[SLOG] INFO: MAC0 and MAC have been created!", evt_tag_str("File", state->macpath));
    }
  else
    {
      if(!readAggregatedMAC(state->macpath, state->aggMAC))
        {
          /* Case-2 */
          //-- invalid MAC file
          //-- create initial aggregated MAC mac0
          create_initial_mac0(key, mac);
          //-- write aggregated MAC and MAC0 file
          //   overwrite both MAC and MAC0 file
          (void) writeAggregatedMAC(state->macpath, mac);
          (void) writeAggregatedMAC(pathMac0, mac);
          msg_warning("[SLOG] ERROR: Checksum of MAC is invalid! MAC0 and MAC have been re-created!", evt_tag_str("File",
                      pathMac0));
          is_good_start = FALSE;
        }
      else
        {
          if (!g_file_test(pathMac0, G_FILE_TEST_IS_REGULAR))
            {
              /* Case-3 */
              //-- valid MAC file available
              //-- no MAC0 file available, use copy of valid MAC file
              (void) writeAggregatedMAC(pathMac0, state->aggMAC);
              msg_warning("[SLOG] ERROR: No MAC0 found! Dummy MAC0 is provided!", evt_tag_str("File", pathMac0));
              is_good_start = FALSE;
            }
          else
            {
              if(!readAggregatedMAC(pathMac0, mac))
                {
                  /* Case-4 */
                  //-- valid MAC file available
                  //-- invalid MAC0 file found, overwrite it, use copy of valid MAC file
                  (void) writeAggregatedMAC(pathMac0, state->aggMAC);
                  msg_warning("[SLOG] ERROR: Checksum of MAC0 invalid! Dummy MAC0 is provided!", evt_tag_str("File", pathMac0));
                  is_good_start = FALSE;
                }
            }
        }
    }

  /* Case-5 */
  // All files are expected to be available
  // Check if current process was able to write files in case it was necessary.

  //-- MAC0 ---
  if (g_file_test(pathMac0, G_FILE_TEST_IS_REGULAR))
    {
      // Read MAC0
      if (!readAggregatedMAC(pathMac0, mac))
        {
          is_good_start = FALSE;
          msg_warning("[SLOG] ERROR: Unable to read MAC0", evt_tag_str("File", pathMac0));
        }
    }
  else
    {
      is_good_start = FALSE;
    }

  //-- MAC ---
  if (g_file_test(state->macpath, G_FILE_TEST_IS_REGULAR))
    {
      // Read aggregated MAC
      if(!readAggregatedMAC(state->macpath, state->aggMAC))
        {
          is_good_start = FALSE;
          msg_warning("[SLOG] ERROR: Unable to read aggregated MAC", evt_tag_str("File", state->macpath));
          if (state->numberOfLogEntries > 0)
            {
              msg_warning("[SLOG] ERROR: Aggregated MAC not found or invalid", evt_tag_str("File", state->macpath));
            }
        }
    }
  else
    {
      is_good_start = FALSE;
    }

  if (TRUE == is_good_start)
    {
      msg_info("[SLOG] INFO: Template with key and MAC file successfully initialized.");
    }
  else
    {
      msg_warning("[SLOG] ERROR: Initialization not successful.");
      msg_warning("[SLOG] WARNING: It is the users reponsiblility to delete MAC files and the log file and provide a new host.key in order to restart in good condition.");
      msg_warning("[SLOG] WARNING: Current setup does not allow full verification!");
    }

  //------------

  // Read key
  if (!readKey(state->key, (guint64 *)&(state->numberOfLogEntries), state->keypath))
    {
      state->badKey = TRUE;
      msg_warning("[SLOG] WARNING: Template parsing failed, key file not found or invalid. Reverting to clear text logging.");
      return TRUE;
    }

  msg_debug("[SLOG] INFO: Key successfully loaded");

  return TRUE;
}

/*
 * Create a new encrypted log entry
 */
static void
tf_slog_call(LogTemplateFunction *self, gpointer s, const LogTemplateInvokeArgs *args, GString *result,
             LogMessageValueType *type)
{
  TFSlogState *state = (TFSlogState *) s;
  *type = LM_VT_STRING;
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
      sLogEntry(state->numberOfLogEntries, errorString, state->key, state->aggMAC, result, outputmacdata,
                G_N_ELEMENTS(outputmacdata));
      g_string_free(errorString, TRUE);
    }
  else
    {
      sLogEntry(state->numberOfLogEntries, args->argv[0], state->key, state->aggMAC, result, outputmacdata,
                G_N_ELEMENTS(outputmacdata));
    }

  memcpy(state->aggMAC, outputmacdata, CMAC_LENGTH);
  evolveKey(state->key);
  state->numberOfLogEntries++;

  int res = writeKey(state->key, state->numberOfLogEntries, state->keypath);

  if (res == 0)
    {
      msg_error("[SLOG] ERROR: Cannot write key to file");
      return;
    }

  res = writeAggregatedMAC(state->macpath, state->aggMAC);

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
