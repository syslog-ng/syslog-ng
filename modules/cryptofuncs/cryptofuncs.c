/*
 * Copyright (c) 2012 Balabit
 * Copyright (c) 2012 Gergely Nagy <algernon@balabit.hu>
 * Copyright (c) 2012 Peter Gyongyosi <gyp@balabit.hu>
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


static Plugin cryptofuncs_plugins[] =
{
  TEMPLATE_FUNCTION_PLUGIN(tf_uuid, "uuid"),
  TEMPLATE_FUNCTION_PLUGIN(tf_hash, "hash"),
  TEMPLATE_FUNCTION_PLUGIN(tf_hash, "sha1"),
  TEMPLATE_FUNCTION_PLUGIN(tf_hash, "sha256"),
  TEMPLATE_FUNCTION_PLUGIN(tf_hash, "sha512"),
  TEMPLATE_FUNCTION_PLUGIN(tf_hash, "md4"),
  TEMPLATE_FUNCTION_PLUGIN(tf_hash, "md5"),
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
