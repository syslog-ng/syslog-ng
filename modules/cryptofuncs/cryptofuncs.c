/*
 * Copyright (c) 2012 Balabit
 * Copyright (c) 2012 Gergely Nagy <algernon@balabit.hu>
 * Copyright (c) 2012 Peter Gyongyosi <gyp@balabit.hu>
 * Copyright (c) 2019 Airbus Commercial Aircraft <secure-logging@airbus.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
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
#include <endian.h>
#include <sys/mman.h>

// Secure logging declarations
#include "slog.h"

static void
tf_uuid(LogMessage *msg, gint argc, GString *argv[], GString *result)
{
  char uuid_str[37];

  uuid_gen_random(uuid_str, sizeof(uuid_str));
  g_string_append (result, uuid_str);
}

TEMPLATE_FUNCTION_SIMPLE(tf_uuid);

/*
 * $($hash_method [opts] $arg1 $arg2 $arg3...)
 *
 * Returns the hash of the argument, using the specified hashing
 * method. Note that the values of the arguments are simply concatenated
 * when calculating the hash.
 *
 * Options:
 *      --length N, -l N    Truncate the hash to the first N characters
 */
typedef struct _TFHashState
{
  TFSimpleFuncState super;
  gint length;
  const EVP_MD *md;
} TFHashState;

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


static gboolean
tf_hash_prepare(LogTemplateFunction *self, gpointer s, LogTemplate *parent, gint argc, gchar *argv[], GError **error)
{
  TFHashState *state = (TFHashState *) s;
  GOptionContext *ctx;
  gint length = 0;
  const EVP_MD *md;
  GOptionEntry hash_options[] =
  {
    { "length", 'l', 0, G_OPTION_ARG_INT, &length, NULL, NULL },
    { NULL }
  };

  ctx = g_option_context_new("hash");
  g_option_context_add_main_entries(ctx, hash_options, NULL);

  if (!g_option_context_parse(ctx, &argc, &argv, error))
    {
      g_option_context_free(ctx);
      return FALSE;
    }
  g_option_context_free(ctx);

  if (argc < 2)
    {
      g_set_error(error, LOG_TEMPLATE_ERROR, LOG_TEMPLATE_ERROR_COMPILE,
                  "$(hash) parsing failed, invalid number of arguments");
      return FALSE;
    }

  if (!tf_simple_func_prepare(self, state, parent, argc, argv, error))
    {
      return FALSE;
    }
  state->length = length;
  md = EVP_get_digestbyname(strcmp(argv[0], "hash") == 0 ? "sha256" : argv[0]);
  if (!md)
    {
      g_set_error(error, LOG_TEMPLATE_ERROR, LOG_TEMPLATE_ERROR_COMPILE, "$(hash) parsing failed, unknown digest type");
      return FALSE;
    }
  state->md = md;
  gint md_size = EVP_MD_size(md);
  if ((state->length == 0) || (state->length > md_size * 2))
    state->length = md_size * 2;
  return TRUE;
}

static guint
_hash(const EVP_MD *md, GString *const *argv, gint argc, guchar *hash, guint hash_size)
{
  gint i;
  guint md_len;
  DECLARE_EVP_MD_CTX(mdctx);
  EVP_MD_CTX_init(mdctx);
  EVP_DigestInit_ex(mdctx, md, NULL);

  for (i = 0; i < argc; i++)
    {
      EVP_DigestUpdate(mdctx, argv[i]->str, argv[i]->len);
    }

  EVP_DigestFinal_ex(mdctx, hash, &md_len);
  EVP_MD_CTX_cleanup(mdctx);
  EVP_MD_CTX_destroy(mdctx);

  return md_len;
}

static void
tf_hash_call(LogTemplateFunction *self, gpointer s, const LogTemplateInvokeArgs *args, GString *result)
{
  TFHashState *state = (TFHashState *) s;
  gint argc;
  guchar hash[EVP_MAX_MD_SIZE];
  gchar hash_str[EVP_MAX_MD_SIZE * 2 + 1];
  guint md_len;

  argc = state->super.argc;
  md_len = _hash(state->md, args->argv, argc, hash, sizeof(hash));
  // we fetch the entire hash in a hex format otherwise we cannot truncate at
  // odd character numbers
  format_hex_string(hash, md_len, hash_str, sizeof(hash_str));
  if (state->length == 0)
    {
      g_string_append(result, hash_str);
    }
  else
    {
      g_string_append_len(result, hash_str, MIN(sizeof(hash_str), state->length));
    }
}

TEMPLATE_FUNCTION(TFHashState, tf_hash, tf_hash_prepare, tf_simple_func_eval, tf_hash_call, tf_simple_func_free_state,
                  NULL);

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
    { "keyfile", 'k', 0, G_OPTION_ARG_FILENAME, &keypathbuffer, NULL, NULL },
    { "macfile", 'm', 0, G_OPTION_ARG_FILENAME, &macpathbuffer, NULL, NULL },
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
      g_string_assign (result, args->argv[0]->str);
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

static Plugin cryptofuncs_plugins[] =
{
  TEMPLATE_FUNCTION_PLUGIN(tf_uuid, "uuid"),
  TEMPLATE_FUNCTION_PLUGIN(tf_hash, "hash"),
  TEMPLATE_FUNCTION_PLUGIN(tf_hash, "sha1"),
  TEMPLATE_FUNCTION_PLUGIN(tf_hash, "sha256"),
  TEMPLATE_FUNCTION_PLUGIN(tf_hash, "sha512"),
  TEMPLATE_FUNCTION_PLUGIN(tf_hash, "md4"),
  TEMPLATE_FUNCTION_PLUGIN(tf_hash, "md5"),
  TEMPLATE_FUNCTION_PLUGIN(tf_slog, "slog"),
};

gboolean
cryptofuncs_module_init(PluginContext *context, CfgArgs *args)
{
  plugin_register(context, cryptofuncs_plugins, G_N_ELEMENTS(cryptofuncs_plugins));
  return TRUE;
}

const ModuleInfo module_info =
{
  .canonical_name = "cryptofuncs",
  .version = SYSLOG_NG_VERSION,
  .description = "The cryptofuncs module provides cryptographic template functions.",
  .core_revision = SYSLOG_NG_SOURCE_REVISION,
  .plugins = cryptofuncs_plugins,
  .plugins_len = G_N_ELEMENTS(cryptofuncs_plugins),
};






