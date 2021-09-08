/*
 * Copyright (c) 2010-2016 Balabit
 * Copyright (c) 2010-2014 Gergely Nagy <algernon@balabit.hu>
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
 *
 */


#include "afmongodb.h"
#include "afmongodb-parser.h"
#include "afmongodb-worker.h"
#include "apphook.h"
#include "messages.h"
#include "stats/stats-registry.h"
#include "plugin.h"
#include "plugin-types.h"
#include "syslog-ng.h"

#include <time.h>

#include "afmongodb-private.h"

#define DEFAULT_URI \
      "mongodb://127.0.0.1:27017/syslog"\
      "?wtimeoutMS=60000&socketTimeoutMS=60000&connectTimeoutMS=60000"

#define DEFAULT_SERVER_SELECTION_TIMEOUT 3000

/*
 * Configuration
 */

LogTemplateOptions *
afmongodb_dd_get_template_options(LogDriver *s)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)s;

  return &self->template_options;
}

void
afmongodb_dd_set_uri(LogDriver *d, const gchar *uri)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;

  if (self->uri_str)
    g_string_assign(self->uri_str, uri);
  else
    self->uri_str = g_string_new(uri);
}

void
afmongodb_dd_set_collection(LogDriver *d, LogTemplate *collection_template)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;

  log_template_unref(self->collection_template);
  self->collection_template = collection_template;

  self->collection_is_literal_string = log_template_is_literal_string(self->collection_template);
}

void
afmongodb_dd_set_value_pairs(LogDriver *d, ValuePairs *vp)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;

  value_pairs_unref(self->vp);
  self->vp = vp;
}

/*
 * Utilities
 */

static gchar *
_format_instance_id(const LogThreadedDestDriver *d, const gchar *format)
{
  const MongoDBDestDriver *self = (const MongoDBDestDriver *)d;
  static gchar args[1024];
  static gchar id[1024];

  if (((LogPipe *)d)->persist_name)
    {
      g_snprintf(args, sizeof(args), "%s", ((LogPipe *)d)->persist_name);
    }
  else
    {
      const mongoc_host_list_t *hosts = mongoc_uri_get_hosts(self->uri_obj);
      const gchar *first_host = "";
      if (hosts)
        {
          if (hosts->family == AF_UNIX)
            first_host = hosts->host;
          else
            first_host = hosts->host_and_port;
        }

      const gchar *db = self->const_db ? self->const_db : "";

      const gchar *replica_set = mongoc_uri_get_replica_set(self->uri_obj);
      if (!replica_set)
        replica_set = "";

      const gchar *coll = self->collection_template->template ? self->collection_template->template : "";

      g_snprintf(args, sizeof(args), "%s,%s,%s,%s", first_host, db, replica_set, coll);
    }
  g_snprintf(id, sizeof(id), format, args);
  return id;
}

static const gchar *
_format_stats_instance(LogThreadedDestDriver *d)
{
  return _format_instance_id(d, "mongodb,%s");
}

static const gchar *
_format_persist_name(const LogPipe *s)
{
  const LogThreadedDestDriver *self = (const LogThreadedDestDriver *)s;

  return s->persist_name ? _format_instance_id(self, "afmongodb.%s")
         : _format_instance_id(self, "afmongodb(%s)");
}

static inline void
_uri_set_default_server_selection_timeout(mongoc_uri_t *uri_obj)
{
#if SYSLOG_NG_HAVE_DECL_MONGOC_URI_SET_OPTION_AS_INT32 && SYSLOG_NG_HAVE_DECL_MONGOC_URI_SERVERSELECTIONTIMEOUTMS
  gint32 server_selection_timeout = mongoc_uri_get_option_as_int32(uri_obj, MONGOC_URI_SERVERSELECTIONTIMEOUTMS,
                                    DEFAULT_SERVER_SELECTION_TIMEOUT);
  mongoc_uri_set_option_as_int32(uri_obj, MONGOC_URI_SERVERSELECTIONTIMEOUTMS, server_selection_timeout);
#endif
}

gboolean
afmongodb_dd_private_uri_init(LogDriver *d)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;

  if (!self->uri_str)
    self->uri_str = g_string_new(DEFAULT_URI);

  self->uri_obj = mongoc_uri_new(self->uri_str->str);
  if (!self->uri_obj)
    {
      msg_error("Error parsing MongoDB URI",
                evt_tag_str("uri", self->uri_str->str),
                evt_tag_str("driver", self->super.super.super.id));
      return FALSE;
    }

  _uri_set_default_server_selection_timeout(self->uri_obj);

  self->const_db = mongoc_uri_get_database(self->uri_obj);
  if (!self->const_db || !strlen(self->const_db))
    {
      msg_error("Missing DB name from MongoDB URI",
                evt_tag_str("uri", self->uri_str->str),
                evt_tag_str("driver", self->super.super.super.id));
      return FALSE;
    }

  msg_verbose("Initializing MongoDB destination",
              evt_tag_str("uri", self->uri_str->str),
              evt_tag_str("db", self->const_db),
              evt_tag_str("collection", self->collection_template->template),
              evt_tag_str("driver", self->super.super.super.id));

  return TRUE;
}

gboolean
afmongodb_dd_client_pool_init(MongoDBDestDriver *self)
{
  self->pool = mongoc_client_pool_new(self->uri_obj);
  if (!self->pool)
    return FALSE;

  mongoc_client_pool_max_size(self->pool, self->super.num_workers);

  return TRUE;
}

/*
 * Main thread
 */

static void
_init_value_pairs_dot_to_underscore_transformation(MongoDBDestDriver *self)
{
  ValuePairsTransformSet *vpts;

  /* Always replace a leading dot with an underscore. */
  vpts = value_pairs_transform_set_new(".*");
  value_pairs_transform_set_add_func(vpts, value_pairs_new_transform_replace_prefix(".", "_"));
  value_pairs_add_transforms(self->vp, vpts);
}

static gboolean
_init(LogPipe *s)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  log_template_options_init(&self->template_options, cfg);

  _init_value_pairs_dot_to_underscore_transformation(self);

  if (!afmongodb_dd_private_uri_init(&self->super.super.super))
    return FALSE;

  if (!afmongodb_dd_client_pool_init(self))
    return FALSE;

  if (!log_threaded_dest_driver_init_method(s))
    return FALSE;

  return TRUE;
}

static gboolean
_deinit(LogPipe *s)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)s;

  if(!log_threaded_dest_driver_deinit_method(s))
    return FALSE;

  if (self->pool)
    mongoc_client_pool_destroy(self->pool);

  if (self->uri_obj)
    mongoc_uri_destroy(self->uri_obj);

  return TRUE;
}

static void
_free(LogPipe *d)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;

  log_template_options_destroy(&self->template_options);

  if (self->uri_str)
    {
      g_string_free(self->uri_str, TRUE);
      self->uri_str = NULL;
    }
  log_template_unref(self->collection_template);

  value_pairs_unref(self->vp);

  log_threaded_dest_driver_free(d);
}

/*
 * Plugin glue.
 */

static void
afmongodb_global_init(void)
{
  mongoc_init();
}

static void
afmongodb_global_deinit(void)
{
  mongoc_cleanup();
}

static void
afmongodb_register_global_initializers(void)
{
  static gboolean initialized = FALSE;

  if (!initialized)
    {
      register_application_hook(AH_STARTUP, (ApplicationHookFunc) afmongodb_global_init, NULL, AHM_RUN_ONCE);
      register_application_hook(AH_SHUTDOWN, (ApplicationHookFunc) afmongodb_global_deinit, NULL, AHM_RUN_ONCE);
      initialized = TRUE;
    }
}

LogDriver *
afmongodb_dd_new(GlobalConfig *cfg)
{
  MongoDBDestDriver *self = g_new0(MongoDBDestDriver, 1);

  afmongodb_register_global_initializers();

  log_threaded_dest_driver_init_instance(&self->super, cfg);

  self->super.super.super.super.init = _init;
  self->super.super.super.super.deinit = _deinit;
  self->super.super.super.super.free_fn = _free;
  self->super.super.super.super.generate_persist_name = _format_persist_name;

  self->super.format_stats_instance = _format_stats_instance;
  self->super.stats_source = stats_register_type("mongodb");
  self->super.worker.construct = afmongodb_dw_new;

  LogTemplate *template = log_template_new(cfg, NULL);
  log_template_compile_literal_string(template, "messages");
  afmongodb_dd_set_collection(&self->super.super.super, template);

  log_template_options_defaults(&self->template_options);
  afmongodb_dd_set_value_pairs(&self->super.super.super, value_pairs_new_default(cfg));

  return &self->super.super.super;
}

extern CfgParser afmongodb_dd_parser;

static Plugin afmongodb_plugin =
{
  .type = LL_CONTEXT_DESTINATION,
  .name = "mongodb",
  .parser = &afmongodb_parser,
};

gboolean
afmongodb_module_init(PluginContext *context, CfgArgs *args)
{
  plugin_register(context, &afmongodb_plugin, 1);
  return TRUE;
}

const ModuleInfo module_info =
{
  .canonical_name = "afmongodb",
  .version = SYSLOG_NG_VERSION,
  .description = "The afmongodb module provides MongoDB destination support for syslog-ng.",
  .core_revision = SYSLOG_NG_SOURCE_REVISION,
  .plugins = &afmongodb_plugin,
  .plugins_len = 1,
};
